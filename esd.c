/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 * Copyright (C) 2014 Gerhard Schiller <gerhard.schiller@gmail.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the Linux ESD interfaces
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>             /* for abs() */
#include <sys/ioctl.h>
#include "xoscope.h"            /* program defaults */
#include <esd.h>

#define ESDDEVICE "ESounD"

static int esd = -2;            /* file descriptor for ESD */

static int esdblock = 0;        /* 1 to block ESD; 0 to non-block */
static int esdrecord = 1;       /* 1 to use ESD record mode; 0 to use ESD monitor mode */
                                /* monitor mode makes more sense; but doesn't seem to work right under PulseAudio */

/* Signal structures we're capturing into */
static Signal left_sig = {"Left Mix", "a"};
static Signal right_sig = {"Right Mix", "b"};

static int trigmode = 0;
static int triglev;
static int trigch;

static char * esd_errormsg1 = NULL;
static char * esd_errormsg2 = NULL;

/* This function is defined as do-nothing and weak, meaning it can be
 * overridden by the linker without error.  It's used for the X
 * Windows GUI for this data source, and is defined in this way so
 * that this object file can be used either with or without GTK.  If
 * this causes compiler problems, just comment out the attribute lines
 * and leave the do-nothing functions.
 */

void esd_gtk_option_dialog() __attribute__ ((weak));
/*void esdsc_gtk_option_dialog() {}*/

static void close_ESD(void)
{
    if (esd >= 0) {
        close(esd);
        esd = -2;
    }
}

static int open_ESD(void)
{
    if (esd >= 0) {
        return 1;
    }

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
    if (!esdblock) {
        fcntl(esd, F_SETFL, O_NONBLOCK);
    }

    return 1;
}

static void reset_sound_card(void)
{
    static char junk[SAMPLESKIP];

    if (esd >= 0) {

        close_ESD();
        open_ESD();

        if (esd < 0) return;

        read(esd, junk, SAMPLESKIP);

        return;
    }

}

static int esd_nchans(void)
{
    /* ESD always has two channels, right? */

    if (esd == -2) {
        open_ESD();
    }

    return (esd >= 0) ? 2 : 0;
}

static int fd(void)
{
    return esd;
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
    /* ESD doesn't support changing sampling rate */
    return 0;
}

static void reset(void)
{
    reset_sound_card();

    left_sig.rate = esd >= 0 ? ESD_DEFAULT_RATE : 0;
    right_sig.rate = esd >= 0 ? ESD_DEFAULT_RATE : 0;

    left_sig.num = 0;
    left_sig.frame ++;

    right_sig.num = 0;
    right_sig.frame ++;

    left_sig.volts = 0;
    right_sig.volts = 0;

    in_progress = 0;
}

/* set_width(int)
 *
 * sets the frame width (number of samples captured per sweep) globally
 * for all the channels.
 */

static void set_width(int width)
{
    /* fprintf(stderr, "set_width(%d)\n", width); */
    left_sig.width = width;
    right_sig.width = width;

    if (left_sig.data != NULL) free(left_sig.data);
    if (right_sig.data != NULL) free(right_sig.data);

    left_sig.data = malloc(width * sizeof(short));

    if(left_sig.data == NULL){
        fprintf(stderr, "set_width(), malloc failed, %s\n", strerror(errno));
        exit(0);
    }
    right_sig.data = malloc(width * sizeof(short));
    if(right_sig.data == NULL){
        fprintf(stderr, "set_width(), malloc failed, %s\n", strerror(errno));
        exit(0);
    }
}

/* get data from sound card, return value is whether we triggered or not */
static int esd_get_data(void)
{
    static unsigned char buffer[MAXWID * 2];
    static int i, j, delay;
    int fd;

    if (esd >= 0) {
        fd = esd;
    } else {
        return 0;
    }

    if (!in_progress) {
        /* Discard excess samples so we can keep our time snapshot close to real-time and minimize
         * sound recording overruns.  For ESD we don't know how many are available (do we?) so we
         * discard them all to start with a fresh buffer that hopefully won't wrap around before we
         * get it read.
         */

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

        if ((i+1)*2 > j) {      /* haven't triggered within the screen */
            return 0;           /* give up and keep previous samples */
        }

        delay = 0;

        if (trigmode) {
            int last = buffer[2*(i-1) + trigch] - 127;
            int current = buffer[2*i + trigch] - 127;
            if (last != current) {
                delay = abs(10000 * (current - (triglev - 127)) / (current - last));
            }
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
        if (in_progress >= left_sig.width) {
            break;
        }
#if 0
        if (*buff == 0 || *buff == 255) {
            clip = i % 2 + 1;
        }
#endif
        left_sig.data[in_progress] = buffer[2*i] - 127;
        right_sig.data[in_progress] = buffer[2*i + 1] - 127;

        in_progress ++;
        i ++;
    }

    left_sig.num = in_progress;
    right_sig.num = in_progress;

    if (in_progress >= left_sig.width) {
        in_progress = 0;
    }

    return 1;
}

static const char * esd_status_str(int i)
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
    if (esdrecord) {
        esdrecord = 0;
    } else {
        esdrecord = 1;
    }

    return 1;
}

static const char * option1str_esd(void)
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
    esd_get_data,
    esd_status_str,
    option1_esd,  /* option1 */
    option1str_esd,  /* option1str */
    NULL,  /* option2, */
    NULL,  /* option2str, */
    esd_set_option,  /* set_option */
    esd_save_option,  /* save_option */
    esd_gtk_option_dialog,  /* gtk_options */
};
