/***************************************************************************
 *   Copyright (C) 2015-2019 by Hans-Peter Deifel                          *
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

#ifndef REGENGINE_H
#define REGENGINE_H

#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#ifdef HAVE_LIBPCRE
#include <pcre.h>
#endif
#include <vector>
#include <string>
#include <memory>


struct match;
enum class CaseSensitivity;

class Regengine
{
public:
	// writes the match data to m. Returns true on success and false on failure
	virtual bool exec(const std::string &str, size_t offset, struct match &m) const = 0;
	virtual ~Regengine() {}
};

// This matches the union of a set of patterns
//
// It just tries all patterns in turn. This could be more efficient by using
// some engine-specific way of combining patterns, e.g with "|" in the posix
// case.
class PatternList : public Regengine
{
public:
	PatternList() {}
	~PatternList() {}
	bool exec(const std::string &str, size_t offset, struct match &m) const override;
	void add_pattern(std::unique_ptr<Regengine> pattern);
private:
	std::vector<std::unique_ptr<Regengine>> patterns;
};

class PosixRegex : public Regengine
{
public:
	PosixRegex(const std::string &pattern, CaseSensitivity case_folding);
	~PosixRegex();
	bool exec(const std::string &str, size_t offset, struct match &m) const override;
private:
	regex_t regex;
	static bool has_uppercase_literals(const std::string &pattern);
};

#ifdef HAVE_LIBPCRE
class PCRERegex : public Regengine
{
public:
	PCRERegex(const std::string &pattern, CaseSensitivity case_folding);
	~PCRERegex();
	bool exec(const std::string &str, size_t offset, struct match &m) const override;
private:
	pcre *regex;
	static bool has_uppercase_literals(const std::string &pattern);
};
#endif

class FixedString : public Regengine
{
public:
	FixedString(const std::string &pattern, CaseSensitivity case_folding);
	bool exec(const std::string &str, size_t offset, struct match &m) const override;
private:
	std::vector<std::string> patterns;
	CaseSensitivity case_folding;
	static bool has_uppercase_literals(const std::string &pattern);
};

#endif /* REGENGINE_H */

/* Local Variables: */
/* mode: c++ */
/* End: */
