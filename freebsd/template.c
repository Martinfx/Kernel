/*
 * Copyright (c) 2000 Cameron Grant <cg@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHERIN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THEPOSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* -------------------------------------------------------------------- */

#define inline __inline

#define XX_PCI_ID 	0x00000000

struct sc_info;

/* channel registers */
struct sc_pchinfo {
	u_int32_t spd, fmt;
	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct sc_info *parent;
};

struct sc_rchinfo {
	u_int32_t spd, fmt;
	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct sc_info *parent;
};

/* device private data */
struct sc_info {
	device_t dev;
	u_int32_t type;

	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;

	struct resource *reg, *irq;
	int regtype, regid, irqid;
	void *ih;
	void *lock;

	int power;
	struct sc_pchinfo pch;
	struct sc_rchinfo rch;
};

/* -------------------------------------------------------------------- */
/* prototypes */

/* stuff */
static void      XX_intr(void *);
static int       XX_power(struct sc_info *, int);
static int       XX_init(struct sc_info *);
static int       XX_uninit(struct sc_info *);
static int       XX_reinit(struct sc_info *);

/* talk to the card */
static u_int32_t XX_rd(struct sc_info *, int, int);
static void 	 XX_wr(struct sc_info *, int, u_int32_t, int);

/* -------------------------------------------------------------------- */
/* channel descriptors */

static u_int32_t XX_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	AFMT_S16_BE,
	AFMT_STEREO | AFMT_S16_BE,
	AFMT_U16_BE,
	AFMT_STEREO | AFMT_U16_BE,
	0
};
static struct pcmchan_caps XX_reccaps = {4000, 48000, XX_recfmt, 0};

static u_int32_t XX_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	AFMT_S16_BE,
	AFMT_STEREO | AFMT_S16_BE,
	AFMT_U16_BE,
	AFMT_STEREO | AFMT_U16_BE,
	0
};
static struct pcmchan_caps XX_playcaps = {4000, 48000, XX_playfmt, 0};

/*
 * newpcm driver locking:
 *
 * each channel, mixer and ac97 instance possesses its own lock, which
 * will be held when any method belonging to that instance is called.
 * thus, the driver need only provide interlocking where an operation on
 * one channel, mixer or ac97 instance may conflict with an operation in
 * progress on another channel, mixer or ac97 instance.
 *
 * all locking operations should be performed using:
 * void *snd_mtxcreate(char *name)
 *	create a mutex
 * void snd_mtxfree(void *m)
 *	destroy a mutex
 * void snd_mtxlock(void *m) / void snd_mtxunlock(*m)
 * 	lock/unlock a mutex
 */
static inline void
XX_lock(struct sc_info *sc)
{
	snd_mtxlock(sc->lock);
}

static inline void
XX_unlock(struct sc_info *sc)
{
	snd_mtxunlock(sc->lock);
}

/* -------------------------------------------------------------------- */
/* Hardware */
static inline u_int32_t
XX_rd(struct sc_info *sc, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(sc->st, sc->sh, regno);
	case 2:
		return bus_space_read_2(sc->st, sc->sh, regno);
	case 4:
		return bus_space_read_4(sc->st, sc->sh, regno);
	default:
		return 0xffffffff;
	}
}

static inline void
XX_wr(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	switch (size) {
	case 1:
		bus_space_write_1(sc->st, sc->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(sc->st, sc->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(sc->st, sc->sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/* ac97 codec */

static u_int32_t
XX_initcd(kobj_t obj, void *devinfo)
{
	struct sc_info *sc = (struct sc_info *)devinfo;

	/* init ac-link */

	/* return number of codecs */
}

static int
XX_rdcd(kobj_t obj, void *devinfo, int regno)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	int codecno;

	codecno = regno >> 8;
	regno &= 0xff;

	/* return value of register regno from codec codecno, -1 === error */
	return -1;
}

static void
XX_wrcd(kobj_t obj, void *devinfo, int regno, u_int32_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	int codecno;

	codecno = regno >> 8;
	regno &= 0xff;
	/* write data to register regno in codec codecno */

	/* return 0 == ok, -1 == error */
	return -1;
}

static kobj_method_t XX_ac97_methods[] = {
    	KOBJMETHOD(ac97_init,		XX_initcd),
    	KOBJMETHOD(ac97_read,		XX_rdcd),
    	KOBJMETHOD(ac97_write,		XX_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(XX_ac97);

/* -------------------------------------------------------------------- */
/* play channel interface */

static void *
XXpchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_pchinfo *ch;

	KASSERT(dir == PCMDIR_PLAY, ("XXpchan_init: bad direction"));
	ch = &sc->pch;
	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = DSP_DEFAULT_SPEED;
	if (sndbuf_alloc(ch->buffer, sc->parent_dmat, XX_BUFFSIZE) == -1)
		return NULL;

	return ch;
}

static int
XXpchan_free(kobj_t obj, void *data)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* free up buffer - called after channel stopped */
	sndbuf_free(ch->buffer);

	/* return 0 if ok */
	return 0;
}

static int
XXpchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* set format from getcaps list - not called if channel running */
	/* return 0 - format _must_ be settable if in list */
	return 0;
}

static int
XXpchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* return closest possible speed */
	return speed;
}

static int
XXpchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct sc_pchinfo *ch = data;

	/* set max #bytes between irqs - not called if channel running */
	/* return closest possible block size <= blocksize */
	return blocksize;
}

static int
XXpchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	switch(go) {
	case PCMTRIG_START:
		/* start at beginning of buffer */
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		/* stop operation */
		break;

	case PCMTRIG_EMLDMAWR:
		/* got play irq, transfer next buffer - ignore if using dma */
	case PCMTRIG_EMLDMARD:
		/* got rec irq, transfer next buffer - ignore if using dma */
	default:
		break;
	}

	/* return 0 if ok */
	return 0;
}

static int
XXpchan_getptr(kobj_t obj, void *data)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* return current byte offset of channel */
}

static struct pcmchan_caps *
XXpchan_getcaps(kobj_t obj, void *data)
{
	return &XX_playcaps;
}

static kobj_method_t XXpchan_methods[] = {
    	KOBJMETHOD(channel_init,		XXpchan_init),
    	KOBJMETHOD(channel_free,		XXpchan_free),
    	KOBJMETHOD(channel_setformat,		XXpchan_setformat),
    	KOBJMETHOD(channel_setspeed,		XXpchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	XXpchan_setblocksize),
    	KOBJMETHOD(channel_trigger,		XXpchan_trigger),
    	KOBJMETHOD(channel_getptr,		XXpchan_getptr),
    	KOBJMETHOD(channel_getcaps,		XXpchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(XXpchan);

/* -------------------------------------------------------------------- */
/* rec channel interface */

static void *
XXrchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_rchinfo *ch;

	KASSERT(dir == PCMDIR_REC, ("XXrchan_init: bad direction"));
	ch = &sc->rch;
	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = DSP_DEFAULT_SPEED;
	if (sndbuf_alloc(ch->buffer, sc->parent_dmat, XX_BUFFSIZE) == -1)
		return NULL;

	return ch;
}

static int
XXrchan_free(kobj_t obj, void *data)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* free up buffer - called after channel stopped */
	sndbuf_free(ch->buffer);

	/* return 0 if ok */
	return 0;
}

static int
XXrchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* set format from getcaps list - not called if channel running */
	/* return 0 - format _must_ be settable if in list */
	return 0;
}

static int
XXrchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* return closest possible speed */
	return speed;
}

static int
XXrchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct sc_rchinfo *ch = data;

	/* set max #bytes between irqs - not called if channel running */
	/* return closest possible block size <= blocksize */
	return blocksize;
}

static int
XXrchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	switch(go) {
	case PCMTRIG_START:
		/* start at beginning of buffer */
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		/* stop operation */
		break;

	case PCMTRIG_EMLDMAWR:
		/* got play irq, transfer next buffer - ignore if using dma */
	case PCMTRIG_EMLDMARD:
		/* got rec irq, transfer next buffer - ignore if using dma */
	default:
		break;
	}

	/* return 0 if ok */
	return 0;
}

static int
XXrchan_getptr(kobj_t obj, void *data)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	/* return current byte offset of channel */
}

static struct pcmchan_caps *
XXrchan_getcaps(kobj_t obj, void *data)
{
	return &XX_reccaps;
}

static kobj_method_t XXrchan_methods[] = {
    	KOBJMETHOD(channel_init,		XXrchan_init),
    	KOBJMETHOD(channel_free,		XXrchan_free),
    	KOBJMETHOD(channel_setformat,		XXrchan_setformat),
    	KOBJMETHOD(channel_setspeed,		XXrchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	XXrchan_setblocksize),
    	KOBJMETHOD(channel_trigger,		XXrchan_trigger),
    	KOBJMETHOD(channel_getptr,		XXrchan_getptr),
    	KOBJMETHOD(channel_getcaps,		XXrchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(XXrchan);

/* -------------------------------------------------------------------- */
/* The interrupt handler */

 /*
  * locking:
  * the device may be locked if required for irq handling, but should be
  * unlocked when chn_intr() is called.
  */
static void
XX_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
}

/* -------------------------------------------------------------------- */
/* stuff */

static int
XX_power(struct sc_info *sc, int state)
{
	switch (state) {
	case 0: /* full power */
		break;
	case 1:
	case 2:
	case 3: /* power off */
		break;
	}
	sc->power = state;

	return 0;
}

static int
XX_init(struct sc_info *sc)
{
	return 0;
}

static int
XX_uninit(struct sc_info *sc)
{
	return 0;
}

static int
XX_reinit(struct sc_info *sc)
{
	return 0;
}

/* -------------------------------------------------------------------- */
/* Probe and attach the card */

static int
XX_pci_probe(device_t dev)
{
	char *s = NULL;

	switch (pci_get_devid(dev)) {
	case XX_PCI_ID:
		s = "xxx";
		break;
	}

	if (s)
		device_set_desc(dev, s);
	return s? 0 : ENXIO;
}

static int
XX_pci_attach(device_t dev)
{
	struct sc_info *sc;
	struct ac97_info *codec;
	u_int32_t data;
	char status[SND_STATUSLEN];

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	sc->lock = snd_mtxcreate(device_get_nameunit(dev));
	sc->dev = dev;
	sc->type = pci_get_devid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);

	sc->regid = PCIR_MAPS;
	sc->regtype = SYS_RES_MEMORY;
	sc->reg = bus_alloc_resource(dev, sc->regtype, &sc->regid,
					0, ~0, 1, RF_ACTIVE);
	if (!sc->reg) {
		sc->regtype = SYS_RES_IOPORT;
		sc->reg = bus_alloc_resource(dev, sc->regtype, &sc->regid,
						0, ~0, 1, RF_ACTIVE);
	}
	if (sc->reg) {
		sc->st = rman_get_bustag(sc->reg);
		sc->sh = rman_get_bushandle(sc->reg);
	} else {
		device_printf(dev, "unable to allocate register space\n");
		goto bad;
	}

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq) {
		device_printf(dev, "unable to allocate interrupt\n");
		goto bad;
	}

	/*
	 * once the driver performs whatever locking it needs, s/0/INTR_MPSAFE/ on
	 * the next line.
	 */
	if (snd_setup_intr(dev, sc->irq, 0, XX_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to setup interrupt\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/XX_BUFFSIZE, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &sc->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	/* power up */
	XX_power(sc, 0);

	/* init chip */
	if (XX_init(sc) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	/* create/init mixer */
	codec = AC97_CREATE(dev, sc, XX_ac97);
	if (codec == NULL)
		goto bad;
	mixer_init(dev, ac97_getmixerclass(), codec);

	if (pcm_register(dev, sc, 1, 1))
		goto bad;
	pcm_addchan(dev, PCMDIR_REC, &XXpchan_class, sc);
	pcm_addchan(dev, PCMDIR_PLAY, &XXrchan_class, sc);

 	snprintf(status, SND_STATUSLEN, "at %s 0x%lx irq %ld",
		 (sc->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(sc->reg), rman_get_start(sc->irq));
	pcm_setstatus(dev, status);

	return 0;

bad:
	if (codec)
		ac97_destroy(codec);
	if (sc->reg)
		bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	if (sc->ih)
		bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	if (sc->parent_dmat)
		bus_dma_tag_destroy(sc->parent_dmat);
	if (sc->lock)
		snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return ENXIO;
}

static int
XX_pci_detach(device_t dev)
{
	int r;
	struct sc_info *sc;

	r = pcm_unregister(dev);
	if (r)
		return r;

	sc = pcm_getdevinfo(dev);
	/* shutdown chip */
	XX_uninit(sc);

	/* power off */
	XX_power(sc, 3);

	bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	bus_dma_tag_destroy(sc->parent_dmat);
	snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return 0;
}

static int
XX_pci_suspend(device_t dev)
{
	struct sc_info *sc;

	sc = pcm_getdevinfo(dev);
	/* save chip state */

	/* power off */
	XX_power(sc, 3);

	return 0;
}

static int
XX_pci_resume(device_t dev)
{
	struct sc_info *sc;

	sc = pcm_getdevinfo(dev);

	/* power up */
	XX_power(sc, 0);

	/* reinit chip */
	if (XX_reinit(sc) == -1) {
        	device_printf(dev, "unable to reinitialize the card\n");
        	return ENXIO;
	}

	/* restore chip state */

	/* restore mixer state */
	if (mixer_reinit(dev) == -1) {
        	device_printf(dev, "unable to reinitialize the mixer\n");
        	return ENXIO;
	}

	return 0;
}

static device_method_t XX_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		XX_pci_probe),
	DEVMETHOD(device_attach,	XX_pci_attach),
	DEVMETHOD(device_detach,	XX_pci_detach),
	DEVMETHOD(device_suspend,       XX_pci_suspend),
	DEVMETHOD(device_resume,        XX_pci_resume),
	{ 0, 0 }
};

static driver_t XX_driver = {
	"pcm",
	XX_methods,
	sizeof(struct snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_XX, pci, XX_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_XX, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_XX, 1);
