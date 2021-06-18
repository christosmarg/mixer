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

static void usage(struct mixer *) __dead2;
static void printall(struct mixer *);
static void printmixer(struct mixer *);
static void printdev(struct mix_dev *);

static void __dead2
usage(struct mixer *m)
{
	printf("usage: %1$s [-f device] [-d unit] [dev [+|-][lvol[:[+|-]rvol]] ...\n"
	    "       %1$s [-f device] [-d unit] -s ...\n"
	    "       %1$s [-f device] [-d unit] -r {^|+|-|=}rdev ...\n"
	    "       %1$s -a\n",
	    getprogname());
	(void)mixer_close(m);
	exit(1);
}

static void
printall(struct mixer *m)
{
	struct mix_dev *dp;

	printmixer(m);
	TAILQ_FOREACH(dp, &m->devs, devs) {
		printdev(dp);
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
printdev(struct mix_dev *d)
{
	printf("    %-11s= %.2f:%.2f\t%+.2f\t", 
	    d->name, d->lvol, d->rvol, d->pan);
	if (d->f_pbk)
	       printf(" pbk");
	if (d->f_rec)
	       printf(" rec");
	if (d->f_src)
		printf(" src");
	printf("\n");
}

int
main(int argc, char *argv[])
{
	struct mixer *m;
	struct mix_dev *dp;
	char lstr[8], rstr[8], *recstr;
	char *name = NULL, buf[NAME_MAX];
	float l, r, lrel, rrel;
	int dusage = 0, opt = 0, dunit;
	int aflag = 0, dflag = 0, rflag = 0, sflag = 0;
	int i, rc = 0;
	char ch, t, k, n;

	while ((ch = getopt(argc, argv, "ad:f:r:s")) != -1) {
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
		case 'r':
			recstr = optarg;
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			dusage = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (aflag) {
		if ((n = mixer_get_nmixers()) < 0)
			err(1, "mixer_get_nmixers");
		for (i = 0; i < n; i++) {
			(void)snprintf(buf, sizeof(buf), "/dev/mixer%d", i);
			if ((m = mixer_open(buf)) == NULL)
				err(1, "mixer_open: %s", buf);
			printall(m);
			(void)mixer_close(m);
		}
		/* XXX: should we return here? */
		return (0);
	}

	if ((m = mixer_open(name)) == NULL)
		err(1, "mixer_open: %s", name);

	while (!dusage) {
		if (dflag) {
			/* We don't want to get in here again. */
			dflag = 0;
			if (mixer_set_default_unit(m, dunit) < 0) {
				warn("cannot set default unit to %d", dunit);
				continue;
			}
			printf("changed default unit to: %d\n", dunit);
		} else if (rflag) {
			rflag = 0;
			if (*recstr != '+' && *recstr != '-' &&
			    *recstr != '=' && *recstr != '^') {
				warnx("unkown modifier: %c", *recstr);
				dusage = 1;
				break;
			}
			switch (*recstr) {
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
			if (*(++recstr) == '\0') {
				warnx("no recording device specified");
				dusage = 1;
				break;
			}
			if ((m->dev = mixer_seldevbyname(m, recstr,
			    m->recmask)) == NULL) {
				warn("unkown recording device: %s", recstr);
				rc = 1;
				goto done;
			}
			if (mixer_modrecsrc(m, opt) < 0) {
				warn("cannot modify device");
				rc = 1;
				goto done;
			}
		} else if (sflag) {
			if (!m->recmask)
				goto done;
			n = 0;
			printmixer(m);
			printf("    recording source: ");
			TAILQ_FOREACH(dp, &m->devs, devs) {
				if (M_ISRECSRC(m, dp->devno)) {
					if (n)
						printf(", ");
					printf("%s", dp->name);
					n++;
				}
			}
			printf("\n");
			goto done;
		} else if (argc > 0) {
			if ((t = sscanf(*argv, "%f:%f", &l, &r)) > 0)
				;	/* nothing */
			else if ((m->dev = mixer_seldevbyname(m, *argv, 
			    m->devmask)) == NULL) {
				warn("unkown device: %s", *argv);
				rc = 1;
				goto done;
			}

			lrel = rrel = 0;
			if (argc > 1) {
				k = sscanf(argv[1], "%7[^:]:%7s", lstr, rstr);
				if (k == EOF) {
					warnx("invalid value: %s", argv[1]);
					dusage = 1;
					break;
				}
				if (k > 0) {
					if (*lstr == '+' || *lstr == '-')
						lrel = rrel = 1;
					l = strtof(lstr, NULL);
				}
				if (k > 1) {
					if (*rstr == '+' || *rstr == '-')
						rrel = 1;
					r = strtof(rstr, NULL);
				}
			}

			switch (argc > 1 ? k : t) {
			case 0:
				printmixer(m);
				printdev(m->dev);
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

				printf("Mixer %s: %.2f:%.2f -> %.2f:%.2f\n",
				   m->dev->name, m->dev->lvol, m->dev->rvol, l, r);

				if (mixer_chvol(m, l, r) < 0) {
					warnx("cannot change volume");
					rc = 1;
				}
				goto done;
			}
		} else
			break;
	}

	dusage ? usage(m) : printall(m);
done:
	(void)mixer_close(m);

	return (rc);
}
