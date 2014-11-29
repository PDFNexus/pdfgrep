bin_PROGRAMS = pdfgrep

pdfgrep_SOURCES = pdfgrep.cc output.cc output.h exclude.cc exclude.h queue.cc queue.h
pdfgrep_LDADD = $(poppler_cpp_LIBS) $(unac_LIBS) -lpthread
AM_CPPFLAGS = $(poppler_cpp_CFLAGS) $(unac_CFLAGS)

EXTRA_DIST=autogen.sh

SUBDIRS = completion doc
