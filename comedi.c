/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Author: Brent Baccala <baccala@freesoft.org>
 *
 * Public domain.
 *
 * This file implements the COMEDI interface for xoscope
 *
 * The capturing occurs in what a normal oscilloscope would call "chop mode" - samples are
 * alternated between the various channels being captured.  This has the effect of reducing the
 * overall sampling rate by a factor equal to the number of channels being captured.  You could also
 * (but we don't) implement an "alt mode" - an entire sweep is taken from one channel, then the
 * entire next sweep is taken from the next channel, etc.  Triggering would be a problem.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <comedilib.h>
#include "xoscope.h"            /* program defaults */
#include "func.h"

#define COMEDI_RANGE 0          /* XXX user should set this */

/* Some of these variables aren't defined static because we need to get at them from our GTK dialog
 * callbacks.  None of them are set from there, that's done via set_option(), but there are read,
 * and for that reason need to be global.
 */

comedi_t *comedi_dev = NULL;
gchar *comedi_board_name = NULL;

static int comedi_opened = 0;   /* t if open has at least been _attempted_ */
static int comedi_running = 0;
static int comedi_error = 0;

/* device_name[] is an array we write the COMEDI device into if it is set by an option (then point
 * device at device_name).  If there's no option, device stays pointing to the default -
 * /dev/comedi0
 */

static char device_name[256];
char *comedi_devname = "/dev/comedi0";

int comedi_subdevice = 0;

int comedi_rate = 50000;        /* XXX set this to max valid upon open */

/* Size of COMEDI's kernel buffer.  -1 means leave it at the default.  Slight bug - if you change
 * it, then change it back to -1, the default setting won't return until you close and re-open the
 * device.  Actually, that might not be good enough - you might need to re-configure the device.
 */

int comedi_bufsize = -1;

#define BUFSZ 1024
static sampl_t buf[BUFSZ];
static int bufvalid=0;

// has to be set later
int zero_value = -1;

static int lag = 0;                     /* lag - see get_data() */

static int subdevice_flags = 0;
static int subdevice_type = COMEDI_SUBD_UNUSED;

int comedi_aref = AREF_GROUND;          /* Global voltage reference setting */

/* This structure associates xoscope Signal structures with the COMEDI channel they are receiving
 * on.  capture_list gets emptied by close_comedi(), added to by enable_chan(), deleted from by
 * disable_chan(), used by start_comedi_running() to build a channel list, and used by get_data() to
 * figure out in which Signal(s) to put the data.
 */

#define NCHANS 8

struct capture {
    struct capture * next;
    int chan;
    Signal * signal;
};

static struct capture *capture_list = NULL;

static int active_channels=0;

static Signal comedi_chans[NCHANS];

/* Triggering information - the (one) channel we're triggering on, the sample level, the type of
 * trigger (0 - freerun, 1 - ascending, 2 - descending), and the index of the trigger channel in the
 * capture list
 */

static int trig_chan = 0;
static int trig_level = 0;
static int trig_mode = 0;
static int trig_index = -1;

/* This function is defined as do-nothing and weak, meaning it can be overridden by the linker
 * without error.  It's used to start the X Windows GTK options dialog for COMEDI, and is defined in
 * this way so that this object file can be used either with or without GTK.  If this causes
 * compiler problems, just comment out the attribute line and leave the do-nothing function.  You
 * will then need to comment out both lines to generate an object file that can be used with GTK.
 */

void comedi_gtk_options() __attribute__ ((weak));
void comedi_gtk_options() {}

/* This function gets called at various points that we need to stop and restart COMEDI, like a rate
 * change, or a change to the list of channels we're capturing for.  start_comedi_running() gets
 * called automatically by get_data(), so we can use stop_comedi_running() pretty liberally.
 */

static void stop_comedi_running(void)
{
    if (comedi_running) {
        comedi_cancel(comedi_dev, 0);
        comedi_running = 0;
    }
    bufvalid = 0;
}

/* XXX This function should make sure the Signal arrays are reset to sane values.  Right now, it
 * just sets their volt and rate values.
 */

static int start_comedi_running(void)
{
    int try;
    int ret = -1;
    comedi_cmd cmd;
    unsigned int chanlist[NCHANS];
    struct capture *capture;
    comedi_range *comedi_rng;
    int maxdata;

    if (!comedi_dev) return 0;

    /* There might have been an error condition that was cleared (like switching from an unsupported
     * subdevice to a supported one), so clear comedi_error here and set it if there's a problem
     * later.  If we're not capturing anything, make sure we set subdevice_type before we return,
     * because nchans() depends on this variable to figure out how to interpret
     * comedi_get_n_channels()
     */

    comedi_error = 0;

    subdevice_flags = comedi_get_subdevice_flags(comedi_dev, comedi_subdevice);
    subdevice_type = comedi_get_subdevice_type(comedi_dev, comedi_subdevice);

    if (active_channels == 0) return 0;

    if ((comedi_bufsize > 0) && (comedi_get_version_code(comedi_dev) > 0x0748)) {
        /* comedi versions pre-0.7.73 have a bug in their buffer size handling.  Not only does it
         * fail to round up to a multiple of PAGE_SIZE correctly, but if you attempt to set a buffer
         * smaller than PAGE_SIZE, it will deallocate the buffer and you'll never get it back
         * without re-configuring the device.  I used to round up to PAGE_SIZE here to avoid the
         * bug, but that requires <asm/page.h> to be present on the system.  Easier is to just skip
         * setting buffer size on pre-0.7.73 comedi.
         */
        ret = comedi_set_buffer_size(comedi_dev, comedi_subdevice, comedi_bufsize);
        if (ret < 0) {
            comedi_error = comedi_errno();
            return ret;
        }
    }

    /* Now we build a COMEDI command structure */

    bzero(&cmd, sizeof(cmd));
    cmd.subdev = comedi_subdevice;

    /* Start with a channel list based on capture_list
     *
     * This code matches up with get_data(), which assumes that the captured data is in the same
     * order as the channels in the capture_list
     */

    cmd.chanlist = chanlist;
    cmd.chanlist_len = 0;

    for (capture = capture_list; capture != NULL; capture = capture->next) {
        chanlist[cmd.chanlist_len++] = CR_PACK(capture->chan,0,comedi_aref);
    }

    if (cmd.chanlist_len == 0) {
        return 0;
    }

    /* comedilib has a comedi_get_cmd_generic_timed() function, but it's set up for sampling a
     * single channel, so I don't use it.  Instead, I try several different varients on comedi
     * command structures in the hopes of finding one that works.
     */

    try = 0;
    do {

        switch (try) {

            /* The first thing we try is to simultaneously sample (that's the convert_src of
             * TRIG_NOW) all the channels at the requested rate.
             */

        case 0:
            cmd.start_src = TRIG_NOW;
            cmd.start_arg = 0;

            cmd.scan_begin_src = TRIG_TIMER;
            cmd.scan_begin_arg = 1e9 / comedi_rate;

            cmd.convert_src = TRIG_NOW;
            cmd.convert_arg = 0;

            cmd.scan_end_src = TRIG_COUNT;
            cmd.scan_end_arg = cmd.chanlist_len;

            cmd.stop_src = TRIG_NONE;
            cmd.stop_arg = 0;

            break;

            /* There's a good chance that won't work (not many cards support it).  So now try
             * sampling each channel at a staggered interval of the requested rate times the number
             * of channels.
             */

        case 1:
            cmd.start_src = TRIG_NOW;
            cmd.start_arg = 0;

            cmd.scan_begin_src = TRIG_FOLLOW;
            cmd.scan_begin_arg = 0;

            cmd.convert_src = TRIG_TIMER;
            cmd.convert_arg = 1e9 / (comedi_rate * active_channels);

            cmd.scan_end_src = TRIG_COUNT;
            cmd.scan_end_arg = cmd.chanlist_len;

            cmd.stop_src = TRIG_NONE;
            cmd.stop_arg = 0;

            break;

            /* OK, that didn't work.  Maybe the card wants timers on both the scan and conversion? */

        case 2:
            cmd.start_src = TRIG_NOW;
            cmd.start_arg = 0;

            cmd.scan_begin_src = TRIG_TIMER;
            cmd.scan_begin_arg = 1e9 / comedi_rate;

            cmd.convert_src = TRIG_TIMER;
            cmd.convert_arg = 1e9 / (comedi_rate * active_channels);

            cmd.scan_end_src = TRIG_COUNT;
            cmd.scan_end_arg = cmd.chanlist_len;

            cmd.stop_src = TRIG_NONE;
            cmd.stop_arg = 0;

            break;

            /* Nothing we tried worked!  There are other possibilities, but none are currently
             * supported by this code.  Complain and return the error code from the last thing we
             * tried.
             */

        default:

            comedi_error = comedi_errno();
            return ret;

        }

        /* COMEDI command testing can be a little funky.  We get a return code indicating which
         * phase of test failed.  Basically, if phase 1 or 2 failed, we're screwed.  If phase 3
         * failed, it might be because we've pushed the limits of the timing past where it can go,
         * and if phase 4 failed, it's just because the device can't support exactly the timings we
         * asked for.  In either of the last two cases, the driver adjusts the offending parameters.
         * That's why we call this function three times.
         */

        ret = comedi_command_test(comedi_dev,&cmd);
        ret = comedi_command_test(comedi_dev,&cmd);
        ret = comedi_command_test(comedi_dev,&cmd);

        try ++;

    } while (ret != 0);

    /* Now we adjust our global rate to whatever we got the card to do. */

    if (cmd.scan_begin_src == TRIG_TIMER) {
        comedi_rate = 1e9 / cmd.scan_begin_arg;
    } else if (cmd.convert_src == TRIG_TIMER) {
        comedi_rate = 1e9 / cmd.convert_arg;
        comedi_rate /= active_channels;
    } else {
        fprintf(stderr, "neither convert_src nor start_src is TRIG_TIMER!?!\n");
    }

    /* Voltage range is currently a global setting.  Find it, and save it into all the Signal(s)
     * we're collecting data into (if we're capturing an analog input subdevice; digital subdevs
     * don't do this).  Signal->volts should be in milivolts per 320 sample values, so take the
     * voltage range given by COMEDI, multiply by 1000 (volts -> millivolts), divide by 2^(sampl_t
     * bits) (sample values in an sampl_t), to get millivolts per sample value, and multiply by 320
     * to get millivolts per 320 sample values.  320 is the size of the vertical display area, in
     * case you wondered.
     *
     * Also, set the rate (samples/sec) at which we'll capture data
     */

    for (capture = capture_list; capture != NULL; capture = capture->next) {

        if (subdevice_type == COMEDI_SUBD_AI) {

            comedi_rng = comedi_get_range(comedi_dev,
                                          comedi_subdevice,
                                          capture->chan, COMEDI_RANGE);
            maxdata=comedi_get_maxdata(comedi_dev,
                                       comedi_subdevice,
                                       0);
            if (comedi_rng != NULL) {
                capture->signal->volts
                    = (comedi_rng->max - comedi_rng->min)
                    * 1000 * 320 / maxdata;
            }

            if (zero_value<0) {
                // we have to set zero value
                if ((comedi_rng != NULL) && (comedi_rng->min<0)&&(comedi_rng->max>0)) {
                    // we are bipolar
                    zero_value=maxdata/2;
                } else {
                    // we are unipolar
                    zero_value=0;
                }
            }

            capture->signal->bits = 0;

#if 0
            printf(" [%g,%g] %s\n",comedi_rng->min,comedi_rng->max,
                   comedi_rng->unit == UNIT_volt ? "V" : "");
#endif

        } else {

            capture->signal->bits = comedi_get_n_channels(comedi_dev, comedi_subdevice);

            capture->signal->volts = 0;

        }

        capture->signal->rate = comedi_rate;
    }

#if 0
    fprintf(stderr, "Sampling every %d(%d) ns(Hz)\n",
            cmd.convert_arg, comedi_rate);
#endif

    ret = comedi_command(comedi_dev,&cmd);
    if (ret >= 0) {
        fcntl(comedi_fileno(comedi_dev), F_SETFL, O_NONBLOCK);
        comedi_running = 1;
    } else {
        comedi_error = comedi_errno();
    }
    return ret;
}

static void close_comedi()
{
#if 0
    struct capture *capture;
#endif

    if (comedi_dev) comedi_close(comedi_dev);
    comedi_dev = NULL;
    if (comedi_board_name) g_free(comedi_board_name);
    comedi_board_name = NULL;
    comedi_running = 0;
    comedi_opened = 0;

    /* Leave active channels alone here in case we're closing a device because of an error and want
     * to re-open later.
     */

#if 0
    while (capture_list != NULL) {
        capture = capture_list->next;
        free(capture_list);
        capture_list = capture;
    }

    active_channels = 0;
#endif
}

static int open_comedi(void)
{
    int i;
    static int once=0;

    close_comedi();
    comedi_error = 0;
    comedi_opened = 1;
    subdevice_flags = 0;
    subdevice_type = COMEDI_SUBD_UNUSED;

    if (!once) {

        /* XXX once is a kludge */

        /* Setup the Signal structures.  Note that the name is set to 'a', 'b', 'c', etc, to conform
         * with xoscope usage and to avoid confusing the user with the display channels.  COMEDI, of
         * course, numbers its channels 0, 1, 2, etc
         */

        for (i = 0 ; i < NCHANS ; i++) {                /* XXX hardwired at 8 */
            comedi_chans[i].data = NULL;
            comedi_chans[i].num = comedi_chans[i].frame = comedi_chans[i].volts = 0;
            comedi_chans[i].listeners = 0;
            //sprintf(comedi_chans[i].name, "Channel %d", i);
            sprintf(comedi_chans[i].name, "Channel %c", 'a' + i);
            comedi_chans[i].savestr[0] = 'a' + i;
            comedi_chans[i].savestr[1] = '\0';
        }

        once = 1;
    }

    comedi_dev = comedi_open(comedi_devname);

    if (! comedi_dev) {
        comedi_error = comedi_errno();
        return 0;
    }

    /* All the GTK stuff uses UTF8, and complains to stderr if it
     * doesn't get it.
     */

    comedi_board_name = g_locale_to_utf8(comedi_get_board_name(comedi_dev),
                                         -1, NULL, NULL, NULL);

    /* XXX I'd kinda like to do this here, but then the read() loop below (to get offset correction
     * for the DAQP) returns errors (-EAGAIN).  If do this later, and start_comedi_running() has
     * problems, then it might hang in get_data(), but I hope I've fixed that now...
     */
    /* fcntl(comedi_fileno(comedi_dev), F_SETFL, O_NONBLOCK); */

    if (comedi_board_name && strncmp(comedi_board_name, "DAQP", 4) == 0) {

        /* Special case for DAQP - unfortunately, COMEDI doesn't (yet) provide a generic interface
         * for boards that can do offset correction, so this special case is designed to handle the
         * Quatech DAQP.  We collect a hundred samples from channel 4 in differential mode, a
         * non-existant channel used by the DAQP specifically for offset correction.
         */

        comedi_cmd cmd;
        unsigned int chan;
        int ret;

#ifdef COMEDI_GET_CMD_GENERIC_TIMED_TAKES_5_ARGS
        ret = comedi_get_cmd_generic_timed(comedi_dev, comedi_subdevice, &cmd,0,0);
#else
        ret = comedi_get_cmd_generic_timed(comedi_dev, comedi_subdevice, &cmd, 0);
#endif

        if (ret >= 0) {
            chan = CR_PACK(4,0,AREF_DIFF);
            cmd.chanlist = &chan;
            cmd.chanlist_len = 1;
            cmd.start_src = TRIG_NOW;
            cmd.start_arg = 0;
            cmd.stop_src = TRIG_COUNT;
            cmd.stop_arg = 100;

            ret = comedi_command_test(comedi_dev, &cmd);

            if (ret >= 0) {
                ret = comedi_command(comedi_dev, &cmd);
                if (ret >= 0) {
                    int i = 0;
                    while ((i < (100 * sizeof(sampl_t)))
                           && (ret = read(comedi_fileno(comedi_dev), buf,
                                          100 * sizeof(sampl_t) - i)) > 0) {
                        i += ret;
                    }
                    if (i == (100 * sizeof(sampl_t))) {
                        zero_value = 0;
                        for (i=0; i<100; i++) zero_value += buf[i];
                        zero_value /= 100;
                    }
                }
            }
        }

        if (ret == -1) {
            comedi_error = comedi_errno();
            /* close_comedi(); */
            return 0;
        }

        comedi_cancel(comedi_dev, 0);
    }

    /* XXX Why can't we do this here?  It doesn't seem to "take" */
    /* fcntl(comedi_fileno(comedi_dev), F_SETFL, O_NONBLOCK); */

    if (start_comedi_running() < 0) {
        return 0;
    } else {
        return 1;
    }

}

void reset_comedi(void)
{
    if (comedi_dev == NULL) {
        open_comedi();
        if (comedi_dev == NULL) {
            return;
        }
    }

    stop_comedi_running();
}

static int nchans(void)
{
    int chans;
    int i;

    if (! comedi_opened) open_comedi();

    /* open_comedi() calls start_comedi_running() just before it returns, so that's how we know
     * subdevice_type has been set correctly when we get here.  However, I really don't know if
     * open_comedi() SHOULD call start_comedi_running() at all, so we may need to revisit this.
     */

    if (comedi_dev == NULL) {
        return 0;
    } else if (subdevice_type == COMEDI_SUBD_AI) {
        /* analog subdevice - mark all channels analog and return num of chans */
        chans = comedi_get_n_channels(comedi_dev, comedi_subdevice);
        for (i = 0; i < chans; i ++) comedi_chans[i].bits = 0;
        return chans;
    } else {
        /* digital subdevice - n_channels returns number of bits */
        comedi_chans[0].bits = comedi_get_n_channels(comedi_dev, comedi_subdevice);
        return 1;
    }
}

static int fd(void)
{
    return (comedi_running ? comedi_fileno(comedi_dev) : -1);
}

/* reset() - part of the data source API.  Called when we're ready to start capturing.  Clears the
 * old capture_list and builds a new one.  capture_ptr is used to make sure we build the list from
 * the top down, not the bottom up, mainly to make sure trig_index counts from the top down.
 * Finally, we start COMEDI.  We don't really need to start COMEDI, just prep it, but we start it in
 * order to set the rate and volts fields (during start_comedi_running) in the Signal structures.
 */

static void reset(void)
{
    struct capture *capture;
    struct capture **capture_ptr;
    int i;

    stop_comedi_running();

    for (capture = capture_list; capture != NULL; capture = capture_list) {
        capture_list = capture->next;
        free(capture);
    }

    capture_list = NULL;
    active_channels = 0;
    trig_index = -1;
    capture_ptr = &capture_list;

    for (i = 0; i < NCHANS; i++) {
        if ((comedi_chans[i].listeners) || ((trig_mode > 0) && (trig_chan == i))) {

            capture = malloc(sizeof(struct capture));
            if (capture == NULL) {
                perror("enable_chan() malloc failed");
                exit(1);
            }

            capture->chan = i;
            capture->signal = &comedi_chans[i];
            capture->next = NULL;
            *capture_ptr = capture;
            capture_ptr = &capture->next;

            comedi_chans[i].num = 0;
            comedi_chans[i].frame ++;

            if ((trig_mode > 0) && (trig_chan == i)) trig_index = active_channels;

            active_channels ++;
        }
    }

    start_comedi_running();
}

static Signal * comedi_chan(int chan)
{
    return &comedi_chans[chan];
}

static int set_trigger(int chan, int *levelp, int mode)
{
    trig_chan = chan;
    trig_level = *levelp;
    trig_mode = mode;
    /* XXX check that trig_level is within subdevice's range */
    return 1;
}

static void clear_trigger(void)
{
    trig_mode = 0;
}

/* Current COMEDI rate logic has some bizarre effects.  As we increase the number of channels
 * sampled, the rate goes down (usually), but doesn't go back up when we decrease the number of
 * sampled chans.
 */

/* XXX the rate we calculate might not be the one that actually gets used */

static int change_rate(int dir)
{
    int oldrate = comedi_rate;

    if (dir == 1) {
        comedi_rate *= 2;
    } else {
        comedi_rate /= 2;
    }

    stop_comedi_running();

    return (comedi_rate != oldrate);
}

/* set_width(int)
 *
 * sets the frame width (number of samples captured per sweep) globally for all the channels.
 */

static void set_width(int width)
{
    int i;

    for (i=0; i<NCHANS; i++) {
        comedi_chans[i].width = width;
        if (comedi_chans[i].data != NULL) free(comedi_chans[i].data);
        comedi_chans[i].data = malloc(width * sizeof(short));
    }
}

/* get_data() -
 * read all available data from comedi device, return value is TRUE if we actually put some samples
 * into the sweep buffer (and thus need a redisplay)
 *
 * This function should always return at the end of a sweep
 */

#define convert(sample) (sample - zero_value)

static int get_data(void)
{
    int bytes_read;
    int samples_read;
    int scans_read;
    int samples_per_frame;
    sampl_t *current_scan, *last_scan;
    int i, j;
    int delay;
    int triggered=0;
    int was_in_sweep=in_progress;
    static struct timeval tv1, tv2;
    struct capture *capture;

    /* This code used to try and start COMEDI running if it wasn't running already.  But if fd()
     * already returned -1, the main code doesn't think we're running, so it's best to leave things
     * alone here...
     */

    if (! comedi_dev || ! comedi_running) return 0;

    /* The way the code's written right now, all the channels are sampled at the same rate and for
     * the same width (number of samples per frame), so we just use the width from the first channel
     * in the capture list to figure how many samples we're capturing.
     */

    samples_per_frame = capture_list->signal->width;

    /* It is possible for this loop to be entered with a full buffer of data already (bufvalid ==
     * sizeof(buf)).  In that case, the read will be called with a zero byte buffer size, and will
     * return zero.  That's why the comparison reads ">=0" and not ">0"
     */
    while ((bytes_read = read(comedi_fileno(comedi_dev),
                              ((char *)buf) + bufvalid, sizeof(buf) - bufvalid))
           >= 0) {

        // fprintf(stderr, "bytes_read=%d; bufvalid=%d\n", bytes_read, bufvalid);

        bytes_read += bufvalid;

        gettimeofday(&tv1, NULL);
        samples_read = bytes_read / sizeof(sampl_t);
        scans_read = samples_read / active_channels;

        /* This is here to catch the case when there's nothing (or not much) in the buffer, and the
         * read() call returned nothing.
         */

        if (scans_read == 0 && bytes_read == 0) break;

        for (i = 0; i < scans_read; i++) {

            current_scan = buf + i * active_channels;

            if (!in_progress && scope.run && i>0) {

                /* Sweep isn't in_progress, so look for a trigger - anything (trig_mode==0) or a
                 * transition between the last sample and the current one that crossed the
                 * trig_level threshold, either going positive (trig_mode==1) or going negative
                 * (trig_mode==2).  Since we check the previous sample, there's an "i>0" case in the
                 * above if statement, and that does mean that we'll miss a trigger if the
                 * transition exactly corresponds with a read buffer boundary.
                 */

                last_scan = buf + (i-1) * active_channels;

                if ((trig_mode == 0) ||
                    ((trig_mode == 1) &&
                     (convert(current_scan[trig_index]) >= trig_level) &&
                     (convert(last_scan[trig_index]) < trig_level)) ||
                    ((trig_mode == 2) &&
                     (convert(current_scan[trig_index]) <= trig_level) &&
                     (convert(last_scan[trig_index]) > trig_level))) {

                    /* found something to trigger on, so compute a delay value based on
                     * extrapolating a straight line between the two sample values that straddle the
                     * triggering point, for high-frequency signals that change significantly
                     * between the two samples.  Set up all the relevent Signal structures, then
                     * fall through into the triggered case below
                     */

                    delay = 0;

                    if (trig_mode != 0) {
                        short current = convert(current_scan[trig_index]);
                        short last = convert(last_scan[trig_index]);
                        if (current != last) {
                            delay = abs(10000 * (current - trig_level) / (current - last));
                        }
                    }

                    for (j=0, capture=capture_list;
                         capture != NULL; capture=capture->next, j++) {
                        capture->signal->frame ++;
                        capture->signal->delay = delay;
                        capture->signal->num = 0;
                    }
                    if (j != active_channels) {
                        fprintf(stderr, "ERROR!   j != active_channels in get_data()\n");
                    }

                    in_progress = 1;

                }
            }

            if (in_progress) {

                /* Sweep in progress */

                for (j=0, capture=capture_list; capture != NULL; capture=capture->next, j++) {
                    capture->signal->data[capture->signal->num ++] = convert(current_scan[j]);
                    in_progress = capture->signal->num;
                }
                if (j != active_channels) {
                    fprintf(stderr, "ERROR!   j != active_channels in get_data()\n");
                }

                triggered = 1;

                if (in_progress >= samples_per_frame) {

                    in_progress = 0;

                    /* If we were in the middle of a sweep when we entered this function, return
                     * now.  Otherwise, keep looking for more sweeps.
                     */

                    if (was_in_sweep) {

                        bufvalid = bytes_read - (i * active_channels * sizeof(sampl_t));
                        if (bufvalid) {
                            memcpy(buf, (char *) buf + bytes_read - bufvalid, bufvalid);
                        }

                        lag = 0;
                        return triggered;
                    }
                }

            }
        }

        /* It would be nice if COMEDI never returned a partial scan to a read() call.
         * Unfortunately, it often does, so we need to tuck the "extra" data away until the next
         * time through this loop...
         */

        bufvalid = bytes_read - (scans_read * active_channels * sizeof(sampl_t));
        if (bufvalid) {
            memcpy(buf, (char *) buf + bytes_read - bufvalid, bufvalid);
        }

    }

    if ((bytes_read < 0) && (errno != EAGAIN)) {

        /* The most common cause of a COMEDI read error is a buffer overflow.  There are all kinds
         * of ways to do it, from hitting space to stop the scope trace to dragging a window while
         * xoscope is running.  In the later case, the window manager will do an X server grab,
         * which will block xoscope the next time it tries to perform an X operation.  Holding the
         * mouse down longer than a split second will cause the COMEDI kernel buffer to overflow,
         * which will trigger this code the next time through.
         *
         * comedi-0.7.60 returned EINVAL on buffer overflows;
         * comedi-0.7.66 returns EPIPE
         *
         * In any event, if we detect an overflow, we reset the capture to start a new trace, record
         * how many microseconds elapsed since the last time we were able to read the device, and
         * report this on the screen as "lag".
         */

        if (errno != EINVAL && errno != EPIPE) perror("comedi read");

        start_comedi_running();
        bufvalid = 0;
        gettimeofday(&tv2, NULL);
        lag = 1000000*(tv2.tv_sec-tv1.tv_sec) + tv2.tv_usec - tv1.tv_usec;
        return 0;
    }

    lag = 0;
    return triggered;
}


static const char * status_str(int i)
{
    static char buffer[16];
    const char *error = comedi_strerror(comedi_error);

    switch (i) {
    case 0:
        return comedi_devname;
    case 2:
        if (comedi_dev) {
            return comedi_board_name;
        } else {
            return split_field(error, 0, 16);
        }

    case 4:
        if (!comedi_dev) {
            return split_field(error, 1, 16);
        } else {
            return "";
        }

    case 1:
        if (comedi_dev) {
            sprintf(buffer, "Subdevice %d", comedi_subdevice);
            return buffer;
        } else {
            return "";
        }
    case 3:
        if (comedi_dev && comedi_error) {
            return split_field(error, 0, 16);
        } else if (lag > 1000) {
            snprintf(buffer, sizeof(buffer), "%d ms lag", lag/1000);
        } else if (lag > 0) {
            snprintf(buffer, sizeof(buffer), "%d \302\265s lag", lag);
        } else {
            return "";
        }
    case 5:
        if (comedi_dev && comedi_error) {
            return split_field(error, 1, 16);
        } else {
            return "";
        }

    default:
        return NULL;
    }
}


/* Option 1 key - global analog reference toggle
 *
 * Analog COMEDI devices typically can select between different references (which signal line is
 * treated as zero point).
 */

static int option1(void)
{
    if (comedi_aref == AREF_GROUND && subdevice_flags & SDF_DIFF)
        comedi_aref = AREF_DIFF;
    else if (comedi_aref == AREF_GROUND && subdevice_flags & SDF_COMMON)
        comedi_aref = AREF_COMMON;
    else if (comedi_aref == AREF_DIFF && subdevice_flags & SDF_COMMON)
        comedi_aref = AREF_COMMON;
    else if (comedi_aref == AREF_DIFF && subdevice_flags & SDF_GROUND)
        comedi_aref = AREF_GROUND;
    else if (comedi_aref == AREF_COMMON && subdevice_flags & SDF_GROUND)
        comedi_aref = AREF_GROUND;
    else if (comedi_aref == AREF_COMMON && subdevice_flags & SDF_DIFF)
        comedi_aref = AREF_DIFF;
    else
        return 0;

    return 1;
}

static const char * option1str(void)
{
    if (! (subdevice_flags & (SDF_GROUND | SDF_DIFF | SDF_COMMON))) {
        return NULL;
    } else if (comedi_aref == AREF_GROUND) {
        return "AREF_GROUND";
    } else if (comedi_aref == AREF_DIFF) {
        return "AREF_DIFF";
    } else if (comedi_aref == AREF_COMMON) {
        return "AREF_COMMON";
    } else {
        return "AREF unknown";
    }
}


#if 0

/* XXX Option 2 key - use this for a per-channel Range setting?
 *
 * If so, would also need to add option strings below.
 */


static int option2(void)
{
    return 0;
}

static char * option2str(void)
{
    return NULL;
}

#endif

static int comedi_set_option(char *option)
{
    char buf[256];
    char *p = buf;

    do {
        *p++ = tolower(*option);
    } while (*option++ && p < buf + sizeof(buf));

    if (sscanf(buf, "rate=%d", &comedi_rate) == 1) {
        reset_comedi();
        return 1;
    } else if (sscanf(buf, "device=%s", device_name) == 1) {
        comedi_devname = device_name;
        close_comedi();
        /* reset_comedi(); */
        open_comedi();
        return 1;
    } else if (sscanf(buf, "subdevice=%d", &comedi_subdevice) == 1) {
        reset_comedi();
        return 1;
    } else if (strcasecmp(buf, "aref=ground") == 0) {
        comedi_aref = AREF_GROUND;
        reset_comedi();
        return 1;
    } else if (strcasecmp(buf, "aref=diff") == 0) {
        comedi_aref = AREF_DIFF;
        reset_comedi();
        return 1;
    } else if (strcasecmp(buf, "bufsize=default") == 0) {
        comedi_bufsize = -1;
        return 1;
    } else if (sscanf(buf, "bufsize=%d", &comedi_bufsize) == 1) {
        reset_comedi();
        return 1;
    } else {
        return 0;
    }
}

static char * comedi_save_option(int i)
{
    static char buf[256];

    switch (i) {
    case 0:
        snprintf(buf, sizeof(buf), "rate=%d", comedi_rate);
        return buf;

    case 1:
        snprintf(buf, sizeof(buf), "device=%s", comedi_devname);
        return buf;

    case 2:
        snprintf(buf, sizeof(buf), "subdevice=%d", comedi_subdevice);
        return buf;

    case 3:
        switch (comedi_aref) {
        case AREF_GROUND:
            return "aref=ground";
        case AREF_DIFF:
            return "aref=diff";
        case AREF_COMMON:
            return "aref=common";
        default:
            return "";
        }

    case 4:
        if (comedi_bufsize > 0) {
            snprintf(buf, sizeof(buf), "bufsize=%d", comedi_bufsize);
            return buf;
        } else {
            return "bufsize=default";
        }

    default:
        return NULL;
    }
}

DataSrc datasrc_comedi = {
    "COMEDI",
    nchans,
    comedi_chan,
    set_trigger,
    clear_trigger,
    change_rate,
    set_width,
    reset,
    fd,
    get_data,
    status_str,
    option1,
    option1str,
#if 0
    option2,
    option2str,
#else
    NULL,
    NULL,
#endif
    comedi_set_option,
    comedi_save_option,
    comedi_gtk_options,
};
