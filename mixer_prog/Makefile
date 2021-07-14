# $FreeBSD$

PROG=		mixer_prog
BINDIR=         /usr/sbin
SRCS=		${PROG}.c
MAN=		${PROG}.8
CFLAGS+=	-pedantic -Wall -Wunused
LDFLAGS+=	-lmixer -lm

.include <bsd.prog.mk>
