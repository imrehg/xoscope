/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the basic oscilloscope internals
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "xoscope.h"            /* program defaults */
#include "display.h"            /* display routines */
#include "func.h"               /* signal math functions */
#include "file.h"               /* file I/O functions */

/* global program structures */
Scope scope;

#ifdef HAVE_LIBASOUND
extern DataSrc datasrc_sc;
#endif
#ifdef HAVE_LIBESD
extern DataSrc datasrc_esd;
#endif
#ifdef HAVE_LIBCOMEDI
extern DataSrc datasrc_comedi;
#endif

DataSrc *datasrcs[] = {
#ifdef HAVE_LIBCOMEDI
    &datasrc_comedi,
#endif
#ifdef HAVE_LIBESD
    &datasrc_esd,
#endif
#ifdef HAVE_LIBASOUND
    &datasrc_sc
#endif
};

int ndatasrcs = sizeof(datasrcs)/sizeof(DataSrc *);
int datasrci = -1;
DataSrc *datasrc = NULL;

Channel ch[CHANNELS];

/* extra global variable definitions */
char *progname;                 /* the program's name, autoset via argv[0] */
char version[] = VERSION;       /* version of the program, from Makefile */
char error[256];                /* buffer for "one-line" error messages */
int clip = 0;                   /* whether we're maxed out or not */
char *filename;                 /* default file name */
int frames = 0;                 /* # of frames (full or partial) captured */
int in_progress = 0;            /* frame collection in progress?
                                 *   if so, this is index of next sample
                                 */

const char * datasrc_names(void)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);
    static char buffer[256];

    buffer[0] = '\0';

    for (i=0; i<limit; i++) {
        if (i > 0) {
            strcat(buffer, "/");
        }
        strcat(buffer, datasrcs[i]->name);
    }

    return buffer;
}

/* display command usage on stdout or stderr and exit */
void usage(int error)
{
    static char *def[] = {"graticule", "signal"}, *onoff[] = {"on", "off"};
    FILE *where;

    where = error ? stderr : stdout;

    fprintf(where, "usage: %s [options] [file]\n\
\n\
Startup Options  Description (defaults)               version %s\n\
-h               this Help message and exit\n\
-D <datasrc>     select named data source (%s)\n\
-A <device>      select named ALSA-device on soundcard        (%s)\n\
-o <option>      specify data source specific options\n\
-# <code>        #=1-%d, code=pos[.bits][:scale[:func#, mem a-z or cmd]] (0:1/1)\n\
-a <channel>     set the Active channel: 1-%d                  (%d)\n\
-s <scale>       time Scale: 1/500000-2000/1 where 1=1ms/div  (1/%d)\n\
-t <trigger>     Trigger level[:type[:channel]]               (%s)\n\
-l <cursors>     cursor Line positions: first[:second[:on?]]  (%s)\n\
-f <font name>   the Font name as-in %s\n\
-p <type>        Plot mode: 0.=point .0=sweep                 (%02d)\n\
                            1.=line  .1=accumulate\n\
                            2.=step  .2=strip-chart\n\
-g <style>       Graticule: 0=none,  1=minor, 2=major         (%d)\n\
-i <min interv>  Minimum display update interval (ms)         (50)\n\
-b               %s Behind instead of in front of %s\n\
-v               turn Verbose key help display %s\n\
file             %s file to load to restore settings and memory\n\
",
            progname, version, datasrc_names(), DEFAULT_ALSADEVICE, CHANNELS, CHANNELS, DEF_A,
            DEF_S, DEF_T, DEF_L,
            fonts,              /* the font method for the display */
            scope.scroll_mode + 10 * scope.plot_mode,
            scope.grat, def[DEF_B], def[!DEF_B],
            onoff[DEF_V], progname);
    exit(error);
}

/* Handle command line options
 *
 * If a data source was specified on the command line, we open it.  Otherwise, we open the first
 * available data source before processing most of the options because the user could specify an
 * option like "-3 0.0:1,000000:a", which is interpreted differently ('a' is either the first data
 * channel or the first memory channel) depending on whether a device is open.
 */

int datasrc_first(void);

void parse_args(int argc, char **argv)
{
    const char     *flags = "Hh"
        "1:2:3:4:5:6:7:8:"
        "a:r:s:t:l:c:m:d:f:p:g:o:i:bvxyz"
        "A:R:S:T:L:C:M:D:F:P:G:o:I:BVXYZ";
    int c;

    /* If a data source, data source option, or ALSA device name was specified on the command line,
     * parse them first.
     */

    while ((c = getopt(argc, argv, flags)) != EOF) {
        if ((c == 'D') || (c == 'o') || (c == 'A')) {
            handle_opt(c, optarg);
        }
    }

    /* If a data source was not specified, open the first one available. */

    if (! datasrc) {
        datasrc_first();
    }

    /* Reset to the beginning of the argument list and process the remaining options. */

    optind = 1;

    while ((c = getopt(argc, argv, flags)) != EOF) {
        if ((c != 'D') && (c != 'o') && (c != 'A')) {
            handle_opt(c, optarg);
        }
    }
}

/* cleanup before exiting due to error or program end */
void cleanup(void)
{
    cleanup_math();
}

/* initialize the scope */
void init_scope(void)
{
    /* XXX fix me - get better plot/scroll mode defaults here */
    scope.plot_mode = DEF_P / 2;
    scope.scroll_mode = DEF_P % 2;
    scope.scale = 1.0 / DEF_S;
    handle_opt('t', DEF_T);
    handle_opt('l', DEF_L);
    handle_opt('A', DEFAULT_ALSADEVICE);
    scope.grat = DEF_G;
    scope.behind = DEF_B;
    scope.run = 1;
    scope.select = DEF_A - 1;
    scope.verbose = DEF_V;
    scope.min_interval = 50;
}

/* initialize the signals */
void init_channels(void)
{
    int i;
    static int first = 1;

    if (!first) {
        do_math();                      /* XXX halt currently running commands */
    }
    first = 0;
    for (i = 0 ; i < CHANNELS ; i++) {
        bzero(&ch[i], sizeof(Channel));
        ch[i].signal = NULL;
        ch[i].scale = 1.0;
        ch[i].pos = 0;
        ch[i].show = 0;
        ch[i].bits = 0;
    }
}

/* samples needed to draw a current display of RATE
 *
 * scope.scale gives us the timebase in milliseconds per divisions.  Multiply by 10 to get
 * millseconds per screen, divide by 1000 to get seconds per screen, multiply by rate (in
 * samples/sec) to get samples per screen, and add one so rounding doesn't make us end a capture
 * before the end of the screen.
 *
 * XXX MAXWID is defined (in configure.ac) as: 1024*256=262144
 * On long time basis, this results in a sweep that does not fill the screen.
 * Under "worst conditions" - 44,1 kHz sampling rate and 2s/div - we need 882000 samples
 * for a full screen.
 */

int samples(int rate)
{
    static unsigned long int r;

    r = rate * 10 * scope.scale / 1000 + 1;

    if (r > MAXWID) {
        r = MAXWID;
    }

    return r;
}

/* scaledown/roundoff/scaleup: scale numbers like a scope does
 *
 * We compute the base ten logarithm of the original number.  Then we throw away the integer part,
 * leaving a number between 0 and 1 corresponding to a leading digit between 1 and 10.  By comparing
 * this to the logarithms of 7.5, 3.5, and 1.5, we either round off to 10, 5, 2, or 1, or pick
 * corresponding rounded-down or rounded-up targets and subtract out the corresponding logarithm.
 * The difference is the power of ten we need to multiply by to get to our target.
 *
 * The functions can also be supplied by a 'multiplier'.  This is to handle the case where the
 * display is calibrated and presented in mV, but the number that's being scaled is a pixel ratio.
 * We scale so that (num * multiplier) is "snapped" to 1, 2, or 5.
 */

#define LOG10  1.0
#define LOG7_5 0.87506
#define LOG5   0.69897
#define LOG3_5 0.54407
#define LOG2   0.30103
#define LOG1_5 0.17609
#define LOG1   0.0
#define LOG0_5 (-LOG2)

double scaledown(double num, double minimum, double multiplier)
{
    double log;

    log = log10(num * multiplier);
    log -= floor(log);

    if (log > LOG7_5) {
        log = LOG5 - log;
    } else if (log > LOG3_5) {
        log = LOG2 - log;
    } else if (log > LOG1_5) {
        log = LOG1 - log;
    } else {
        log = LOG0_5 - log;
    }

    num *= pow(10.0, log);

    return (num > minimum) ? num : minimum;
}

double roundoff(double num, double multiplier)
{
    double log;

    log = log10(num * multiplier);
    log -= floor(log);

    if (log > LOG7_5) {
        log = LOG10 - log;
    } else if (log > LOG3_5) {
        log = LOG5 - log;
    } else if (log > LOG1_5) {
        log = LOG2 - log;
    } else {
        log = LOG1 - log;
    }

    num *= pow(10.0, log);

    return num;
}

double scaleup(double num, double maximum, double multiplier)
{
    double log;

    log = log10(num * multiplier);
    log -= floor(log);

    if (log > LOG7_5) {
        log = LOG10 + LOG2 - log;
    } else if (log > LOG3_5) {
        log = LOG10 - log;
    } else if (log > LOG1_5) {
        log = LOG5 - log;
    } else {
        log = LOG2 - log;
    }

    num *= pow(10.0, log);

    return (num < maximum) ? num : maximum;
}

/* Sometimes we want to pass an int to a callback function that expects pointers */

const gpointer int_to_int_pointer(int i)
{
    static int array_size = 0;
    static int * array = NULL;

    if ((array == NULL) || (i > (array_size-1))) {
        array = g_renew(int, array, i+1);
        while (array_size < i+1) {
            array[array_size] = array_size;
            array_size ++;
        }
    }

    return (array + i);
}

/* Close the current data source */

void datasrc_close(void)
{
    int j,k;

    if (datasrc) {

        if (datasrc->clear_trigger) {
            datasrc->clear_trigger();
        }

        scope.trige = 0;

        /* clear listeners on our signal channels prior to close */
        for (j=0; j<datasrc->nchans(); j++) {
            Signal *sig = datasrc->chan(j);
            if (sig->listeners) {
                for (k=0; k<CHANNELS; k++) {
                    if (ch[k].signal == sig) {
                        recall_on_channel(NULL, &ch[k]);
                    }
                }
            }
        }
    }

    datasrc = NULL;
    datasrci = -1;
}

int datasrc_open(DataSrc *new_datasrc)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);

    if (new_datasrc->nchans() > 0) {

        datasrc_close();

        datasrc = new_datasrc;

        for (i=0; i<limit; i++) {
            if (datasrc == datasrcs[i]) datasrci = i;
        }

        /* All data sources have at least one channel.  Show it. */

        if (ch[0].signal == NULL) {
            ch[0].show = 1;
            recall_on_channel(datasrc->chan(0), &ch[0]);
        }

        /* If data sources has a second channel, show it.  Older versions did. */

        if ((ch[1].signal == NULL) && (datasrc->nchans() > 1)) {
            ch[1].show = 1;
            recall_on_channel(datasrc->chan(1), &ch[1]);
        }

        return 1;

    }

    return 0;
}

/* Open a data source even if it's open function returns 0 */

void datasrc_force_open(DataSrc *new_datasrc)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);

    datasrc_close();

    datasrc = new_datasrc;

    if (datasrc == NULL) {
        return;
    }

    for (i=0; i<limit; i++) {
        if (datasrc == datasrcs[i]) datasrci = i;
    }

    /* If data sources has a channel, show it. */

    /* XXX problem here - if data source requires options to be set before it can open properly,
     * nchans() won't return anything valid yet
     */

    if ((ch[0].signal == NULL) && (datasrc->nchans() > 0)) {
        ch[0].show = 1;
        recall_on_channel(datasrc->chan(0), &ch[0]);
    }

    /* If data sources has a second channel, show it.  Older versions did. */

    if ((ch[1].signal == NULL) && (datasrc->nchans() > 1)) {
        ch[1].show = 1;
        recall_on_channel(datasrc->chan(1), &ch[1]);
    }

}

/* Find the first valid datasrc; should only be called once return TRUE if successful; FALSE if no
 * valid datasrcs were found
 */

int datasrc_first(void)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);

    for (i=0; i<limit; i++) {
        if (datasrc_open(datasrcs[i])) {
            return 1;
        }
    }
    return 0;
}

/* Advance to next datasrc
 * return TRUE if successful; FALSE if no other valid datasrcs were found
 */

int datasrc_next(void)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);

    if (datasrci < 0) return datasrc_first();

    for (i=(datasrci+1)%limit; i != datasrci; i=(i+1)%limit) {
        if (datasrc_open(datasrcs[i])) {
            return 1;
        }
    }
    return 0;
}

/* Select the named datasrc (case insensitive match)
 * return TRUE if successful; FALSE if it wasn't found or couldn't be opened
 */

int datasrc_byname(char *name)
{
    int i;
    int limit = sizeof(datasrcs)/sizeof(DataSrc *);

    /* Don't do anything if we're selecting the data source we've got */
    if (datasrc && strcasecmp(name, datasrc->name) == 0) return 1;

    for (i=0; i<limit; i++) {
        if (strcasecmp(name, datasrcs[i]->name) == 0) {
            datasrc_force_open(datasrcs[i]);
            return 1;
        }
    }

    return 0;
}

/* gr_* UIs call this after selecting file and confirming overwrite */
void savefile(char *file)
{
    writefile(filename = file);
}

/* gr_* UIs call this after selecting file to load */
void loadfile(char *file)
{
    readfile(filename = file);
}

/* handle single key commands */
void handle_key(unsigned char c)
{
    static Channel *p;
    int displayed_samples, max_samples;

    p = &ch[scope.select];

    /* These next two are used for keyboard movement of the cursors.
     *
     * Cursors are stored as sample numbers (1 based).  max_samples gives the largest legal value
     * for a cursor.  displayed_samples gives the number of samples displayed on the screen at once
     * so we can jump by 1/20 of a screen width.  If the signal is wider than the screen width (i.e,
     * the scrollbar is on), then these two numbers will be different.
     */

    displayed_samples = p->signal ? samples(p->signal->rate) : 0;
    if ( p->signal ) {
        if ( samples(p->signal->rate) > p->signal->num ) {
            max_samples = samples(p->signal->rate);
        } else {
            max_samples = p->signal->num;
        }
    } else {
        max_samples = 0;
    }

    if (c >= 'A' && c <= 'Z') {
        if (p->signal) {
            if(!scope.run){
                save(c - 'A', scope.select);/* store channel */
                clear();                    /* need this in case other chan displays mem */
            }
            else {
                set_save_pending(c);        /* remember to store channel as soon as a sweep is complete*/
            }
        }
        return;
    } else if (c >= 'a' && c <= 'z') {
        if (datasrc && (c - 'a' < datasrc->nchans())) {
            recall(datasrc->chan(c - 'a'));     /* recall data channel */
        } else if (mem[c - 'a'].num > 0) {
            recall(&mem[c - 'a']);      /* recall memory location if something there */
        }
        p->show = 1;                    /* always display newly recalled channel */
        clear();
        return;
    } else if (c >= '1' && c <= '0' + CHANNELS) {
        scope.select = (c - '1');       /* select channel */
        // clear();                     /* do this in case cursors move; see comments in display.c:drawdata() */
        show_data();
        update_text();
        return;
    }
    switch (c) {
    case 0:                     /* no key pressed */
        break;
    case '\'':                  /* toggle manual cursors */
        scope.curs = ! scope.curs;
        show_data();
        break;
    case '"':                   /* reset cursors to first sample */
        scope.cursa = scope.cursb = 1;
        show_data();
        break;
    case 'q' - 96:              /* -96 is CTRL keys */
        if ((scope.cursa -= displayed_samples / 20) < 1) {
            scope.cursa = max_samples - 1;
        }
        show_data();
        break;
    case 'w' - 96:
        if ((scope.cursa += displayed_samples / 20) >= max_samples) {
            scope.cursa = 1;
        }
        show_data();
        break;
    case 'e' - 96:
        if (--scope.cursa < 1) {
            scope.cursa = max_samples - 1;
        }
        show_data();
        break;
    case 'r' - 96:
        if (++scope.cursa >= max_samples) {
            scope.cursa = 1;
        }
        show_data();
        break;
    case 'a' - 96:
        if ((scope.cursb -= displayed_samples / 20) < 1) {
            scope.cursb = max_samples - 1;
        }
        show_data();
        break;
    case 's' - 96:
        if ((scope.cursb += displayed_samples / 20) >= max_samples) {
            scope.cursb = 1;
        }
        show_data();
        break;
    case 'd' - 96:
        if (--scope.cursb < 1) {
            scope.cursb = max_samples - 1;
        }
        show_data();
        break;
    case 'f' - 96:
        if (++scope.cursb >= max_samples) {
            scope.cursb = 1;
        }
        show_data();
        break;
    case '\t':
        if (p->show) {          /* show / hide channel */
            p->show = 0;
        } else {
            p->show = 1;
        }
        show_data();
        update_text();
        break;
    case '~':
        if ((p->bits += 2) > 16) {
            p->bits = 0;
        }
        show_data();
        update_text();
        break;
    case '`':
        if ((p->bits -= 2) < 0) {
            p->bits = 16;
        }
        show_data();
        update_text();
        break;
    case '}':
        if (p->signal) {                /* increase scale - 100x is the max */
            if (p->signal->volts != 0) {
                p->scale = scaleup(p->scale, 100, 1.0/p->signal->volts);
            } else {
                p->scale = scaleup(p->scale, 100, 1);
            }
            update_text();
            show_data();
        }
        break;
    case '{':
        if (p->signal) {                /* decrease scale - 1:100 is the min */
            if (p->signal->volts != 0) {
                p->scale = scaledown(p->scale, .01, 1.0/p->signal->volts);
            } else {
                p->scale = scaledown(p->scale, .01, 1);
            }
            update_text();
            show_data();
        }
        break;
    case ']':
        p->pos += 0.1;          /* position up */
        if (p->pos < -1) {
            p->pos = -1;
        }
        if (p->pos * p->pos < 0.0001) {
            p->pos = 0;
        }
        update_text();
        show_data();
        break;
    case '[':
        p->pos -= 0.1;          /* position down */
        if (p->pos > 1) {
            p->pos = 1;
        }
        if (p->pos * p->pos < 0.0001) {
            p->pos = 0;
        }
        update_text();
        show_data();
        break;
    case ';':
        if (scope.select > 1) { /* next math function */
            next_func();
            p->show = 1;
            clear();
        } else {
            message("Math can not run on Channel 1 or 2");
        }
        break;
    case ':':
        if (scope.select > 1) { /* previous math function */
            prev_func();
            p->show = 1;
            clear();
        } else {
            message("Math can not run on Channel 1 or 2");
        }
        break;
    case '0':
        /* this corresponds to a minimum time scale of 2 ns/div */
        scope.scale = scaledown(scope.scale, 1.0/500000, 1);
        timebase_changed();
        break;
    case '9':
        /* this corresponds to a maximum time scale of 2 sec/div */
        scope.scale = scaleup(scope.scale, 2000, 1);
        timebase_changed();
        break;
    case '=':
        if (datasrc && datasrc->set_trigger) {  /* increase trigger */
            scope.trig += 8;
            datasrc->set_trigger(scope.trigch, &scope.trig, scope.trige);
            clear();
        }
        break;
    case '-':
        if (datasrc && datasrc->set_trigger) {  /* decrease trigger */
            scope.trig -= 8;
            datasrc->set_trigger(scope.trigch, &scope.trig, scope.trige);
            clear();
        }
        break;
    case '_':                   /* change trigger channel */
        if (scope.trige != 0 && datasrc && datasrc->set_trigger) {
            do {
                scope.trigch ++;
                if (scope.trigch >= datasrc->nchans()) {
                    scope.trigch = 0;
                }
            } while (datasrc->set_trigger(scope.trigch,
                                          &scope.trig, scope.trige) == 0);
            clear();
        }
        break;
    case '+':
        if (datasrc && datasrc->set_trigger) {  /* change trigger type */
            do {
                scope.trige++;
                if (scope.trige > 2) {
                    scope.trige = 0;
                }
                if (scope.trige == 0 && datasrc->clear_trigger) {
                    datasrc->clear_trigger();
                }
            } while ((scope.trige != 0) &&
                     (datasrc->set_trigger(scope.trigch,
                                           &scope.trig, scope.trige) == 0));
            clear();
        }
        break;
    case '(':
        if (datasrc && datasrc->change_rate && datasrc->change_rate(-1)) {
            in_progress = 0;
            clear();
        }
        break;
    case ')':
        if (datasrc && datasrc->change_rate && datasrc->change_rate(1)) {
            in_progress = 0;
            clear();
        }
        break;
    case '@':                   /* load file */
        LoadSaveFile(0);
        break;
    case '#':                   /* save file */
        LoadSaveFile(1);
        break;
    case '$':                   /* run external math */
        if (scope.select > 1) {
            PerlFunction();
        } else {
            message("Pipes can not run on Channel 1 or 2");
        }
        break;
    case '&':                   /* toggle between various data sources */
        if (datasrc_next()) {
            clear();
        }
        break;
    case '*':
        if (datasrc && (datasrc->option1 != NULL) && datasrc->option1()) {
            clear();
        }
        break;
    case '^':
        if (datasrc && (datasrc->option2 != NULL) && datasrc->option2()) {
            clear();
        }
        break;
    case '!':
        scope.scroll_mode++;            /* sweep, accumulate, stripchart */
        if (scope.scroll_mode > 2) {
            scope.scroll_mode = 0;
            scope.plot_mode++;          /* point, line, step */
            if (scope.plot_mode > 2) {
                scope.plot_mode = 0;
            }
        }
        update_text();
        show_data();
        break;
    case ',':
        scope.grat++;           /* graticule off/on/more */
        if (scope.grat > 2) {
            scope.grat = 0;
        }
        update_text();
        show_data();
        break;
    case '.':
        scope.behind = !scope.behind; /* graticule behind/in front of signal */
        update_text();
        break;
    case '?':
    case '/':
        scope.verbose = !scope.verbose; /* on-screen help */
        update_text();
        break;
    case ' ':
        scope.run++;            /* run / wait / stop */
        if (scope.run > 2) {
            scope.run = 0;
        }
        if ((scope.run == 1) && datasrc) {
            setinputfd(datasrc->fd());  /* were stopped, so now start */
        }
        update_text();
        break;
    case '\r':
    case '\n':
        clear();                        /* refresh screen */
        break;
    case '\b':
    case 127:
        recall(NULL);           /* backspace/DEL - clear channel */
        clear();                        /* otherwise chan freezes instead of clear */
        break;
    case '\e':
        cleanup();                      /* quit */
        exit(0);
        break;
    default:
        c = 0;                  /* ignore unknown keys */
    }
}

/* main program */

int main(int argc, char **argv)
{
    progname = strrchr(argv[0], '/');
    if (progname == NULL) {
        progname = argv[0];             /* global for error messages, usage */
    } else {
        progname++;
    }
    init_scope();
    init_channels();
    init_math();
    if ((argc = OpenDisplay(argc, argv)) == FALSE) {
        exit(1);
    }
    parse_args(argc, argv);		/* also opens a data source, if possible */
    init_widgets();
    clear();

    filename = FILENAME;
    if ((optind < argc) && (argv[optind] != NULL)) {
        filename = argv[optind];
        readfile(filename);
    }

    clear();
    animate(NULL);

    gtk_main();

    cleanup();
    exit(0);
}

/* split_field() is used when we want to take a (possibly) long string and split it across two
 * fields.  'fieldtwo' is true to return field 2, otherwise return field 1.
 */

const char * split_field(const char *str, int fieldtwo, int limit)
{
    static char buffer[256];
    int i;

    if (strlen(str) > limit) {

        char * buf_index = NULL;

        if (!fieldtwo) {

            for (i=0; i<limit; i++) {
                buffer[i] = str[i];

                if (str[i] == ' ') {
                    buf_index = &buffer[i];
                }
            }

            if (buf_index) {
                *buf_index = '\0';
            } else {
                buffer[i] = '\0';
            }

        } else {

            const char *sp_index = NULL;

            for (i=0; i<limit; i++) {

                if (str[i] == ' ') {
                    sp_index = &str[i];
                }
            }

            if (sp_index) {
                i = 0;
                while (*sp_index) buffer[i++] = *sp_index++;
                buffer[i] = '\0';
            } else {
                i = 0;
                while (str[limit+i]) buffer[i] = str[i];
                buffer[i] = '\0';
            }
        }

        return buffer;

    } else {

        /* Length of string less than limit */

        if (fieldtwo) {
            return "";
        } else {
            return str;
        }

    }
}
