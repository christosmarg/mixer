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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mixer.h>

static void usage(struct mixer *) __dead2;
static void printrecsrc(struct mixer *, int);

static void __dead2
usage(struct mixer *m)
{
	struct mix_dev *dp;
	int n;

	printf("usage: %1$s [-f device] [-s | -S] [dev [+|-][voll[:[+|-]volr]] ...\n"
	    "       %1$s [-f device] [-s | -S] recsrc ...\n"
	    "       %1$s [-f device] [-s | -S] {^|+|-|=}rec rdev ...\n",
	    getprogname());
	if (m->devmask) {
		printf(" devices: ");
		n = 0;
		TAILQ_FOREACH(dp, &m->devs, devs) {
			if (M_ISDEV(m, dp->devno)) {
				if (n)
					printf(", ");
				printf("%s", dp->name);
				n++;
			}
		}
	}
	if (m->recmask) {
		printf("\n rec devices: ");
		n = 0;
		TAILQ_FOREACH(dp, &m->devs, devs) {
			if (M_ISREC(m, dp->devno)) {
				if (n)
					printf(", ");
				printf("%s", dp->name);
				n++;
			}
		}
	}
	printf("\n");
	(void)mixer_close(m);
	exit(1);
}

static void
printrecsrc(struct mixer *m, int sflag)
{
	struct mix_dev *dp;
	int n;

	if (!m->recmask)
		return;
	if (!sflag)
		printf("Recording source: ");
	n = 0;
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (M_ISRECSRC(m, dp->devno)) {
			if (sflag)
				printf("%srec ", n ? " +" : "=");
			else if (n)
				printf(", ");
			printf("%s", dp->name);
			n++;
		}
	}
	if (!sflag)
		printf("\n");
}

int
main(int argc, char *argv[])
{
	struct mixer *m;
	struct mix_dev *dp;
	char lstr[8], rstr[8], *name = NULL;
	float l, r, lrel, rrel;
	int dusage = 0, drecsrc = 0, sflag = 0, Sflag = 0;
	char ch, n, t, k, opt = 0;

	/* FIXME: problems with -rec */
	while ((ch = getopt(argc, argv, "f:sS")) != -1) {
		switch (ch) {
		case 'f':
			name = optarg;
			break;
		case 's':
			sflag = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		case '?':
		default:
			dusage = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (sflag && Sflag)
		dusage = 1;

	if ((m = mixer_open(name)) == NULL)
		err(1, "mixer_open: %s", name);

	if (!argc && !dusage) {
		n = 0;
		TAILQ_FOREACH(dp, &m->devs, devs) {
			if (sflag || Sflag) {
				printf("%s%s%c%.2f:%.2f", n ? " " : "",
				    dp->name, Sflag ? ':' : ' ',
				    dp->lvol, dp->rvol);
				n++;
			} else
				printf("Mixer %-8s is currently set to %.2f:%.2f\n",
				    dp->name, dp->lvol, dp->rvol);
		}
		if (n && m->recmask)
			printf(" ");
		printrecsrc(m, sflag || Sflag);
		(void)mixer_close(m);
		return 0;
	}

	n = 0;
	while (argc > 0 && !dusage) {
		if (strcmp("recsrc", *argv) == 0) {
			drecsrc = 1;
			argc--;
			argv++;
			continue;
		} else if (strcmp("rec", *argv + 1) == 0) {
			if (**argv != '+' && **argv != '-'
			&&  **argv != '=' && **argv != '^') {
				warnx("unkown modifier: %c", **argv);
				dusage = 1;
				break;
			}
			if (argc <= 1) {
				warnx("no recording device specified");
				dusage = 1;
				break;
			}
			if ((m->dev = mixer_seldevbyname(m, argv[1], 
			    m->recmask)) == NULL) {
				warnx("unkown recording revice: %s", argv[1]);
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
			if (mixer_modrecsrc(m, opt) < 0) {
				warnx("cannot modify device");
				break;
			}
			drecsrc = 1;
			argc -= 2;
			argv += 2;
			continue;
		}

		if ((t = sscanf(*argv, "%f:%f", &l, &r)) > 0)
			;	/* nothing */
		else if ((m->dev = mixer_seldevbyname(m, *argv, 
		    m->devmask)) == NULL) {
			warnx("unkown device: %s", *argv);
			dusage = 1;
			break;
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
			printf("Mixer %-8s is currently set to %.2f:%.2f\n",
			    m->dev->name, m->dev->lvol, m->dev->rvol);
			argc--;
			argv++;
			continue;
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
			if (mixer_chvol(m, l, r) < 0) {
				warnx("cannot change volume");
				break;
			}
			if (!Sflag)
				printf("Setting the mixer %s from %.2f:%.2f "
				   "to %.2f:%.2f\n",
				   m->dev->name, m->dev->lvol, m->dev->rvol, l, r);
			argc -= 2;
			argv += 2;
		}
	}

	if (dusage)
		usage(m);
	if (drecsrc) 
		printrecsrc(m, sflag || Sflag);
	(void)mixer_close(m);

	return 0;
}
