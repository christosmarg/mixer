/*-
 * Copyright (c) 2021 Christos Margiolis <christos@freebsd.org>
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

#ifndef _MIXER_H_
#define _MIXER_H_

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/soundcard.h>

#include <limits.h>

#define M_ADDRECDEV		0x01
#define M_REMOVERECDEV		0x02
#define M_SETRECDEV		0x04
#define M_TOGGLERECDEV		0x08

#define M_ISSET(n, f)		((1 << (n)) & (f))
#define M_ISDEV(m, n)		M_ISSET(n, (m)->devmask)
#define M_ISREC(m, n)		M_ISSET(n, (m)->recmask)
#define M_ISRECSRC(m, n)	M_ISSET(n, (m)->recsrc)

struct mix_dev {
	char name[NAME_MAX];
	int devno;
	int lvol;
	int rvol;
	int lmute;
	int rmute;
	//int rate;
	//int samples;
	int f_pbk;
	int f_rec;
	TAILQ_ENTRY(mix_dev) devs;
};

struct mixer {
	TAILQ_HEAD(head, mix_dev) devs;
	struct mix_dev *dev;
	oss_mixerinfo mi;
	oss_card_info ci;
	char name[NAME_MAX];
	int fd;
	int unit;
	int devmask;
	int recmask;
	int recsrc;
	int f_default;
};

struct mixer *mixer_open(const char *);
int mixer_close(struct mixer *);
int mixer_seldevbyname(struct mixer *, const char *, int);
int mixer_chvol(struct mixer *, int, int);
int mixer_modrecsrc(struct mixer *, int);
int mixer_get_default_unit(void);
int mixer_set_default_unit(struct mixer *, int);

#endif /* _MIXER_H_ */
