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

#define MIX_ISSET(n,f)		(((1 << (n)) & (f)) ? 1 : 0)
#define MIX_ISDEV(m,n)		MIX_ISSET(n, (m)->devmask)
#define MIX_ISMUTE(m,n)		MIX_ISSET(n, (m)->mutemask)
#define MIX_ISREC(m,n)		MIX_ISSET(n, (m)->recmask)
#define MIX_ISRECSRC(m,n)	MIX_ISSET(n, (m)->recsrc)

struct mix_dev {
	char name[NAME_MAX];
	int devno;
	struct mix_volume {
#define MIX_VOLMIN		0.0f
#define MIX_VOLMAX		1.0f
#define MIX_VOLNORM(v)		((v) / 100.0f)
#define MIX_VOLDENORM(v)	((int)roundf((v) * 100.0f))
		float left;
		float right;
	} vol;
	TAILQ_ENTRY(mix_dev) devs;
};

struct mixer {
	TAILQ_HEAD(, mix_dev) devs;
	struct mix_dev *dev;
	oss_mixerinfo mi;
	oss_card_info ci;
	char name[NAME_MAX];
	int fd;
	int unit;
	int ndev;
	int devmask;
#define MIX_MUTE		0x01
#define MIX_UNMUTE		0x02
#define MIX_TOGGLEMUTE		0x04
	int mutemask;
	int recmask;
#define MIX_ADDRECSRC		0x01
#define MIX_REMOVERECSRC	0x02
#define MIX_SETRECSRC		0x04
#define MIX_TOGGLERECSRC	0x08
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
