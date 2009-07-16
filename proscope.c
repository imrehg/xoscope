/*
 * @(#)$Id: proscope.c,v 2.1 2009/01/15 07:05:59 baccala Exp $
 *
 * Copyright (C) 1997 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the ProbeScope protocol, as defined in its' hlp file.
 * Tested with a "V3.0 1995" RadioShack ProbeScope.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include "proscope.h"
#include "oscope.h"
#include "display.h"

unsigned int ps_rate[] = {20000000, 10000000, 2000000, 1000000,
			  200000, 100000, 20000, 10000, 2000, 1000};

ProbeScope ps;

Signal ps_signal;

int psfd;

/* identify a ProbeScope; called from ser_*.c files */
int
idprobescope(int fd)
{
  int c, byte = 0, try = 0;

  flush_serial(fd);

  while (byte < 300 && try < 75) { /* give up in 7.5ms */
    if ((c = getonebyte(fd)) < 0) {
      usleep(100);		/* try again in 0.1ms */
      try++;
    } else if (c > 0x7b) {
      psfd = fd;
      return(1);		/* ProbeScope found! */
    } else
      byte++;
    PSDEBUG("%d\t", try);
    PSDEBUG("%d\n", byte);
  }

  return(0);
}

/* initialize the probescope structure */
static int
open_probescope(void)
{

  ps.found = ps.wait = ps.volts = 0;
  ps.trigger = ps.level = ps.dvm = ps.flags = 0;
  ps.coupling = "?";

  ps.probed = 1;

  ps_signal.width = 128;
  if (ps_signal.data == NULL) ps_signal.data = malloc(128 * sizeof(short));

  return init_serial_probescope();
}

static int
serial_fd(void)
{
  return psfd;
}

static int nchans(void)
{
  if (!ps.found && !ps.probed) open_probescope();

  return ps.found ? 1 : 0;
}

static Signal *ps_chan(int chan)
{
  return &ps_signal;
}

/* No rate change support for Probescope */

static int change_rate(int dir)
{
  return 0;
}

static void reset(void)
{
  ps.probed = 0;
}

/* get one set of bytes from the ProbeScope, if possible */
int
get_data(void)
{
  int c, maybe, byte = -1, try = 0, cls = 0, dvm = 0, gotdvm = 0, flush = 1;

  while (byte < 138 && try < 280) { /* allow max 2 cycles to find sync byte */
    c = getonebyte(psfd);
    if (c < 0) {		/* byte available? no: */
      if (++try > 10 && byte <= 1) {
	PSDEBUG("%s\n", "!");
	byte = -1;
	try = 999;		/* give up if we've retried ~ 4ms */
      } else {
	PSDEBUG("%s", ".");
	flush = 0;		/* we've caught up with the serial FIFO! */
	usleep(400);		/* next arrive ~ 8bits/19200bits/s = .4167ms */
      }
    } else {			/* byte available? yes: */
      if (c > 0x7b) {		/* Synchronization Byte and/or WAITING! */
	PSDEBUG("\n%3d", byte);
	PSDEBUG(",%3d:", try);
	PSDEBUG("%02x  ", c);
	if ((maybe = c & PS_WAIT ? 0 : 1) != ps.wait) {
	  ps.wait = maybe;
	  cls = 1;
	}
	while ((c = getonebyte(psfd)) > 0x7b && ++try < 280) {
	}			/* suck all available sync/wait bytes */
	if (ps.wait || try >= 280) {
	  byte = -1;		/* return now if we're waiting or timed out */
	  try = 888;
	} else if (c > -1) {	/* we have byte 1 now */
	  byte = 1;
	} else {		/* byte 1 could be next */
	  byte = 0;
	}
      }
      if (byte >= 5 && byte <= 132) { /* Signal Bytes from 0 to 3F Hex */
	ps_signal.data[byte - 4] = (c - 32) * 5;
      } else if (byte >= 134 && byte <= 136) { /* DVM Values, 100, 10, 1 */
	PSDEBUG("%d,", c);
	dvm += c * (byte == 134 ? 100 : byte == 135 ? 10 : 1);
	gotdvm = 1;
      } else if (byte == 1) {	/* Switch Setting Byte */
	PSDEBUG("1sw=%02x  ", c);
	if ((maybe = c & PS_100V ? 100 : c & PS_10V ? 10 : 1) != ps.volts) {
	  ps.volts = maybe;
	  ps_signal.volts = maybe * 1000;
	  cls = 1;
	}
	ps.coupling = c & PS_AC ? "AC" : c & PS_DC ? "DC" : "GND";
      } else if (byte == 2) {	/* Timebase Definition Byte */
	PSDEBUG("2tb=%02x  ", c);
	if (c < 10 && (maybe = ps_rate[c]) != ps_signal.rate) {
	  ps_signal.rate = maybe;
	  cls = 1;
	}
      } else if (byte == 3) {	/* Trigger Definition Byte, true if set */
	PSDEBUG("3tr=%02x  ", c);
	if (c != ps.trigger) {
	  ps.trigger = c;
	  cls = 1;
	}
      } else if (byte == 4) {	/* Trigger Level Byte, true if set */
	PSDEBUG("4lv=%02x  ", c);
	if ((maybe = c & PS_TP5 ? 5 : c & PS_TP3 ? 3 : c & PS_TP1 ? 1
	     : c & PS_TM1 ? -1 : c & PS_TM3 ? -3 : -5) != ps.level) {
	  ps.level = maybe;
	  cls = 1;
	}
      } else if (byte == 133) { /* Undocumented Trigger Level (maybe?) */
	PSDEBUG("133TR=%02x  ", c);
      } else if (byte == 137) {	/* DVM Flags, true if set */
	PSDEBUG(" 137FL=%02x", c);
	ps.flags = c;
	if (c & PS_UNDERFLOW || c & PS_OVERFLOW)
	  clip = 3;
	if (c & PS_MINUS)
	  dvm *= -1;
      }
      if (byte > -1) byte++;	/* processed a valid byte */
      try++;
    }
  }
  ps_signal.data[0] = ps_signal.data[1];
  ps_signal.frame ++;
  ps_signal.num = 128;

  if (gotdvm) ps.dvm = dvm;
  if (cls) clear();		/* non-DVM text need changed? */
  if (flush) flush_serial(psfd);	/* catch up if we're getting behind */
  return 1;		/* XXX not sure about this */
}

static char * status_str(int i)
{
  static char string[81];

  if (ps.probed && !ps.found) {
    switch (i) {
    case 0:
      return split_field(serial_error, 0, 16);
    case 2:
      return split_field(serial_error, 1, 16);
    default:
      return "";
    }
  }

  if (!ps.found) return "";

  switch(i) {

  case 0:
    sprintf(string, "%d Volt Range", ps.volts);
    return string;

  case 1:
    return (ps.trigger & PS_SINGLE ? "SINGLE" : "   RUN");

  case 4:
    sprintf(string, "%s ~ %g V", ps.trigger & PS_PINT ? "+INTERN"
	    : ps.trigger & PS_MINT ? "-INTERN"
	    : ps.trigger & PS_PEXT ? "+EXTERN"
	    : ps.trigger & PS_MEXT ? "-EXTERN" : "AUTO",
	    (float)ps.level * (ps.trigger & PS_PEXT || ps.trigger & PS_MEXT
			       ? 1.0 : (float)ps.volts) / 10);
    return string;

  case 7:
    return (ps.wait ? "WAITING!" : NULL);
  }

  return NULL;
}

DataSrc datasrc_ps = {
  "ProbeScope",
  nchans,
  ps_chan,
  NULL, /* set_trigger, */
  NULL, /* clear_trigger, */
  change_rate,
  NULL,		/* set_width */
  reset,
  serial_fd,
  get_data,
  status_str,
  NULL, /* option1, */
  NULL, /* option1str, */
  NULL, /* option2, */
  NULL, /* option2str, */
  NULL,
  NULL,
  NULL,	/* gtk_options */
};
