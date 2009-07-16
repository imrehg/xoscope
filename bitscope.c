/*
 * @(#)$Id: bitscope.c,v 2.1 2009/01/15 07:05:59 baccala Exp $
 *
 * Copyright (C) 2000 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the BitScope (www.bitscope.com) driver.
 * Developed on a 10 MHz "BC000112" BitScope with Motorola MC10319 ADC.
 * Some parts based on bitscope.c by Ingo Cyliax
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "oscope.h"
#include "proscope.h"		/* for PSDEBUG macro */
#include "bitscope.h"

BitScope bs;			/* the BitScope structure */

static Signal analogA_signal, analogB_signal, digital_signal;

/* use real read and write except on DOS where they are emulated */
#ifndef GO32
#define serial_read(a, b, c)	read(a, b, c)
#define serial_write(a, b, c)	write(a, b, c)
#endif

/* This function is defined as do-nothing and weak, meaning it can be
 * overridden by the linker without error.  It's used to start the X
 * Windows GTK options dialog for Bitscope, and is defined in this way
 * so that this object file can be used either with or without GTK.
 * If this causes compiler problems, just comment out the attribute
 * line and leave the do-nothing function.  You will then need to
 * comment out both lines to generate an object file that can be used
 * with GTK.
 */

void bitscope_dialog() __attribute__ ((weak));
void bitscope_dialog() {}

/* run CMD command string on bitscope FD or return 0 if unsuccessful */
static int
bs_cmd(int fd, char *cmd)
{
  int i, j, k;
  char c;

/*    if (fd < 3) return(0); */
  in_progress = 0;
  j = strlen(cmd);
  PSDEBUG("bs_cmd: %s\n", cmd);
  for (i = 0; i < j; i++) {
    if (serial_write(fd, cmd + i, 1) == 1) {
      k = 10;
      while (serial_read(fd, &c, 1) < 1) {
	if (!k--) return(0);
	PSDEBUG("cmd sleeping %d\n", k);
	usleep(1);
      }
      if (cmd[i] != c)
	fprintf(stderr, "bs mismatch @ %d: sent:%c != recv:%c\n", i, cmd[i], c);
    } else {
      sprintf(error, "write failed to %d", fd);
      perror(error);
    }
  }
  return(1);
}

/* read N bytes from bitscope FD into BUF, or return 0 if unsuccessful */
static int
bs_read(int fd, char *buf, int n)
{
  int i = 0, j, k = n + 10;

  in_progress = 0;
  buf[0] = '\0';
  while (n) {
    if ((j = serial_read(fd, buf + i, n)) < 1) {
      if (!k--) return(0);
      PSDEBUG("read sleeping %d\n", k);
      usleep(1);
    } else {
      n -= j;
      i += j;
    }
  }
  buf[i] = '\0';
  return(1);
}

/* asynchronously read N bytes from bitscope FD into BUF, return N when done */
static int
bs_read_async(int fd, char *buf, int n, char c)
{
  static char *pos, *end;
  int i;

  if (in_progress) {
    if ((i = serial_read(fd, pos, end - pos)) > 0) {
      pos += i;
      if (pos >= end) {
	in_progress = 0;
	buf[n] = '\0';
	return(n);
      }
    }
    return(0);
  }
  pos = buf;
  *pos = '\0';
  end = pos + n;
  in_progress = c;
  return(0);
}

/* Run IN on bitscope FD and store results in OUT (not including echo) */
/* Only the last command in IN should produce output; else this won't work */
static int
bs_io(int fd, char *in, char *out)
{
  char c;

  out[0] = '\0';
  if (!in_progress) {
    if (in[0] == 'M')
      bs_cmd(fd, bs.version >= 110 ? "M" : "S");
    else
      bs_cmd(fd, in);
  }
  switch (c = in[strlen(in) - 1]) {
  case 'T':
    return bs_read_async(fd, out, 6, c);
  case 'M':
    if (bs.version >= 110)
      return bs_read_async(fd, out, R15 * 2, c);
  case 'A':
    if (bs.version >= 110)
      return bs_read_async(fd, out, R15, c);
  case 'S':			/* DDAA, per sample + newlines per 16 */
    return bs_read_async(fd, out, R15 * 5 + (R15 / 16) + 1, c);
  case 'P':
    if (bs.version >= 110)
      return bs_read(fd, out, 14);
    break;
  case 0:			/* least used last for efficiency */
  case '?':
    return bs_read(fd, out, 10);
  case 'p':
    return bs_read(fd, out, 4);
  case 'R':
  case 'W':
    if (bs.version >= 110)
      return bs_read(fd, out, 4);
  }
  in_progress = 0;
  return(1);
}

static int
bs_getreg(int fd, int reg)
{
  unsigned char i[8], o[8];
  sprintf(i, "[%x]@p", reg);
  if (!bs_io(fd, i, o))
    return(-1);
  return strtol(o, NULL, 16);
}

static int
bs_getregs(int fd, unsigned char *reg)
{
  unsigned char buf[8];
  int i;

  if (!bs_cmd(fd, "[3]@"))
    return(0);
  for (i = 3; i < 24; i++) {
    if (!bs_io(fd, "p", buf))
      return(0);
    reg[i] = strtol(buf, NULL, 16);
//    printf("reg[%d]=%d\n", i, reg[i]);
    if (!bs_io(fd, "n", buf))
      return(0);
  }
  return(1);
}

static int
bs_putregs(int fd, unsigned char *reg)
{
  unsigned char buf[8];
  int i;

  if (!bs_cmd(fd, "[3]@"))
    return(0);
  for (i = 3; i < 24; i++) {
    if (reg[i])
      sprintf(buf, "[%x]sn", reg[i]);
    else
      sprintf(buf, "[sn");
    if (!bs_cmd(fd, buf))
      return(0);
  }
  return(1);
}

static void
bs_flush_serial()
{
  int c, byte = 0;

  c = 10;			/* clear serial FIFO */
  while ((byte < sizeof(bs.buf)) && c--) {
    byte += serial_read(bs.fd, bs.buf, sizeof(bs.buf) - byte);
    usleep(1);
  }
}


static char *
save_option(int i)
{
  int regs[] = {0, 5, 6, 7, 14}; /* regs to save */
  static char buf[32];

  if (i < sizeof(regs) / sizeof(regs[0])) {
    sprintf(buf, "%d:0x%02x\n", regs[i], bs.r[regs[i]]);
    return buf;
  } else {
    return NULL;
  }
}


static int
parse_option(char *optarg)
{
  char *p;
  int reg = 0, val = 0;

  reg = strtol(optarg, NULL, 0);
  if (reg >= 0 && reg < 24) {
    if ((p = strchr(optarg, ':')) != NULL) {
      val = strtol(++p, NULL, 0);
      bs.R[reg] = val;
//      printf("bs_option: %s b[%d] = %d\n", optarg, reg, val);
      return 1;
    }
  }
  return 0;
}


static int
bs_changerate(int fd, int dir)
{
  unsigned char buf[10];

  bs_flush_serial();
  if (dir > 0)
    bs.r[13] = bs.r[13] ? bs.r[13] / 2 : 128; /* TB	time base */
  else if (dir < 0)
    bs.r[13] = bs.r[13] ? bs.r[13] * 2 : 1;
  /* XXX need to set rate in Signal structures here - is this formula right? */
//  mem[23].rate = mem[24].rate = mem[25].rate = 1250000 / (19 + 3 * (R13 + 1));
  sprintf(buf, "[d]@[%x]s", bs.r[13]);
  bs_cmd(fd, buf);
  return 1;
}

static int changerate(int dir)
{
  return bs_changerate(bs.fd, dir);
}


static void
bs_fixregs(int fd)
{
  int i;

  bs.r[3] = bs.r[4] = 0;
//  bs.r[5] = 0;
//  bs.r[6] = 127;		/*  scope.trig; ? */
  //  bs.r[6] = 0xff;		/* don't care */
//  bs.r[7] = TRIGEDGE | TRIGEDGEF2T | LOWER16BNC | TRIGCOMPARE | TRIGANALOG;
  bs.r[8] = 1;			/* trace mode, 0-4 */

  /* 0: 0, 2, 179 */
  /* 1: 63, 53, 1 */
  /* 2: 0, 80, 1 */
  /* 3: 0, 16319, 1 */

  //  SETWORD(&bs.r[11], 2);	/* try to just fill the 16384 memory: */
  //  bs.r[13] = 179;		/* 2,179 through mode 0 formula = 1644 */

  bs.r[20] = 63;			/* PRETD	pre trigger delay */
  SETWORD(&bs.r[11], 53);	/* PTD		post trigger delay */
  bs.r[13] = 1;
  bs_changerate(fd, 0);

//  bs.r[14] = PRIMARY(RANGE1200 | CHANNELA | ZZCLK)
//    | SECONDARY(RANGE1200 | CHANNELB | ZZCLK);
  bs.r[15] = 0;			/* max samples per dump (256) */
//  bs.r[0] = 0x11;
  for (i = 0; i < 24; i++) {
    if (bs.R[i] > -1)
      bs.r[i] = bs.R[i];
  }
}

/* initialize previously identified BitScope FD */
static void
bs_init(int fd)
{
  static int volts[] = {
     130 * 5 / 2,  600 * 5 / 2,  1200 * 5 / 2,  3160 * 5 / 2,
    1300 * 5 / 2, 6000 * 5 / 2, 12000 * 5 / 2, 31600 * 5 / 2,
     632 * 5 / 2, 2900 * 5 / 2,  5800 * 5 / 2, 15280 * 5 / 2,
  };
  static char *labels[] = {
    "BNC B x1",		"BNC A x1",
    "BNC B x10",	"BNC A x10",
    "POD Ch. A",	"POD Ch. B",
    "Logic An.",
  };

  if (fd < 3) return;
  bs.found = 1;
  in_progress = 0;
  bs.version = strtol(bs.bcid + 2, NULL, 10);
  analogA_signal.rate = 25000000;
  analogB_signal.rate = 25000000;
  digital_signal.rate = 25000000;
//  mem[23].rate = mem[24].rate = mem[25].rate = 12500000;

  analogA_signal.width = MAXWID;
  analogB_signal.width = MAXWID;
  digital_signal.width = MAXWID;

  if (analogA_signal.data != NULL) free(analogA_signal.data);
  if (analogB_signal.data != NULL) free(analogB_signal.data);
  if (digital_signal.data != NULL) free(digital_signal.data);

  analogA_signal.data = malloc(MAXWID * sizeof(short));
  analogB_signal.data = malloc(MAXWID * sizeof(short));
  digital_signal.data = malloc(MAXWID * sizeof(short));

//  printf("bs_init\n");
  bs_getregs(fd, bs.r);
  bs_fixregs(fd);
  bs_putregs(fd, bs.r);

  strcpy(digital_signal.name, labels[6]);

  if (bs.r[7] & UPPER16POD) {
    analogA_signal.volts = volts[(bs.r[14] & RANGE3160) + 8];
    analogB_signal.volts = volts[((bs.r[14] >> 4) & RANGE3160) + 8];
    strcpy(analogA_signal.name, labels[4]);
    strcpy(analogB_signal.name, labels[5]);
  } else {
    analogA_signal.volts = volts[(bs.r[14] & RANGE3160)
				+ (bs.r[0] & 0x01 ? 4 : 0)];
    analogB_signal.volts = volts[((bs.r[14] >> 4) & RANGE3160)
				+ (bs.r[0] & 0x10 ? 4 : 0)];
    strcpy(analogA_signal.name,
	   labels[((bs.r[14] & PRIMARY(CHANNELA)) ? 1 : 0)
		 + ((bs.r[0] & 0x01) ? 2 : 0)]);
    strcpy(analogB_signal.name,
	   labels[((bs.r[14] & SECONDARY(CHANNELA)) ? 1 : 0)
		 + ((bs.r[0] & 0x10) ? 2 : 0)]);
  }
}

/* identify a BitScope; called from ser_*.c files */
int
idbitscope(int fd)
{
  bs.fd = fd;

  flush_serial(bs.fd);

  bs_flush_serial();
  if (bs_io(bs.fd, "?", bs.buf) && bs.buf[1] == 'B' && bs.buf[2] == 'C') {
    strncpy(bs.bcid, bs.buf + 1, sizeof(bs.bcid));
    bs.bcid[8] = '\0';
    bs_init(bs.fd);
    return(1);		/* BitScope found! */
  }
  return(0);
}

static int open_bitscope(void)
{
  int i;

  for (i = 0; i < sizeof(bs.R) / sizeof(bs.R[0]); i++) {
    bs.R[i] = -1;
  }

  bs.probed = 1;

  return init_serial_bitscope();
}

static int nchans(void)
{
  if (! bs.found && ! bs.probed) open_bitscope();

  return bs.found ? 3 : 0;
}

static int
serial_fd(void)
{
  return bs.found ? bs.fd : -1;
}

static Signal * bs_chan(int chan)
{
  switch (chan) {
  case 0:
    return &analogA_signal;
  case 1:
    return &analogB_signal;
  case 2:
    return &digital_signal;
  default:
    return NULL;
  }
}

static void reset(void)
{
  bs.probed = 0;
}

/* get pending available data from FD, or initiate new data collection */
static int bs_getdata(void)
{
  static unsigned char *buff;
  static int alt = 0, k, n;
  int fd = bs.fd;

  if (!fd) return(0);		/* device open? */
  if (in_progress == 'M') {	/* finish a get */
    if ((n = bs_io(fd, "M", bs.buf))) {
      buff = bs.buf;
      if (bs.version >= 110) { /* M mode, simple bytes */
	while (buff < bs.buf + n) {
	  if (k >= MAXWID)
	    break;
	  if (k > 8192)
	    alt = 1;
	  digital_signal.data[k] = *buff++ - 128;
	  if (alt == 0) analogA_signal.data[k++] = *buff++ - 128;
	  else analogB_signal.data[k++] = *buff++ - 128;
	}
	analogA_signal.num = k > 8192 ? 8192 : k;
	if (k > 8192) analogB_signal.num = k - 8192;
	digital_signal.num = k;
      } else {			/* S mode, hex ASCII */
	while (*buff != '\0') {
	  if (k >= MAXWID)
	    break;
	  if (*buff == '\r' || *buff == '\n')
	    buff++;
	  else {
	    n = strtol(buff, NULL, 16);
	    if (alt == 0) analogA_signal.data[k] = (n & 0xff) - 128;
	    else analogB_signal.data[k] = (n & 0xff) - 128;
	    digital_signal.data[k] = ((n & 0xff00) >> 8) - 128;
	    buff += 5;
	    if (alt) k++;
	    alt = !alt;
	  }
	}
	/* XXX this code is different from 'M', shouldn't it be the same? */
	analogA_signal.num = k;
	analogB_signal.num = k;
	digital_signal.num = k;
      }
      if (k >= samples(analogA_signal.rate) || k >= 16 * 1024) { /* all done */
//      if (k >= 8192 + samples(mem[23].rate) || k >= 16 * 1024) {
	k = 0;
	alt = 0;
	in_progress = 0;
      } else {		/* still need more, start another */
	bs_io(fd, "M", bs.buf);
      }
    }
  } else if (in_progress == 'T') { /* finish a trigger */
    if (bs_io(fd, "T", bs.buf)) {
      fprintf(stderr, "%s", bs.buf);
      bs_io(fd, "M", bs.buf);	/* start a get */
      analogA_signal.num = 0;
      analogA_signal.frame ++;
      analogB_signal.num = 0;
      analogB_signal.frame ++;
      digital_signal.num = 0;
      digital_signal.frame ++;
      k = 0;
    } else
      return(0);
  } else {			/* start a trigger */
    return(bs_io(fd, ">T", bs.buf));
  }
  return(1);
}

static char * status_str(int i)
{
  switch (i) {
  case 0:
    if (bs.probed && !bs.found) {
      return split_field(serial_error, 0, 16);
    } else {
      return bs.bcid;
    }
  case 2:
    if (bs.probed && !bs.found) {
      return split_field(serial_error, 1, 16);
    } else {
      return NULL;
    }
  default:
    return NULL;
  }
}

DataSrc datasrc_bs = {
  "BitScope",
  nchans,
  bs_chan,
  NULL, /* set_trigger, */
  NULL, /* clear_trigger, */
  changerate,
  NULL,		/* set_width */
  reset,
  serial_fd,
  bs_getdata,
  status_str,
  NULL, /* option1, */
  NULL, /* option1str, */
  NULL, /* option2, */
  NULL, /* option2str, */
  parse_option,
  save_option,
  bitscope_dialog,
};
