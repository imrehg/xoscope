/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 * Copyright (C) 2014 Gerhard Schiller <gerhard.schiller@gmail.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the Linux ALSA sound card interface
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>             /* for abs() */
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <linux/soundcard.h>
#include "xoscope.h"            /* program defaults */

char    alsaDevice[32] = "\0";

/* If you want to xoscope to display volts simply set the global variable alsa_volts
   * as described below:
   *
   * Step 1:
   * Determine the peak-peak voltage for a full swing (-128 to +127 steps in 8-bit mode
   * or -32768 to 32767 steps in 16-bit mode).
   * Hint:
   * If you can only measure the rms of a signal, apply a sine wave
   * (e.g. mains voltage via a transformer),then calculate the peak-peak voltage:
   * Volts[pp] = Volts[rms] * sqrt(2) * 2.
   *
   * Step 2:
   * Calculate alsa_volts from the peak-peak voltage:
   *  8-bit mode: alsa_volts = V[pp] * 1000mV/V * 320 / 255
   * 16-bit mode: alsa_volts = V[pp] * 1000mV/V * 320 / 65535
   *
   * In case you are curious why we multiply by 320, tThe explanation for the voltage range
   * in comedi.c says:
   *    Signal->volts should be in milivolts per 320 sample values,
   *    so take the voltage range given by COMEDI,
   *    multiply by 1000 (volts -> millivolts),
   *    divide by 2^(sampl_t bits) (sample values in an sampl_t), to get mV per sample value, and
   *    multiply by 320 to get millivolts per 320 sample values.
   *
   *    320 is the size of the vertical display area, in case you wondered....
   */
double  alsa_volts = 0.0;

static snd_pcm_t *handle        = NULL;
snd_pcm_format_t pcm_format = 0;

static int sc_chans = 0;
static int sound_card_rate = DEF_R;     /* sampling rate of sound card */

/* Signal structures we're capturing into */
static Signal left_sig = {"Left Mix", "a"};
static Signal right_sig = {"Right Mix", "b"};

static int trigmode = 0;
static int triglev;
static int trigch;

static const char * snd_errormsg1 = NULL;
static const char * snd_errormsg2 = NULL;

/* This function is defined as do-nothing and weak, meaning it can be overridden by the linker
 * without error.  It's used for the X Windows GUI for this data source, and is defined in this way
 * so that this object file can be used either with or without GTK.  If this causes compiler
 * problems, just comment out the attribute lines and leave the do-nothing functions.
 */
void alsa_gtk_option_dialog() __attribute__ ((weak));

/* close the sound device */
static void close_sound_card(void)
{
    if (handle != NULL) {
        snd_pcm_drop(handle);
        snd_pcm_hw_free(handle);
        snd_pcm_close(handle);

        handle = NULL;
    }
}

static int open_sound_card(void)
{
    unsigned int rate = sound_card_rate;
    unsigned int chan = 2;
    int rc;
    snd_pcm_hw_params_t *params;
    int dir = 0;
    snd_pcm_uframes_t pcm_frames;
    int intervall_ms;

    if (handle != NULL){
        return 1;
    }

    snd_errormsg1 = NULL;
    snd_errormsg2 = NULL;

    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, alsaDevice, SND_PCM_STREAM_CAPTURE,  SND_PCM_NONBLOCK);
    if (rc < 0) {
        snd_errormsg1 = "opening ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    rc = snd_pcm_hw_params_any(handle, params);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_any() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    rc = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_set_access() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    /* Set and check format, i.e. bits per sample */
#if SC_16BIT
    /* Signed 16-bit little-endian format */
    rc = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    pcm_format = SND_PCM_FORMAT_S16_LE;
#else
    /* Unsigned 8-bit format */
    rc = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
    pcm_format = SND_PCM_FORMAT_U8;
#endif
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_set_format() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    rc = snd_pcm_hw_params_get_format(params, &pcm_format);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_get_format() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }
#if SC_16BIT
    if (pcm_format != SND_PCM_FORMAT_S16_LE) {
        snd_errormsg1 = "Can't set 16-bit format (SND_PCM_FORMAT_S16_LE)";
        return 0;
    }
#else
    if (pcm_format != SND_PCM_FORMAT_U8) {
        snd_errormsg1 = "Can't set 8-bit format (SND_PCM_FORMAT_U8)";
        return 0;
    }
#endif

    /* Two channels (stereo) */
    rc = snd_pcm_hw_params_set_channels(handle, params, chan);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_set_channels() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }
    rc = snd_pcm_hw_params_get_channels(params, &chan);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_get_channels() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }
    sc_chans = chan;

    rc = snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_set_rate_near() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }
    if (rate != sound_card_rate) {
        snd_errormsg1 = "requested sample rate not available ";
        return 0;
    }
    sound_card_rate = rate;

    /* Set period period size (measured in frames).
     * 
     * A period is the number of frames in between each hardware interrupt.
     * 
     * sound_card_rate is in Hz, that means we get "sound_card_rate" samples per second.
     * We query for samples at SND_QUERY_INTERVALL or scope.min_interval ms. 
     * So the frames buffer must hold at least: 
     *                      (sound_card_rate * interval) / 1000 frames.
     *
     * As we dont use interrup-style transfer, we could leave it to the alse driver
     * to choose the buffer size. 
     * But to be sure, we set a lower limit of 5 times the minimum value.
     */
    intervall_ms = 
            scope.min_interval > SND_QUERY_INTERVALL ? scope.min_interval : SND_QUERY_INTERVALL;
    pcm_frames = (sound_card_rate * intervall_ms ) / 200;
    rc = snd_pcm_hw_params_set_buffer_size_min(handle, params, &pcm_frames);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params_set_buffer_size_min() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        snd_errormsg1 = "snd_pcm_hw_params() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    if ((rc = snd_pcm_prepare (handle)) < 0) {
        snd_errormsg1 = "snd_pcm_prepare() failed ";
        snd_errormsg2 = snd_strerror(rc);
        return 0;
    }

    return 1;
}

static void reset_sound_card(void)
{
    static unsigned char *junk = NULL;

    if (junk == NULL) {
        if (!(junk =  malloc((SAMPLESKIP * snd_pcm_format_width(pcm_format) / 8) * 2))) {
            snd_errormsg1 = "malloc() failed " ;
            snd_errormsg2 = strerror(errno);
            return;
        }
    }

    if (handle != NULL) {
        close_sound_card();
        open_sound_card();

        if (handle == NULL) {
            return;
        }
        snd_pcm_readi(handle, junk, SAMPLESKIP);
    }
}

static int sc_nchans(void)
{
    if (handle == NULL) {
        open_sound_card();
    }
    return (handle != NULL) ? sc_chans : 0;
}

static int fd(void)
{
    return -1;
}

static Signal *sc_chan(int chan)
{
    return (chan ? &right_sig : &left_sig);
}

/* Triggering - we save the trigger level in the raw, unsigned byte values that we read from the
 * sound card
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

static void reset(void)
{
    reset_sound_card();

    left_sig.rate = sound_card_rate;
    right_sig.rate = sound_card_rate;

    left_sig.num = 0;
    left_sig.frame ++;

    right_sig.num = 0;
    right_sig.frame ++;

    left_sig.volts = alsa_volts;
    right_sig.volts = alsa_volts;

    in_progress = 0;
}

/* This is the buffer into wich we read the interleaved data from the soundcard.
 * Interleaved means the data is transfered in individual frames, 
 * where each frame is composed of a single sample from each channel. 
 *
 * The buffer is sized so that the data for a full sweep of the scope fits into it.
 * The number of samples for a full sweep depends on the time base and the sample rate 
 * of the scope.
 * It is stored in bufferSizeFrames (also equal to: left_sig/right_sig.width).
 * Therfore the size has to be recaluleted and the buffer realocated 
 * when the time base and/or the sample rate changes.
 */

#if SC_16BIT
static short *buffer = NULL;
#else
static unsigned char *buffer = NULL;
#endif
static int  bufferSizeFrames    = 0;    /* The size of the buffer,measured in Frames */

/* set_width(int)
 *
 * sets the frame width (number of samples captured per sweep) globally for all the channels.
 */

static void set_width(int width)
{
    left_sig.width = width;
    right_sig.width = width;
    bufferSizeFrames = width;

    if (left_sig.data != NULL) 
        g_free(left_sig.data);
    if (right_sig.data != NULL) 
        g_free(right_sig.data);
    
    left_sig.data = g_new0(short, width);
    right_sig.data = g_new0(short, width);
    
#if SC_16BIT
    if(buffer == NULL)
        buffer = g_new0(short, width * 2);
    else
        buffer = g_renew(short, buffer, width * 2);
#else
    if(buffer == NULL)
        buffer = g_new0(unsigned char, width * 2);
    else
        buffer = g_renew(unsigned char, buffer, width * 2);
#endif
}


/* get data from ALSA sound system, */
/* return value is 0 when we wait for a trigger event or on error, otherwise 1 */
/* in_progress: 0 when we start a new plot, when a plot is in progress, number of samples read. */

static int sc_get_data(void)
{
    int i, delay;
    int rdCnt, rdMax;           /* measured in frames ! */

    if (handle == NULL) {
        return 0;
    }

    rdMax = bufferSizeFrames - in_progress;
    if (!in_progress) {
        /* Discard excess samples so we can keep our time snapshot close to real-time and minimize
         * sound recording overruns.  For ESD we don't know how many are available (do we?) so we
         * discard them all to start with a fresh buffer that hopefully won't wrap around before we
         * get it read.
         */

        /* read until we get something smaller than a full buffer */
        while ((rdCnt = snd_pcm_readi(handle, buffer, bufferSizeFrames)) == bufferSizeFrames)
            ;
    } 
    else {
        rdCnt = snd_pcm_readi(handle, buffer, rdMax);
    }

    if (rdCnt < 0) {
        if (rdCnt == -EAGAIN) { /* EAGAIN means try again, i.e. no data available */
            return 0;
        }
        else if (rdCnt == -EPIPE) { /* EPIPE means overrun */
            snd_pcm_recover(handle, rdCnt, TRUE);
            snd_pcm_readi(handle, buffer, rdMax); // flush frame buffer
            usleep(1000);
            return sc_get_data();
        }
        else {
            snd_pcm_recover(handle, rdCnt, TRUE);
            snd_pcm_readi(handle, buffer, rdMax); // flush frame buffer
            usleep(1000);
            return 0;
        }
    }

    if (rdCnt < 0) {
        if (rdCnt == -EAGAIN) { /* EAGAIN means try again, i.e. not data available */
            return 0;
        }
        else if (rdCnt == -EPIPE) { /* EPIPE means overrun */
            snd_pcm_recover(handle, rdCnt, TRUE);
            snd_pcm_readi(handle, buffer, rdMax); // flush frame buffer
            usleep(1000);
            return sc_get_data();
        }
        else {
            snd_pcm_recover(handle, rdCnt, TRUE);
            snd_pcm_readi(handle, buffer, rdMax); // flush frame buffer
            usleep(1000);
            return 0;
        }
    }
    i = 0;
    if (!in_progress) {
        int trigger, val, prev, k;
#if SC_16BIT
    trigger = (triglev - 127) * 256;
#else
    trigger = (triglev - 0);
#endif
        if (trigmode == 1) {
            /* locate rising edges. Try to handle handle noise by looking at the next 10 values.
             * Might miss a trigger point close to the right edge of the screen
             */
#if SC_16BIT
            prev = SHRT_MAX;
#else
            prev = UCHAR_MAX;
#endif
            for (i = 0; i < rdCnt; i++) {
                val = buffer[2*i + trigch];
                if (val > trigger && prev <= trigger){
                    int rising = 0;
                    for(k = i + 1; k < i + 11 && k < rdCnt; k++) {
                        if(buffer[2*k + trigch] > val){
                            rising++;
                        }
                    }
                    if(rising > 5){
                        break;
                    }
                }
                prev = val;
            }
        }
        else if(trigmode == 2) {
            /* same for falling edges */
            prev = 0;
#if SC_16BIT
            prev = SHRT_MIN;
#else
            prev = 0;
#endif
            for (i = 0; i < rdCnt; i++) {
                val = buffer[2*i + trigch];
                if (val < trigger && prev >= trigger){
                    int falling = 0;
                    for(k = i + 1; k < i + 11 && k < rdCnt; k++) {
                        if(buffer[2*k + trigch] < val){
                            falling++;
                        }
                    }
                    if(falling > 5){
                        break;
                    }
                }
                prev = val;
            }
        }

        if (i >= rdCnt) {  /* haven't triggered within the screen */
             return 0;     /* give up */
        }

        /* The delay value calculated here is only used in on_databox_button_press_event()
         * But it seems on_databox_button_press_event() isn't associated with anything.
         * Most likely it was used in the now defunct code for "cursors"
         */
        delay = 0;

#if SC_16BIT
        left_sig.data[0] = buffer[2*i];
#else
        left_sig.data[0] = buffer[2*i] - 127;
#endif
        left_sig.delay = delay;
        left_sig.num = 1;
        left_sig.frame ++;

#if SC_16BIT
        right_sig.data[0] = buffer[2*i + 1];
#else
        right_sig.data[0] = buffer[2*i + 1] - 127;
#endif
        right_sig.delay = delay;
        right_sig.num = 1;
        right_sig.frame ++;

        i ++;
        in_progress = 1;
    }

    while (i < rdCnt) {
        if (in_progress >= left_sig.width) { // enough samples for a screen
            break;
        }
#if SC_16BIT
        left_sig.data[in_progress] = buffer[2*i];
        right_sig.data[in_progress] = buffer[2*i + 1];
#else
        left_sig.data[in_progress] = buffer[2*i] - 127;
        right_sig.data[in_progress] = buffer[2*i + 1] - 127;
#endif

        in_progress ++;
        i++;
    }

    left_sig.num = in_progress;
    right_sig.num = in_progress;

    if (in_progress >= left_sig.width) { // enough samples for a screen
        in_progress = 0;
    }
    return 1;
}

static const char * snd_status_str(int i)
{
#ifdef DEBUG
    static char string[16];
    sprintf(string, "status %d", i);
    return string;
#endif

    switch (i) {
    case 0:
        return alsaDevice;

    case 2:
        if (snd_errormsg1){
            return snd_errormsg1;
        } else {
            return "";
        }

    case 4:
        if (snd_errormsg2) {
            return snd_errormsg2;
        } else {
            return "";
        }
    }
    return NULL;
}

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

DataSrc datasrc_sc = {
    "ALSA",
    sc_nchans,
    sc_chan,
    set_trigger,
    clear_trigger,
    change_rate,
    set_width,
    reset,
    fd, //should be REALY unused....
    sc_get_data,
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
    NULL  /* gtk_options */
};

