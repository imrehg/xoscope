/*
 * @(#)$Id: sc_linux.c,v 2.4 2009/06/26 05:18:48 baccala Exp $
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the Linux ESD and sound card interfaces
 *
 * These two interfaces are very similar and share a lot of code.  In
 * shared routines, we can tell which one we're using by looking at
 * the 'snd' and 'esd' variables to see which one is a valid file
 * descriptor (!= -1).  Although it's possible for both to be open at
 * the same time (say, when sound card is open, user pushes '&' to
 * select next device, and xoscope tries to open ESD to see if it
 * works before closing the sound card), this really shouldn't happen
 * in any of the important routines.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>		/* for abs() */
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include "oscope.h"		/* program defaults */
#ifdef HAVE_LIBESD
#include <esd.h>
#endif

#define ESDDEVICE "ESounD"
#define SOUNDDEVICE "/dev/dsp"

static int snd = -2;		/* file descriptor for /dev/dsp */
static int esd = -2;		/* file descriptor for ESD */

#ifdef HAVE_LIBESD
static int esdblock = 0;	/* 1 to block ESD; 0 to non-block */
static int esdrecord = 0;	/* 1 to use ESD record mode; 0 to use ESD monitor mode */
#endif

static int sc_chans = 0;
static int sound_card_rate = DEF_R;	/* sampling rate of sound card */

/* Signal structures we're capturing into */
static Signal left_sig = {"Left Mix", "a"};
static Signal right_sig = {"Right Mix", "b"};

static int trigmode = 0;
static int triglev;
static int trigch;

static char * snd_errormsg1 = NULL;
static char * snd_errormsg2 = NULL;

#ifdef HAVE_LIBESD
static char * esd_errormsg1 = NULL;
static char * esd_errormsg2 = NULL;
#endif

/* This function is defined as do-nothing and weak, meaning it can be
 * overridden by the linker without error.  It's used for the X
 * Windows GUI for this data source, and is defined in this way so
 * that this object file can be used either with or without GTK.  If
 * this causes compiler problems, just comment out the attribute lines
 * and leave the do-nothing functions.
 */

void sc_gtk_option_dialog() __attribute__ ((weak));
void sc_gtk_option_dialog() {}

void esd_gtk_option_dialog() __attribute__ ((weak));
void esdsc_gtk_option_dialog() {}

/* close the sound device */
static void
close_sound_card()
{
  if (snd >= 0) {
    close(snd);
    snd = -2;
  }
}

/* show system error and close sound device if given ioctl status is bad */
static void
check_status_ioctl(int d, int request, void *argp, int line)
{
  if (ioctl(d, request, argp) < 0) {
    snd_errormsg1 = "sound ioctl";
    snd_errormsg2 = strerror(errno);
    close_sound_card();
  }
}

#ifdef HAVE_LIBESD

/* close ESD */
static void
close_ESD()
{
  if (esd >= 0) {
    close(esd);
    esd = -2;
  }
}

/* turn ESD on */
static int
open_ESD(void)
{
  if (esd >= 0) return 1;

  esd_errormsg1 = NULL;
  esd_errormsg2 = NULL;

  if (esdrecord) {
    esd = esd_record_stream_fallback(ESD_BITS8|ESD_STEREO|ESD_STREAM|ESD_RECORD,
				     ESD_DEFAULT_RATE, NULL, progname);
  } else {
    esd = esd_monitor_stream(ESD_BITS8|ESD_STEREO|ESD_STREAM|ESD_MONITOR,
                             ESD_DEFAULT_RATE, NULL, progname);
  }

  if (esd <= 0) {
    esd_errormsg1 = "opening " ESDDEVICE;
    esd_errormsg2 = strerror(errno);
    return 0;
  }

  /* we have esd connection! non-block it? */
  if (!esdblock)
    fcntl(esd, F_SETFL, O_NONBLOCK);

  return 1;
}

#endif

/* turn the sound device (/dev/dsp) on */
static int
open_sound_card(void)
{
  int rate = sound_card_rate;
  int chan = 2;
  int bits = 8;
  int parm;
  int i = 3;

  if (snd >= 0) return 1;

  snd_errormsg1 = NULL;
  snd_errormsg2 = NULL;

  /* we try a few times in case someone else is using device (FvwmAudio) */
  while ((snd = open(SOUNDDEVICE, O_RDONLY | O_NDELAY)) < 0 && i > 0) {
    sleep(1);
    i --;
  }

  if (snd < 0) {
    snd_errormsg1 = "opening " SOUNDDEVICE;
    snd_errormsg2 = strerror(errno);
    return 0;
  }

  parm = chan;			/* set mono/stereo */
  //check_status_ioctl(snd, SOUND_PCM_WRITE_CHANNELS, &parm, __LINE__);
  check_status_ioctl(snd, SNDCTL_DSP_CHANNELS, &parm, __LINE__);
  sc_chans = parm;

  parm = bits;			/* set 8-bit samples */
  check_status_ioctl(snd, SOUND_PCM_WRITE_BITS, &parm, __LINE__);

  parm = rate;			/* set sampling rate */
  //check_status_ioctl(snd, SOUND_PCM_WRITE_RATE, &parm, __LINE__);
  //check_status_ioctl(snd, SOUND_PCM_READ_RATE, &parm, __LINE__);
  check_status_ioctl(snd, SNDCTL_DSP_SPEED, &parm, __LINE__);
  sound_card_rate = parm;

  return 1;
}

static void
reset_sound_card(void)
{
  static char junk[SAMPLESKIP];

#ifdef HAVE_LIBESD
  if (esd >= 0) {

    close_ESD();
    open_ESD();

    if (esd < 0) return;

    read(esd, junk, SAMPLESKIP);

    return;
  }
#endif

  if (snd >= 0) {

    close_sound_card();
    open_sound_card();

    if (snd < 0) return;

    read(snd, junk, SAMPLESKIP);
  }
}

#ifdef HAVE_LIBESD

static int esd_nchans(void)
{
  /* ESD always has two channels, right? */

  if (esd == -2) open_ESD();

  return (esd >= 0) ? 2 : 0;
}

#endif

static int sc_nchans(void)
{
  if (snd == -2) open_sound_card();

  return (snd >= 0) ? sc_chans : 0;
}

static int fd(void)
{
  return (snd >= 0) ? snd : esd;
}

static Signal *sc_chan(int chan)
{
  return (chan ? &right_sig : &left_sig);
}

/* Triggering - we save the trigger level in the raw, unsigned
 * byte values that we read from the sound card
 */

static int set_trigger(int chan, int *levelp, int mode)
{
  trigch = chan;
  trigmode = mode;
  triglev = 127 + *levelp;
  if (triglev > 255) {
    triglev = 255;
    *levelp = 128;
  }
  if (triglev < 0) {
    triglev = 0;
    *levelp = -128;
  }
  return 1;
}

static void clear_trigger(void)
{
  trigmode = 0;
}

static int change_rate(int dir)
{
  int newrate = sound_card_rate;

  if (esd >= 0) return 0;

  if (dir > 0) {
    if (sound_card_rate > 16500)
      newrate = 44100;
    else if (sound_card_rate > 9500)
      newrate = 22050;
    else
      newrate = 11025;
  } else {
    if (sound_card_rate < 16500)
      newrate = 8000;
    else if (sound_card_rate < 33000)
      newrate = 11025;
    else
      newrate = 22050;
  }

  if (newrate != sound_card_rate) {
    sound_card_rate = newrate;
    return 1;
  }
  return 0;
}

static void
reset(void)
{
  reset_sound_card();

#ifdef HAVE_LIBESD
  left_sig.rate = esd >= 0 ? ESD_DEFAULT_RATE : sound_card_rate;
  right_sig.rate = esd >= 0 ? ESD_DEFAULT_RATE : sound_card_rate;
#else
  left_sig.rate = sound_card_rate;
  right_sig.rate = sound_card_rate;
#endif

  left_sig.num = 0;
  left_sig.frame ++;

  right_sig.num = 0;
  right_sig.frame ++;
}

/* set_width(int)
 *
 * sets the frame width (number of samples captured per sweep) globally
 * for all the channels.
 */

static void set_width(int width)
{
  left_sig.width = width;
  right_sig.width = width;

  if (left_sig.data != NULL) free(left_sig.data);
  if (right_sig.data != NULL) free(right_sig.data);

  left_sig.data = malloc(width * sizeof(short));
  right_sig.data = malloc(width * sizeof(short));
}

/* get data from sound card, return value is whether we triggered or not */
static int
get_data()
{
  static unsigned char buffer[MAXWID * 2];
  static int i, j, delay;
  int fd;

  if (snd >= 0) fd = snd;
  else if (esd >= 0) fd = esd;
  else return (0);

  if (!in_progress) {
    /* Discard excess samples so we can keep our time snapshot close
       to real-time and minimize sound recording overruns.  For ESD we
       don't know how many are available (do we?) so we discard them
       all to start with a fresh buffer that hopefully won't wrap
       around before we get it read. */

    /* read until we get something smaller than a full buffer */
    while ((j = read(fd, buffer, sizeof(buffer))) == sizeof(buffer));

  } else {

    /* XXX this ends up discarding everything after a complete read */
    j = read(fd, buffer, sizeof(buffer));

  }

  i = 0;

  if (!in_progress) {

    if (trigmode == 1) {
      i ++;
      while (((i+1)*2 <= j) && ((buffer[2*i + trigch] < triglev) ||
				(buffer[2*(i-1) + trigch] >= triglev))) i ++;
    } else if (trigmode == 2) {
      i ++;
      while (((i+1)*2 <= j) && ((buffer[2*i + trigch] > triglev) ||
				(buffer[2*(i-1) + trigch] <= triglev))) i ++;
    }

    if ((i+1)*2 > j)		/* haven't triggered within the screen */
      return(0);		/* give up and keep previous samples */

    delay = 0;

    if (trigmode) {
      int last = buffer[2*(i-1) + trigch] - 127;
      int current = buffer[2*i + trigch] - 127;
      if (last != current)
	delay = abs(10000 * (current - (triglev - 127)) / (current - last));
    }

    left_sig.data[0] = buffer[2*i] - 127;
    left_sig.delay = delay;
    left_sig.num = 1;
    left_sig.frame ++;

    right_sig.data[0] = buffer[2*i + 1] - 127;
    right_sig.delay = delay;
    right_sig.num = 1;
    right_sig.frame ++;

    i ++;
    in_progress = 1;
  }

  while ((i+1)*2 <= j) {
    if (in_progress >= left_sig.width) break;
#if 0
    if (*buff == 0 || *buff == 255)
      clip = i % 2 + 1;
#endif
    left_sig.data[in_progress] = buffer[2*i] - 127;
    right_sig.data[in_progress] = buffer[2*i + 1] - 127;

    in_progress ++;
    i ++;
  }

  left_sig.num = in_progress;
  right_sig.num = in_progress;

  if (in_progress >= left_sig.width) in_progress = 0;

  return(1);
}

static char * snd_status_str(int i)
{
#ifdef DEBUG
  static char string[16];
  sprintf(string, "status %d", i);
  return string;
#endif

  switch (i) {
  case 0:
    if (snd_errormsg1) return snd_errormsg1;
    else return "";
  case 2:
    if (snd_errormsg2) return snd_errormsg2;
    else return "";
  }
  return NULL;
}

#ifdef HAVE_LIBESD

static char * esd_status_str(int i)
{
  switch (i) {
  case 0:
    if (esd_errormsg1) return esd_errormsg1;
    else return "";
  case 2:
    if (esd_errormsg2) return esd_errormsg2;
    else return "";
  }
  return NULL;
}

/* ESD option key 1 (*) - toggle Record mode */

static int option1_esd(void)
{
  if(esdrecord)
    esdrecord = 0;
  else
    esdrecord = 1;

  return 1;
}

static char * option1str_esd(void)
{
  static char string[16];

  sprintf(string, "RECORD:%d", esdrecord);
  return string;
}

static int esd_set_option(char *option)
{
  if (sscanf(option, "esdrecord=%d", &esdrecord) == 1) {
    return 1;
  } else {
    return 0;
  }
}

/* The GUI interface in sc_linux_gtk.c depends on the esdrecord option
 * being number 1.
 */

static char * esd_save_option(int i)
{
  static char buf[32];

  switch (i) {
  case 1:
    snprintf(buf, sizeof(buf), "esdrecord=%d", esdrecord);
    return buf;

  default:
    return NULL;
  }
}

#endif

#ifdef DEBUG
static char * option1str_sc(void)
{
  static char string[16];

  sprintf(string, "opt1");
  return string;
}

static char * option2str_sc(void)
{
  static char string[16];

  sprintf(string, "opt2");
  return string;
}
#endif

static int sc_set_option(char *option)
{
  if (sscanf(option, "rate=%d", &sound_card_rate) == 1) {
    return 1;
  } else if (strcmp(option, "dma=") == 0) {
    /* a deprecated option, return 1 so we don't indicate error */
    return 1;
  } else {
    return 0;
  }
}

static char * sc_save_option(int i)
{
  static char buf[32];

  switch (i) {
  case 0:
    snprintf(buf, sizeof(buf), "rate=%d", sound_card_rate);
    return buf;

  default:
    return NULL;
  }
}

#ifdef HAVE_LIBESD
DataSrc datasrc_esd = {
  "ESD",
  esd_nchans,
  sc_chan,
  set_trigger,
  clear_trigger,
  change_rate,
  set_width,
  reset,
  fd,
  get_data,
  esd_status_str,
  option1_esd,  /* option1 */
  option1str_esd,  /* option1str */
  NULL,  /* option2, */
  NULL,  /* option2str, */
  esd_set_option,  /* set_option */
  esd_save_option,  /* save_option */
  esd_gtk_option_dialog,  /* gtk_options */
};
#endif

DataSrc datasrc_sc = {
  "Soundcard",
  sc_nchans,
  sc_chan,
  set_trigger,
  clear_trigger,
  change_rate,
  set_width,
  reset,
  fd,
  get_data,
  snd_status_str,
#ifdef DEBUG
  NULL,
  option1str_sc,
  NULL,
  option2str_sc,
#else
  NULL,  /* option1, */
  NULL,  /* option1str, */
  NULL,  /* option2, */
  NULL,  /* option2str, */
#endif
  sc_set_option,
  sc_save_option,
  NULL,  /* gtk_options */
};
