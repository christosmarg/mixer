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

#ifndef _MIXER_H_
#define _MIXER_H_

__BEGIN_DECLS

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/soundcard.h>

#include <limits.h>
#include <math.h>

#define M_ISSET(n,f)	(((1 << (n)) & (f)) ? 1 : 0)
#define M_ISDEV(m,n)	M_ISSET(n, (m)->devmask)
#define M_ISMUTE(m,n)	M_ISSET(n, (m)->mutemask)
#define M_ISREC(m,n)	M_ISSET(n, (m)->recmask)
#define M_ISRECSRC(m,n)	M_ISSET(n, (m)->recsrc)

struct mix_dev {
	char name[NAME_MAX];
	int devno;
	struct mix_volume {
#define M_VOLMIN	0.0f
#define M_VOLMAX	1.0f
#define M_VOLNORM(v)	((v) / 100.0f)
#define M_VOLDENORM(v)	((int)roundf((v) * 100.0f))
		float left;
		float right;
	} vol;
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
	int ndev;
	int devmask;
#define M_MUTE		0x01
#define M_UNMUTE	0x02
#define M_TOGGLEMUTE	0x04
	int mutemask;
	int recmask;
#define M_ADDRECSRC	0x01
#define M_REMOVERECSRC	0x02
#define M_SETRECSRC	0x04
#define M_TOGGLERECSRC	0x08
	int recsrc;
	int f_default;
};

typedef struct mix_volume mix_volume_t;

struct mixer *mixer_open(const char *);
int mixer_close(struct mixer *);
struct mix_dev *mixer_getdev(struct mixer *, int);
struct mix_dev *mixer_getdevbyname(struct mixer *, const char *);
int mixer_setvol(struct mixer *, mix_volume_t);
int mixer_setmute(struct mixer *, int);
int mixer_modrecsrc(struct mixer *, int);
int mixer_getdunit(void);
int mixer_setdunit(struct mixer *, int);
int mixer_getnmixers(void);

__END_DECLS

#endif /* _MIXER_H_ */
