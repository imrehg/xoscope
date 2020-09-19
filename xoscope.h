/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file defines the program's global variables and structures
 *
 */

#include <gtk/gtk.h>            /* need GdkPoint below */
#include <gtkdatabox_graph.h>

#include "config.h"

extern char alsaDevice[];

#define SND_QUERY_INTERVALL 10 // we check for new data every SND_QUERY_INTERVALL ms

/* global program variables */
extern char *progname;
extern char version[];
extern char error[256];
extern int quit_key_pressed;
extern int clip;
extern char *filename;
extern int in_progress;

typedef struct Scope {          /* The oscilloscope */
    int plot_mode;              /* 0 - point; 1 - line; 2 - step */
    int scroll_mode;            /* 0 - sweep; 1 - accumulate; 2 - stripchart */
    int verbose;
    int run;
    float scale;
    int grat;
    int behind;
    int select;
    int trigch;
    int trige;
    int trig;
    int curs;
    int cursa;
    int cursb;
    int min_interval;
} Scope;
extern Scope scope;

/* Signal - the input/memory/math signals
 *
 * sc_linux.c depends on the order of these fields in a static initializer
 */

typedef struct Signal {
    char name[16];              /* Textual name of this signal (for display) */
    char savestr[256];          /* String used in save files */
    int rate;                   /* sampling rate in samples/sec */
#if SC_16BIT
    double volts;               /* millivolts per 320 sample values */
#else
    int volts;                  /* millivolts per 320 sample values */
#endif
    int frame;                  /* Current frame number, for comparisons */
    int num;                    /* number of samples read from current frame */
    int delay;                  /* Delay, in ten-thousandths of samples */
    int listeners;              /* Number of things 'listening' to this Sig */
    int bits;                   /* number of valid bits - 0 for analog sig */
    int width;                  /* size of data[] in samples */
    short *data;                /* the data samples */
} Signal;

extern Signal mem[26];          /* Memory channels */

typedef struct DataSrc {        /* A source of data samples */

    char *              name;

    /* returns number of data channels available.  Return value can change around pretty much at
     * will.  Zero indicates device unavailable
     */
    int                 (* nchans)(void);

    /* returns a pointer to the Signal structure for a numbered channel */
    Signal *            (* chan)(int chan);

    /* mode is 1 (rising trigger) or 2 (falling trigger) returns TRUE if trigger could be set;
     * otherwise (it couldn't be set) return FALSE and leave trigger cleared 'level' is a pointer to
     * the trigger level in signed raw sample values If a trigger can be set near, but not exactly
     * on, the requested level, the function returns TRUE, sets the trigger, and modifies 'level' to
     * the adjusted value.  */
    int                 (* set_trigger)(int chan, int *levelp, int mode);
    void                (* clear_trigger)(void);

    /* dir is 1 for faster; -1 for slower; returns TRUE if rate changed */
    int                 (* change_rate)(int dir);

    /* sets the frame width (number of samples to capture per sweep) for all channels in the
     * DataSrc.  Success is indicated by the 'width' field changing in the DataSrc's Signal
     * structures.  Can be NULL to indicate device does not support multiple frame widths.
     */
    void                (* set_width)(int width);

    /* reset() gets called whenever we want to start a new capture sweep.  It should get called
     * after any of the above channel, trigger, or rate functions have been used to change the data
     * source capturing parameters, or after the capturing channels have been changed.  Only after
     * reset() has been called are the rate and volts fields in the Signal structures guaranteed
     * valid.
     */
    void                (* reset)(void);

    /* returns a file descriptor to poll on (for read) to indicate that the data source has data
     * available for read, via get_data() below.  return -1 to indicate no active signal capturing.
     * Always gets called after a reset()
     */
    int                 (* fd)(void);

    /* get_data() is called when poll(2) on the file descriptor returned by fd() indicates data is
     * available to read.  It is responsible for filling in the Signal arrays returned by the chan()
     * function, and returns TRUE if the trigger condition was hit (currently used only in
     * single-shot mode to stop the scope after a single trace).  It also must set the global
     * variable 'in_progress' TRUE if it returns in the middle of a trace, and FALSE at the end of a
     * trace.  get_data() must always return at the end of every trace, as the display code assumes
     * that a trace is completely drawn before moving on the next trace.
     */
    int                 (* get_data)(void);

    /* This function allows the data source to display various status information on the screen.  i
     * ranges from 0 to 7, and corresponds to 8 information fields near the bottom of the display.
     * Fields 0 through 3 have room for about 16 chars, fields 4 and 5 have room for about 20, and
     * fields 6 and 7 can hold about 12.  Function pointer can be NULL if there's no status info to
     * display.
     */

    const char *        (* status_str)(int i);

    /* These functions are called when the option1 (*) or option2 (^) keys are pressed and should
     * return 1 to do a datasrc->reset.  Their corresponding string functions should return a short
     * string to be displayed on the screen, which can be NULL.  If the option function returns
     * TRUE, a clear() is done.  Any of these pointers can be NULL.  If the 'str' function is NULL,
     * no help will be displayed for that option key, but this doesn't preclude the corresponding
     * option function from doing something!
     */

    int                 (* option1)(void);
    const char *        (* option1str)(void);
    int                 (* option2)(void);
    const char *        (* option2str)(void);

    /* set_option()/save_option()
     *
     * These functions are used to set and save data source specific options, either from the
     * command line or from a savefile, including but not limited to the keyboard options
     * corresponding to option1/option2.  To generate the savefile, save_option() will be called
     * repeatedly with arguments starting at zero and increasing until it returns NULL.  Each string
     * returned will later trigger a call to set_option() when the savefile is loaded.  Returning an
     * empty string will skip to the next integer without outputing anything.
     *
     * set_option() should return TRUE if it was able to parse and process the option; FALSE
     * otherwise.  The options can also be specified on the command line, so should be human
     * readable.
     *
     * Both function pointers can be NULL.
     */

    int                 (* set_option)(char *);
    char *              (* save_option)(int);

    /* This function is only used if the X Windows GTK interface is in use.  If non-NULL, it causes
     * the "Device Options..." item on the File menu to be made sensitive, and the function itself
     * is called whenever that item is clicked.  It should trigger a popup menu with device-specific
     * options.
     */
    void                (* gtk_options)(void);

} DataSrc;

extern DataSrc *datasrc;
extern DataSrc *datasrcs[];
extern int ndatasrcs;

typedef GdkPoint Point;

typedef struct SignalLine {
    int next_point;
    struct SignalLine *next;    /* keep a linked list */
    GtkDataboxGraph *graph;
    gfloat *X;
    gfloat *Y;
    short *data;
    double x_offset;
    double y_offset;
    double y_scale;
} SignalLine;

typedef struct Channel {        /* The display channels */
    Signal *signal;
    SignalLine *signalline[16]; /* 16 - could have up to 16 bits per sample,
                                 * thus, up to 16 signal lines per channel
                                 * in digital mode */
    double scale;               /* Scaling factor we multiply samples by */
    gfloat pos;                 /* Location of zero line on scope display;
                                 * 0 is center; 1 is top; -1 is bottom */
    int old_frame;              /* last frame number plotted */
    int color;
    int show;
    int bits;
} Channel;
extern Channel ch[CHANNELS];

/* functions that are called by files other than xoscope.c */
void    usage(int);
void    handle_key(unsigned char);
void    cleanup(void);
void    init_scope(void);
void    init_channels(void);
int     samples(int);
void    loadfile(char *);
void    savefile(char *);
const char *    split_field(const char *, int, int);

int     datasrc_byname(char *);
void    datasrc_force_open(DataSrc *);

double  roundoff(double, double);

int     max(int, int);
int     min(int, int);

const gpointer int_to_int_pointer(int);

extern char serial_error[];

/* Functions defined in display library specific files */
void    setinputfd(int);
void    settimeout(int);
