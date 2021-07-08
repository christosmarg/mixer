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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mixer.h"

#define BASEPATH "/dev/mixer"

static int _mixer_readvol(struct mixer *, struct mix_dev *);

static const char *names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
static int ndev = 0;

static int
_mixer_readvol(struct mixer *m, struct mix_dev *dev)
{
	int v;

	if (ioctl(m->fd, MIXER_READ(dev->devno), &v) < 0)
		return (-1);
	dev->vol.left = M_VOLNORM(v & 0x00ff);
	dev->vol.right = M_VOLNORM((v >> 8) & 0x00ff);

	return (0);
}

/*
 * Open a mixer device in `/dev/mixerN`, where N is the number of the mixer 
 * file. Each device maps to an actual pcmN audio card, so `/dev/mixer0` 
 * is the mixer device for pcm0, and so on.
 *
 * @param: `name`: path to a mixer device. If it's NULL or "/dev/mixer", 
 *	we open the default mixer (i.e `hw.snd.default_unit`).
 */
struct mixer *
mixer_open(const char *name)
{
	struct mixer *m = NULL;
	struct mix_dev *dp;
	int i;

	if ((m = calloc(1, sizeof(struct mixer))) == NULL)
		goto fail;

	if (name != NULL) {
		/* `name` does not start with "/dev/mixer". */
		if (strncmp(name, BASEPATH, strlen(BASEPATH)) != 0) {
			errno = EINVAL;
			goto fail;
		}
		/* `name` is "/dev/mixer" so, we'll use the default unit. */
		if (strncmp(name, BASEPATH, strlen(name)) == 0)
			goto dunit;
		m->unit = strtol(name + strlen(BASEPATH), NULL, 10);
		(void)strlcpy(m->name, name, sizeof(m->name));
	} else {
dunit:
		if ((m->unit = mixer_getdunit()) < 0)
			goto fail;
		(void)snprintf(m->name, sizeof(m->name) - 1, "/dev/mixer%d", m->unit);
	}

	if ((m->fd = open(m->name, O_RDWR)) < 0)
		goto fail;

	m->devmask = m->recmask = m->recsrc = 0;
	m->f_default = m->unit == mixer_getdunit();
	/* The unit number _must_ be set before the ioctl. */
	m->mi.dev = m->unit;
	m->ci.card = m->unit;
	if (ioctl(m->fd, SNDCTL_MIXERINFO, &m->mi) < 0 || 
	    ioctl(m->fd, SNDCTL_CARDINFO, &m->ci) < 0 || 
	    ioctl(m->fd, SOUND_MIXER_READ_DEVMASK, &m->devmask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_MUTE, &m->mutemask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_RECMASK, &m->recmask) < 0 ||
	    ioctl(m->fd, SOUND_MIXER_READ_RECSRC, &m->recsrc) < 0)
		goto fail;

	TAILQ_INIT(&m->devs);
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (!M_ISDEV(m, i))
			continue;
		if ((dp = calloc(1, sizeof(struct mix_dev))) == NULL)
			goto fail;
		dp->devno = i;
		if (_mixer_readvol(m, dp) < 0)
			goto fail;
		(void)strlcpy(dp->name, names[i], sizeof(dp->name));
		TAILQ_INSERT_TAIL(&m->devs, dp, devs);
		ndev++;
	}

	/* The default device is always "vol". */
	m->dev = TAILQ_FIRST(&m->devs);

	return (m);
fail:
	if (m != NULL)
		(void)mixer_close(m);

	return (NULL);
}

/*
 * Free resources and close the mixer.
 */
int
mixer_close(struct mixer *m)
{
	struct mix_dev *dp;
	int r;

	r = close(m->fd);
	while (!TAILQ_EMPTY(&m->devs)) {
		dp = TAILQ_FIRST(&m->devs);
		TAILQ_REMOVE(&m->devs, dp, devs);
		free(dp);
	}
	free(m);

	return (r);
}

struct mix_dev *
mixer_getdev(struct mixer *m, int dev)
{
	struct mix_dev *dp;

	if (dev < 0 || dev >= ndev) {
		errno = ERANGE;
		return (NULL);
	}
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (dp->devno == dev)
			return (dp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Select a mixer device (e.g vol, pcm, mic) by name. The mixer structure
 * keeps a list of all the devices the mixer has, but only one can be
 * manipulated at a time -- this is what the `dev` field is for. Each
 * time we want to manipulate a device, `dev` has to point to it first.
 *
 * The caller has to assign the return value to `m->dev`.
 *
 * @param: `name`: device name (e.g vol, pcm, ...)
 */
struct mix_dev *
mixer_getdevbyname(struct mixer *m, const char *name)
{
	struct mix_dev *dp;

	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (!strncmp(dp->name, name, sizeof(dp->name)))
			return (dp);
	}
	errno = EINVAL;

	return (NULL);
}

/*
 * Change the mixer's left and right volume. The allowed volume values are
 * between M_VOLMIN and M_VOLMAX. The `ioctl` for volume change requires
 * an integer value between 0 and 100 stored as `lvol | rvol << 8` --  for
 * that reason, we de-normalize the 32-bit float volume value, before
 * we pass it to the `ioctl`.
 *
 * If the volumes passed are not in the range `M_VOLMIN <= vol <= M_VOLMAX`,
 * we return an error and `errno` is set to ERANGE. Volume clumping should
 * be handlded by the caller.
 */
int
mixer_setvol(struct mixer *m, mix_volume_t vol)
{
	int v;

	if (vol.left < M_VOLMIN || vol.left > M_VOLMAX || 
	    vol.right < M_VOLMIN || vol.right > M_VOLMAX) {
		errno = ERANGE;
		return (-1);
	}
	v = M_VOLDENORM(vol.left) | M_VOLDENORM(vol.right) << 8;
	if (ioctl(m->fd, MIXER_WRITE(m->dev->devno), &v) < 0)
		return (-1);
	if (_mixer_readvol(m, m->dev) < 0)
		return (-1);

	return (0);
}

int
mixer_setmute(struct mixer *m, int opt)
{
	switch (opt) {
	case M_MUTE:
		m->mutemask |= (1 << m->dev->devno);
		break;
	case M_UNMUTE:
		m->mutemask &= ~(1 << m->dev->devno);
		break;
	case M_TOGGLEMUTE:
		m->mutemask ^= (1 << m->dev->devno);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (ioctl(m->fd, SOUND_MIXER_WRITE_MUTE, &m->mutemask) < 0)
		return (-1);
	if (ioctl(m->fd, SOUND_MIXER_READ_MUTE, &m->mutemask) < 0)
		return (-1);

	return 0;
}

/*
 * Modify the mixer's selected device flags. The `recsrc` flag tells
 * us if a device is a recording source.
 */
int
mixer_modrecsrc(struct mixer *m, int opt)
{
	if (!m->recmask || !M_ISREC(m, m->dev->devno)) {
		errno = ENODEV;
		return (-1);
	}
	switch (opt) {
	case M_ADDRECDEV:
		m->recsrc |= (1 << m->dev->devno);
		break;
	case M_REMOVERECDEV:
		m->recsrc &= ~(1 << m->dev->devno);
		break;
	case M_SETRECDEV:
		m->recsrc = (1 << m->dev->devno);
		break;
	case M_TOGGLERECDEV:
		m->recsrc ^= (1 << m->dev->devno);
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (ioctl(m->fd, SOUND_MIXER_WRITE_RECSRC, &m->recsrc) < 0)
		return (-1);
	if (ioctl(m->fd, SOUND_MIXER_READ_RECSRC, &m->recsrc) < 0)
		return (-1);

	return (0);
}

/*
 * Get default audio card's number. This is used to open the default mixer
 * and set the mixer structure's `f_default` flag.
 */
int
mixer_getdunit(void)
{
	int unit;
	size_t size;

	size = sizeof(int);
	if (sysctlbyname("hw.snd.default_unit", &unit, &size, NULL, 0) < 0)
		return (-1);

	return (unit);
}

/*
 * Change the default audio card. This is normally _not_ a mixer feature, but
 * it's useful to have, so the caller can avoid having to manually use
 * the sysctl API.
 * 
 * @param: `unit`: the audio card number (e.g pcm0, pcm1, ...).
 */
int
mixer_setdunit(struct mixer *m, int unit)
{
	size_t size;

	size = sizeof(int);
	if (sysctlbyname("hw.snd.default_unit", NULL, 0, &unit, size) < 0)
		return (-1);
	m->f_default = m->unit == unit;

	return (0);
}

/*
 * Get the total number of mixers in the system.
 */
int
mixer_getnmixers(void)
{
	struct mixer *m;
	oss_sysinfo si;

	/* 
	 * Open a dummy mixer because we need the `fd` field for the
	 * `ioctl` to work.
	 */
	if ((m = mixer_open(NULL)) == NULL)
		return (-1);
	if (ioctl(m->fd, OSS_SYSINFO, &si) < 0) {
		(void)mixer_close(m);
		return (-1);
	}
	(void)mixer_close(m);

	return (si.nummixers);
}
