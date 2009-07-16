/*
 * @(#)$Id: ser_unix.c,v 2.0 2008/12/17 17:35:46 baccala Exp $
 *
 * Copyright (C) 1997 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the unix (Linux) serial interface to scopes.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <termio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "oscope.h"
#include "proscope.h"
#include "bitscope.h"

char device[512] = "";		/* Serial device */
int sflags;
struct termio stbuf, svbuf;	/* termios: svbuf=saved, stbuf=set */

char serial_error[256];

/* return a single byte from the serial device or return -1 if none avail. */
int
getonebyte(int fd)
{
  static unsigned char ch;

  if (read(fd, &ch, 1) == 1)
    return(ch);
  return(-1);
}

/* return a single byte from the serial device or return -1 if none avail. */
int
GETONEBYTE(int fd)			/* we buffer here just to be safe */
{
  static unsigned char buff[256];
  static int count = 0, pos = 0;

  if (pos >= count) {
    if (fd)
      if ((count = read(fd, buff, 256 * sizeof(unsigned char))) < 1)
	return(-1);
    pos = 0;
  }
  return buff[pos++];
}

/* discard all input, clearing the serial FIFO queue to catch up */
void
flush_serial(int fd)
{
  while (getonebyte(fd) > -1) {
  }
}

/* serial cleanup routine called as the program exits: restore settings */
void
cleanup_serial(int fd)
{
  if (fd > 0) {
    if (ioctl(fd, TCSETA, &svbuf) < 0) {
      /* sprintf(serial_error, "Can't ioctl set device %s", device); */
      /* perror(error); */
    }
    close(fd);
  }
}

/* check given serial device for BitScope (0) or ProbeScope (1) */
int
findscope(char *dev, int i)
{
  int fd;

  if ((fd = open(dev, sflags)) < 0) {
    sprintf(serial_error, "%s %s", dev, strerror(errno));
    return(0);
  }
  if (ioctl(fd, TCGETA, &svbuf) < 0) { /* save settings */
    sprintf(serial_error, "%s Can't ioctl TCGETA", dev);
    close(fd);
    return(0);
  }
  if (ioctl(fd, TCSETA, &stbuf) < 0) {
    sprintf(serial_error, "%s Can't ioctl TCSETA", dev);
    close(fd);
    return(0);
  }

  if ((i && idprobescope(fd)) || (!i && idbitscope(fd))) {
    return (1);		/* serial port scope found! */
  }

  if (ioctl(fd, TCSETA, &svbuf) < 0) { /* restore settings */
#if 0
    sprintf(serial_error, "Can't ioctl (set) %s", dev);
    close(fd);
    return(0);		/* nothing found. */
#endif
  }

  sprintf(serial_error, "%s No %s found\n", dev,
	  i==0 ? "BitScope" : "ProbeScope");
  return(0);
}

/* set [scope].found to non-zero if we find a scope on a serial port */
int
init_serial_bitscope(void)
{
  char *p;

  /* BitScope serial port settings */
  sflags = O_RDWR | O_NDELAY | O_NOCTTY;
  memset(&stbuf, 0, sizeof(stbuf));
  stbuf.c_cflag = B57600 | CS8 | CLOCAL | CREAD;
  stbuf.c_iflag = IGNPAR;	/* | ICRNL; (this would hose binary dumps) */

  if ((p = getenv("BITSCOPE")) == NULL) /* first place to look */
    p = BITSCOPE;		/* -D default defined in Makefile */
  strcpy(device, p);

  return findscope(device, 0);
}

/* set [scope].found to non-zero if we find a scope on a serial port */
int
init_serial_probescope(void)
{
  char *p;

  /* ProbeScope serial port settings */
  sflags = O_RDONLY | O_NDELAY;
  memset(&stbuf, 0, sizeof(stbuf));
  stbuf.c_cflag = B19200 | CS7 | CLOCAL | CREAD;
  stbuf.c_iflag = ISTRIP;

  if ((p = getenv("PROBESCOPE")) == NULL) /* second place to look */
    p = PROBESCOPE;		/* -D default defined in Makefile */
  strcpy(device, p);

  return findscope(device, 1);
}
