ipsec_PROGRAMS = pki

pki_SOURCES = pki.c pki.h command.c command.h \
	commands/gen.c \
	commands/issue.c \
	commands/keyid.c \
	commands/pub.c \
	commands/req.c \
	commands/self.c \
	commands/print.c \
	commands/signcrl.c \
	commands/verify.c

pki_LDADD = $(top_builddir)/src/libstrongswan/libstrongswan.la
pki.o :	$(top_builddir)/config.status

INCLUDES = -I$(top_srcdir)/src/libstrongswan
AM_CFLAGS = \
	-DPLUGINS=\""${pki_plugins}\""
