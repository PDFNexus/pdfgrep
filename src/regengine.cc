/***************************************************************************
 *   Copyright (C) 2015-2023 by Hans-Peter Deifel                          *
 *   hpd@hpdeifel.de                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 *   Boston, MA 02110-1301 USA.                                            *
 ***************************************************************************/

#include "regengine.h"

#include <regex.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cctype>
#include <algorithm>

#include "output.h"
#include "pdfgrep.h"

bool PatternList::exec(const std::string& str, size_t offset, struct match& m) const {
	struct match m_copy = m;

	for (auto& r : patterns) {
		if (r->exec(str, offset, m_copy)) {
			m.start = m_copy.start;
			m.end = m_copy.end;
			return true;
		}
	}
	return false;
}

void PatternList::add_pattern(std::unique_ptr<Regengine> pattern) {
	patterns.push_back(move(pattern));
}

// Shamelessly stolen from ripgrep!
//
// This implementation is a compromise. It successfully ignores escape sequences
// such as '\S', but fails to ignore things like '\p{Ll}'.
static bool regex_has_uppercase_literals(const std::string& pattern) {
	bool escaped = false;

	// FIXME This iterates trough bytes in an UTF-8 string, so 'isupper'
	// will work only for the most simple cases.
	//
	// C++ doesn't provide sensible unicode primitives for such tasks, so we
	// should finally start to use ICU.
	for (char c : pattern) {
		if (escaped) {
			escaped = false;
			continue;
		}
		if (isupper(c))
			return true;
		if (c == '\\') {
			escaped = true;
		}
	}
	return false;
}

// regex(3)

PosixRegex::PosixRegex(const std::string& pattern, CaseSensitivity case_folding) {
	bool case_insensitive =
	  case_folding == CaseSensitivity::INSENSITIVE
	  || (case_folding == CaseSensitivity::SMART && !has_uppercase_literals(pattern));

	int regex_flags = REG_EXTENDED | (case_insensitive ? REG_ICASE : 0);

	// The regcomp/regexec implementation of OpenBSD doesn't like empty
	// patterns. Thus we just replace empty patterns by "()", which does
	// have the same semantics.
	const char* c_str_pattern;
	if (pattern.empty()) {
		c_str_pattern = "()";
	} else {
		c_str_pattern = pattern.c_str();
	}

	int ret = regcomp(&this->regex, c_str_pattern, regex_flags);
	if (ret != 0) {
		char err_msg[256];
		regerror(ret, &this->regex, err_msg, 256);
		err() << err_msg << std::endl;
		exit(EXIT_ERROR);
	}
}

bool PosixRegex::exec(const std::string& str, size_t offset, struct match& m) const {
	regmatch_t match[] = { { 0, 0 } };
	const int nmatch = 1;

	// If we aren't at the beginning of the page, ^ should not match.
	int flags = offset == 0 ? 0 : REG_NOTBOL;

	int ret = regexec(&this->regex, &str[offset], nmatch, match, flags);

	if (ret != 0) {
		return false;
	}

	m.start = offset + match[0].rm_so;
	m.end = offset + match[0].rm_eo;

	return true;
}

PosixRegex::~PosixRegex() {
	regfree(&this->regex);
}

bool PosixRegex::has_uppercase_literals(const std::string& pattern) {
	return regex_has_uppercase_literals(pattern);
}

// pcre(3)

#ifdef HAVE_LIBPCRE

PCRERegex::PCRERegex(const std::string& pattern, CaseSensitivity case_folding) {
	const char* pcre_err;
	int pcre_err_ofs;

	const bool case_insensitive =
	  case_folding == CaseSensitivity::INSENSITIVE
	  || (case_folding == CaseSensitivity::SMART && !has_uppercase_literals(pattern));

	const int pcre_options = PCRE_UTF8 | (case_insensitive ? PCRE_CASELESS : 0);

	this->regex =
	  pcre_compile(pattern.c_str(), pcre_options, &pcre_err, &pcre_err_ofs, nullptr);

	if (this->regex == nullptr) {
		err() << pattern << std::endl;
		err() << std::setw(pcre_err_ofs + 1) << "^" << std::endl;
		err() << "Error compiling PCRE pattern: " << pcre_err << std::endl;
		exit(EXIT_ERROR);
	}
}

PCRERegex::~PCRERegex() {
	pcre_free(this->regex);
}

bool PCRERegex::exec(const std::string& str, size_t offset, struct match& m) const {
	const size_t len = str.size();
	int ov[3];

	const int ret = pcre_exec(this->regex, nullptr, str.c_str(), len, offset, 0, ov, 3);

	// TODO: Print human readable error
	if (ret < 0) {
		return false;
	}

	m.start = ov[0];
	m.end = ov[1];

	return true;
}

bool PCRERegex::has_uppercase_literals(const std::string& pattern) {
	return regex_has_uppercase_literals(pattern);
}

#endif // HAVE_LIBPCRE

FixedString::FixedString(const std::string& pattern, CaseSensitivity case_folding)
  : case_folding(case_folding) {
	std::istringstream str{ pattern };
	std::string line;

	if (pattern.empty()) {
		// special case for the empty pattern. In this case we _do_ want
		// matches, but getline returns false leaving our patterns array
		// empty. Thus we add the whole pattern explicitly.
		patterns.push_back(pattern);
		return;
	}

	// split pattern at newlines
	while (getline(str, line)) {
		patterns.push_back(line);
	}
}

bool FixedString::exec(const std::string& str, size_t offset, struct match& m) const {
	// We use C-style strings here, because of strcasestr
	const char* str_begin = &str[offset];

	// FIXME Searching for multiple patterns is very inefficient, because we
	// search the same thing over and over, until it becomes the next match.
	// We should introduce some kind of caching here

	const char* min_result = nullptr;
	const std::string* min_pattern;

	for (const std::string& pattern : patterns) {
		const char* result;
		const bool case_insensitive = this->case_folding == CaseSensitivity::INSENSITIVE
		                              || (this->case_folding == CaseSensitivity::SMART
		                                  && !has_uppercase_literals(pattern));

		if (case_insensitive) {
			result = strcasestr(str_begin, pattern.c_str());
		} else {
			result = strstr(str_begin, pattern.c_str());
		}

		if (result != nullptr) {
			if (min_result == nullptr || result < min_result) {
				min_result = result;
				min_pattern = &pattern;
			}
		}
	}

	if (min_result != nullptr) {
		m.start = offset + (min_result - str_begin);
		m.end = m.start + (*min_pattern).size();
		return true;
	}

	return false;
}

bool FixedString::has_uppercase_literals(const std::string& pattern) {
	// FIXME: As above: This won't work with anything outside of ASCII.
	return std::any_of(pattern.begin(), pattern.end(), [](char c) { return std::isupper(c); });
}
