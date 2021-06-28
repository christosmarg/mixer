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

static void usage(void) __dead2;
static void printall(struct mixer *, int);
static void printmixer(struct mixer *);
static void printdev(struct mix_dev *, int);
static void printrecsrc(struct mixer *, int);

static void __dead2
usage(void)
{
	printf("usage: %1$s [-f device] [-d unit] [-o] [dev [+|-][lvol[:[+|-]rvol]] ...\n"
	    "       %1$s [-f device] [-d unit] [-o] -s ...\n"
	    "       %1$s [-f device] [-d unit] [-o] {^|+|-|=}rec rdev ...\n"
	    "       %1$s [-o] -a\n",
	    getprogname());
	exit(1);
}

static void
printall(struct mixer *m, int oflag)
{
	struct mix_dev *dp;

	if (!oflag)
		printmixer(m);
	TAILQ_FOREACH(dp, &m->devs, devs) {
		printdev(dp, oflag);
	}
}

static void
printmixer(struct mixer *m)
{
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
		printf("\n");
	} else {
		/* FIXME: get rid of the space */
		printf("%s %.2f:%.2f ", d->name, d->lvol, d->rvol);
		if (d->f_src)
			printf("+rec %s ", d->name);
	}
}

static void
printrecsrc(struct mixer *m, int oflag)
{
	struct mix_dev *dp;
	int n = 0;

	if (!m->recmask)
		return;
	if (!oflag) {
		printmixer(m);
		printf("    recording source: ");
	}
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (M_ISRECSRC(m, dp->devno)) {
			if (n)
				printf("%s ", oflag ? " " : ", ");
			printf("%s", dp->name);
			n++;
		}
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	struct mixer *m;
	char lstr[8], rstr[8], *name = NULL, buf[NAME_MAX];
	float l, r, lrel, rrel;
	int dusage = 0, opt = 0, dunit;
	int aflag = 0, dflag = 0, oflag = 0, sflag = 0;
	int i, rc = 0;
	char ch, n, k;

	while ((ch = getopt(argc, argv, "ad:f:or:s")) != -1) {
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
		case 'r':
			/* Reserved for {+|-|^|=}rec rdev. */
			/* FIXME: problems with -rec */
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

	while (!dusage) {
		if (dflag) {
			/* We don't want to get in here again. */
			dflag = 0;
			/* XXX: should we die if any of these two fails? */
			if ((n = mixer_getdunit()) < 0) {
				warn("cannot get default unit");
				continue;
			}
			if (mixer_setdunit(m, dunit) < 0) {
				warn("cannot set default unit to %d", dunit);
				continue;
			}
			printf("default_unit: %d -> %d\n", n, dunit);
		} else if (sflag) {
			printrecsrc(m, oflag);
			goto done;
		} else if (argc > 0 && strcmp("rec", *argv + 1) == 0) {
			if (**argv != '+' && **argv != '-' &&
			    **argv != '=' && **argv != '^') {
				warnx("unknown modifier: %c", **argv);
				dusage = 1;
				break;
			}
			switch (**argv) {
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
			}
			if ((m->dev = mixer_getdevbyname(m, argv[1])) == NULL) {
				warn("unknown recording device: %s", argv[1]);
				/* XXX */
				rc = 1;
				goto done;
			}
			/* Keep the previous state. */
			n = m->dev->f_src != 0;
			if (mixer_modrecsrc(m, opt) < 0)
				warn("cannot modify device: %c%s", **argv, argv[1]);
			else
				printf("%s.recsrc: %d -> %d\n", argv[1],
				    n, m->dev->f_src != 0);
			argc -= 2;
			argv += 2;
		} else if (argc > 0) {
			if ((k = sscanf(*argv, "%f:%f", &l, &r)) > 0)
				;	/* nothing */
			else if ((m->dev = mixer_getdevbyname(m, *argv)) == NULL) {
				warn("unknown device: %s", *argv);
				rc = 1;
				goto done;
			}

			lrel = rrel = 0;
			if (argc > 1) {
				n = sscanf(argv[1], "%7[^:]:%7s", lstr, rstr);
				if (n == EOF) {
					warnx("invalid value: %s", argv[1]);
					dusage = 1;
					break;
				}
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
			}
			switch (argc > 1 ? n : k) {
			case 0:
				if (!oflag)
					printmixer(m);
				printdev(m->dev, oflag);
				goto done;
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

				printf("%s.volume: %.2f:%.2f -> %.2f:%.2f\n",
				   m->dev->name, m->dev->lvol, m->dev->rvol, l, r);

				if (mixer_setvol(m, l, r) < 0) {
					warnx("cannot change volume");
					rc = 1;
				}
				argc -= 2;
				argv += 2;
			}
		} else
			break;
	}

	if (dusage) {
		(void)mixer_close(m);
		usage();
	} else
		printall(m, oflag);
done:
	(void)mixer_close(m);

	return (rc);
}
