/*-
 * Copyright (c) 2021 Christos Margiolis <christos@FreeBSD.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mixer.h>

#define LEN(x) (sizeof(x) / sizeof(x[0]))

#define CTRL_VOL 0
#define CTRL_MUT 1
#define CTRL_SRC 2

struct ctrl {
	char name[NAME_MAX];
	void (*mod)(struct mixer *, const char *);
	void (*print)(struct mixer *);
	/* TODO: printed flag */
};

static void	printall(struct mixer *, int);
static void	printminfo(struct mixer *, int);
static void	printdev(struct mix_dev *, int);
static void	printrecsrc(struct mixer *, int);
static int	findctrl(const char *);
static void	mod_volume(struct mixer *, const char *);
static void	mod_mute(struct mixer *, const char *);
static void	mod_recsrc(struct mixer *, const char *);
static void	print_volume(struct mixer *);
static void	print_mute(struct mixer *);
static void	print_recsrc(struct mixer *);
static void	usage(void) __dead2;

static const struct ctrl ctrls[] = {
	[CTRL_VOL] = { "volume",	mod_volume,	print_volume },
	[CTRL_MUT] = { "mute",		mod_mute,	print_mute },
	[CTRL_SRC] = { "recsrc",	mod_recsrc,	print_recsrc },
};

static void
printall(struct mixer *m, int oflag)
{
	struct mix_dev *dp;

	printminfo(m, oflag);
	TAILQ_FOREACH(dp, &m->devs, devs) {
		printdev(dp, oflag);
	}
}

static void
printminfo(struct mixer *m, int oflag)
{
	if (oflag)
		return;
	printf("%s: <%s> %s", m->mi.name, m->ci.longname, m->ci.hw_info);
	if (m->f_default)
		printf(" (default)");
	printf("\n");
}

static void
printdev(struct mix_dev *d, int oflag)
{
	if (!oflag) {
		printf("    %-11s= %.2f:%.2f\t%+.2f\t", 
		    d->name, d->lvol, d->rvol, d->pan);
		if (d->f_pbk)
		       printf(" pbk");
		if (d->f_rec)
		       printf(" rec");
		if (d->f_src)
			printf(" src");
		if (d->f_mut)
			printf(" mute");
		printf("\n");
	} else {
		printf("%s.%s=%.2f:%.2f\n", 
		    d->name, ctrls[CTRL_VOL].name, d->lvol, d->rvol);
		if (d->f_src)
			printf("%s.%s=+\n", d->name, ctrls[CTRL_SRC].name);
		/* TODO: add f_mut */
	}
}

static void
printrecsrc(struct mixer *m, int oflag)
{
	struct mix_dev *dp;
	int n = 0;

	if (!m->recmask)
		return;
	printminfo(m, oflag);
	if (!oflag)
		printf("    recording source(s): ");
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (M_ISRECSRC(m, dp->devno)) {
			if (n++)
				printf("%s ", oflag ? " " : ", ");
			printf("%s", dp->name);
		}
	}
	printf("\n");
}

static int
findctrl(const char *ctrl)
{
	int i;

	for (i = 0; i < LEN(ctrls); i++)
		if (strcmp(ctrl, ctrls[i].name) == 0)
			return (i);

	return (-1);
}

static void
mod_volume(struct mixer *m, const char *val)
{
	char lstr[8], rstr[8];
	float l, r, lprev, rprev, lrel, rrel;
	int n;

	n = sscanf(val, "%7[^:]:%7s", lstr, rstr);
	if (n == EOF) {
		warnx("invalid volume value: %s", val);
		return;
	}
	lrel = rrel = 0;
	if (n > 0) {
		if (*lstr == '+' || *lstr == '-')
			lrel = rrel = 1;
		l = strtof(lstr, NULL);
	}
	if (n > 1) {
		if (*rstr == '+' || *rstr == '-')
			rrel = 1;
		r = strtof(rstr, NULL);
	}
	switch (n) {
	case 1:
		r = l; /* FALLTHROUGH */
	case 2:
		if (lrel)
			l += m->dev->lvol;
		if (rrel)
			r += m->dev->rvol;

		if (l < M_VOLMIN)
			l = M_VOLMIN;
		else if (l > M_VOLMAX)
			l = M_VOLMAX;
		if (r < M_VOLMIN)
			r = M_VOLMIN;
		else if (r > M_VOLMAX)
			r = M_VOLMAX;

		lprev = m->dev->lvol;
		rprev = m->dev->rvol;
		if (mixer_setvol(m, l, r) < 0)
			warn("%s.%s=%.2f:%.2f", 
			    m->dev->name, ctrls[CTRL_VOL].name, l, r);
		else
			printf("%s.%s: %.2f:%.2f -> %.2f:%.2f\n",
			   m->dev->name, ctrls[CTRL_VOL].name, 
			   lprev, rprev, l, r);
	}
}

static void
mod_mute(struct mixer *m, const char *val)
{
	int n, opt = -1;

	switch (*val) {
	case '0':
		opt = M_UNMUTE;
		break;
	case '1':
		opt = M_MUTE;
		break;
	case '^':
		opt = M_TOGGLEMUTE;
		break;
	default:
		warnx("%c: no such modifier", *val);
		return;
	}
	n = m->dev->f_mut;
	if (mixer_setmute(m, opt) < 0)
		warn("%s.%s=%c", m->dev->name, ctrls[CTRL_MUT].name, *val);
	else
		printf("%s.%s: %d -> %d\n",
		    m->dev->name, ctrls[CTRL_MUT].name, n, m->dev->f_mut);
}

static void
mod_recsrc(struct mixer *m, const char *val)
{
	int n, opt = -1;

	switch (*val) {
	case '+':
		opt = M_ADDRECDEV;
		break;
	case '-':
		opt = M_REMOVERECDEV;
		break;
	case '=':
		opt = M_SETRECDEV;
		break;
	case '^':
		opt = M_TOGGLERECDEV;
		break;
	default:
		warnx("%c: no such modifier", *val);
		return;
	}
	n = m->dev->f_src;
	if (mixer_modrecsrc(m, opt) < 0)
		warn("%s.%s=%c", m->dev->name, ctrls[CTRL_SRC].name, *val);
	else
		printf("%s.%s: %d -> %d\n", 
		    m->dev->name, ctrls[CTRL_SRC].name, n, m->dev->f_src);
}

static void
print_volume(struct mixer *m)
{
	printf("%s.%s=%.2f:%.2f\n", 
	    m->dev->name, ctrls[CTRL_VOL].name, m->dev->lvol, m->dev->rvol);
}

static void
print_mute(struct mixer *m)
{
	printf("%s.%s=%d\n", m->dev->name, ctrls[CTRL_MUT].name, m->dev->f_mut);
}

static void
print_recsrc(struct mixer *m)
{
	printf("%s.%s=%d\n",
	    m->dev->name, ctrls[CTRL_SRC].name, m->dev->f_rec);
}

static void __dead2
usage(void)
{
	printf("usage: %1$s [-f device] [-d unit] [-os] [command ...]\n"
	    "       %1$s [-d unit] [-os] -a\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct mixer *m;
	char *name = NULL, buf[NAME_MAX];
	char *p, *bufp, *devstr, *ctrlstr, *valstr = NULL;
	int dunit, i, n, pall = 1;
	int aflag = 0, dflag = 0, oflag = 0, sflag = 0;
	char ch;

	while ((ch = getopt(argc, argv, "ad:f:os")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dunit = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE)
				err(1, "strtol");
			dflag = 1;
			break;
		case 'f':
			name = optarg;
			break;
		case 'o':
			oflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Print all mixers and exit. */
	if (aflag) {
		if ((n = mixer_getnmixers()) < 0)
			err(1, "mixer_get_nmixers");
		for (i = 0; i < n; i++) {
			(void)snprintf(buf, sizeof(buf), "/dev/mixer%d", i);
			if ((m = mixer_open(buf)) == NULL)
				err(1, "mixer_open: %s", buf);
			if (sflag)
				printrecsrc(m, oflag);
			else {
				printall(m, oflag);
				if (oflag)
					printf("\n");
			}
			(void)mixer_close(m);
		}
		return (0);
	}

	if ((m = mixer_open(name)) == NULL)
		err(1, "mixer_open: %s", name);

	/* XXX: make it a control? */
	if (dflag) {
		if ((n = mixer_getdunit()) < 0) {
			warn("cannot get default unit");
			goto parse;
		}
		if (mixer_setdunit(m, dunit) < 0) {
			warn("cannot set default unit to: %d", dunit);
			goto parse;
		}
		printf("default_unit: %d -> %d\n", n, dunit);
	}
	if (sflag) {
		printrecsrc(m, oflag);
		(void)mixer_close(m);
		return (0);
	}

parse:
	while (argc > 0) {
		if ((p = bufp = strdup(*argv)) == NULL)
			err(1, "strdup(%s)", *argv);
		/* Split the string into device, control and value. */
		devstr = strsep(&p, ".");
		if ((m->dev = mixer_getdevbyname(m, devstr)) == NULL) {
			warnx("%s: no such device", devstr);
			goto next;
		}
		/* Input: `dev`. */
		if (p == NULL) {
			printdev(m->dev, 1);
			pall = 0;
			goto next;
		}
		ctrlstr = strsep(&p, "=");
		if ((n = findctrl(ctrlstr)) < 0) {
			warnx("%s.%s: no such control", devstr, ctrlstr);
			goto next;
		}
		/* Input: `dev.ctrl`. */
		if (p == NULL) {
			ctrls[n].print(m);
			pall = 0;
			goto next;
		}
		valstr = p;
		/* Input: `dev.ctrl=val`. */
		ctrls[n].mod(m, valstr);
next:
		free(p);
		argc--;
		argv++;
	}

	if (pall)
		printall(m, oflag);
	(void)mixer_close(m);

	return (0);
}
