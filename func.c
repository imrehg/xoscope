/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the signal math and memory.
 * To add math functions, search for !!! and add to those sections.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "xoscope.h"
#include "fft.h"
#include "display.h"
#include "func.h"
#include "xoscope_gtk.h"

Signal mem[26];         /* 26 memories, corresponding to 26 letters */
short  mem_pending[26]; /* Flags to indicate we wont to store a channel when a sweep is complete */

/* recall given memory register to the currently selected signal */
void recall_on_channel(Signal *signal, Channel *ch)
{
    if (ch->signal) ch->signal->listeners --;
    ch->signal = signal;
    if (signal) {
        signal->listeners ++;

        /* no guarantee that signal->bits will be correct yet */
        ch->bits = signal->bits;
    }
}

void recall(Signal *signal)
{
    recall_on_channel(signal, &ch[scope.select]);
}

extern int in_progress;

/* store the currently selected signal to the given memory register */


void set_save_pending(char c)
{
    mem_pending[c - 'A'] = scope.select;
}

void do_save_pending(void)
{
    int i;

    if(in_progress)
        return;
        
    for(i = 0; i < 26; i++){
        if(mem_pending[i] >= 0){
            save(i, mem_pending[i]);
            mem_pending[i] = -1;
            /* This is from the original code.
             * I'm not sure ist really needed.
             * Original comment follows: 
             */           
             /* need this in case other chan displays mem */
             clear(); 
        }
    }
}

void save(int dest, int src)
{
    if (ch[src].signal == NULL) 
        return;

    /* Don't want the name - leave that at 'Memory x'
     * Also, increment frame instead of setting it to signal->frame in case signal->frame is the
     * same as mem's old frame number!
     */
    if (mem[dest].data != NULL) {
        free(mem[dest].data);
    }
    mem[dest].data = malloc(ch[src].signal->width * sizeof(short));
    if(mem[dest].data == NULL){
        fprintf(stderr, "malloc failed in save()\n");
        exit(0);
    }
    memcpy(mem[dest].data, ch[src].signal->data, ch[src].signal->width * sizeof(short));

    mem[dest].rate = ch[src].signal->rate;
    mem[dest].num = ch[src].signal->width;
    mem[dest].width = ch[src].signal->width;
    mem[dest].frame ++;
    mem[dest].volts = ch[src].signal->volts;
}

/* !!! External process handling
 *
 * Could use some work... the original code (xoscope-1.8) always sent the first h_points data points
 * from the first two display channels through to the external process, even if the scaling on those
 * channels was set up so that weren't h_points of valid data, or set down so that what was
 * displayed was way more than h_points.  In any event, I'm not a big fan of these math functions,
 * so I just left it alone like that.
 */

struct external {
    struct external *next;
    Signal signal;
    int pid;                    /* Zero if we already closed it down */
    int to, from, errors;                       /* Pipes */
    int last_frame_ch0, last_frame_ch1;
    int last_num_ch0, last_num_ch1;
};

static struct external *externals = NULL;

/* startcommand() / start_command_on_channel()
 *
 * Start an external command running on the current display channel.
 *
 * xoscope version 1.x included an auxilary program (operl) that could be invoked to display the
 * result of a Perl function.  We now call perl directly, to avoid dependence on an external program
 * being installed.  For backwards compatibility, external commands that begin with the string
 * "operl" as interpreted as perl functions.
 *
 * gr_* UIs call this after prompting for command to run
 */

void start_program_on_channel(const char *command, Channel *ch_select)
{
    struct external *ext;
    int pid;
    int from[2], to[2], errors[2];
    static char *path, *oscopepath;

    if (pipe(to) || pipe(from) || pipe(errors)) { /* get a set of pipes */
        sprintf(error, "%s: can't create pipes", progname);
        perror(error);
        return;
    }

    signal(SIGPIPE, SIG_IGN);

    if ((pid = fork()) > 0) {           /* parent */
        close(to[0]);
        close(from[1]);
        close(errors[1]);
    } else if (pid == 0) {              /* child */
        int fd;

        dup2(to[0], 0);
        dup2(from[1], 1);
        dup2(errors[1], 2);

        /* We now want to close everything except the first three file descriptors (there can be a
         * lot of descriptors open in the parent, to other external programs, to the windowing
         * system, possibly to a file being loaded).  We tacitly assume that pipe() assigned the
         * lowest available descriptors, so errors[1] should be our largest descriptor.
         */

        for (fd=3; fd <= errors[1]; fd ++) {
            close(fd);
        }

        /* XXX add additional environment vars here for sampling rate and number of samples per
         * frame
         */

        if ((oscopepath = getenv("OSCOPEPATH")) == NULL) {
            oscopepath = PACKAGE_LIBEXEC_DIR;
            }

        path = g_malloc(strlen(oscopepath) + 6);
            sprintf(path,"PATH=%s", oscopepath);
            putenv(path);

        execlp("/bin/sh", "sh", "-c", command, NULL);
        sprintf(error, "%s: child can't exec /bin/sh -c \"%s\"",
                progname, command);
        perror(error);
        exit(1);
    } else {                    /* fork error */
        sprintf(error, "%s: can't fork", progname);
        perror(error);
        return;
    }

    ext = g_new0(struct external, 1);

    snprintf(ext->signal.savestr, sizeof(ext->signal.savestr), "%s", command);
    snprintf(ext->signal.name, sizeof(ext->signal.name), "%s", command);

    ext->pid = pid;
    ext->from = from[0];
    ext->to = to[1];
    ext->errors = errors[0];
    fcntl(ext->errors, F_SETFL, O_NONBLOCK);

    /* XXX Here we inherit various parameters from channel 0.  These should be set more
     * intelligently.
     */

    if (ch[0].signal != NULL) {
        ext->signal.data = g_new(short, ch[0].signal->width);

        ext->signal.width = ch[0].signal->width;
        ext->signal.rate = ch[0].signal->rate;
        ext->signal.delay = ch[0].signal->delay;
        ext->signal.bits = ch[0].signal->bits;
    }

    ext->next = externals;
    externals = ext;

    recall_on_channel(&ext->signal, ch_select);
    ch[scope.select].show = 1;
}

void start_perl_function_on_channel(const char *command, Channel *ch_select)
{
    struct external *ext;
    int pid;
    int program[2], from[2], to[2], errors[2];
    FILE *program_FILE;
    static char *envvar;
    extern char * operl_program;

    if (pipe(program) || pipe(to) || pipe(from) || pipe(errors)) { /* get a set of pipes */
        sprintf(error, "%s: can't create pipes", progname);
        perror(error);
        return;
    }

    signal(SIGPIPE, SIG_IGN);

    if ((pid = fork()) > 0) {           /* parent */
        close(program[0]);
        close(to[0]);
        close(from[1]);
        close(errors[1]);
    } else if (pid == 0) {              /* child */
        int fd;

        dup2(program[0], 0);
        dup2(to[0], 3);
        dup2(from[1], 1);
        dup2(errors[1], 2);

        /* We now want to close everything except the first four file descriptors (there can be a
         * lot of descriptors open in the parent, to other external programs, to the windowing
         * system, possibly to a file being loaded).  We tacitly assume that pipe() assigned the
         * lowest available descriptors, so errors[1] should be our largest descriptor.
         */

        for (fd=4; fd <= errors[1]; fd ++) {
            close(fd);
        }

        /* XXX add additional environment vars here for sampling rate and number of samples per
         * frame
         */

        envvar = g_malloc(strlen(command) + 6);
        sprintf(envvar,"FUNC=%s", command);
        putenv(envvar);

        execlp(PERL, PERL, NULL);
        perror("can't exec " PERL);
        exit(1);
    } else {                    /* fork error */
        sprintf(error, "%s: can't fork", progname);
        perror(error);
        return;
    }

    /* Send the embedded Perl script to the child on its stdin. */

    program_FILE = fdopen(program[1], "w");
    fputs(operl_program, program_FILE);
    fclose(program_FILE);

    ext = g_new0(struct external, 1);

    snprintf(ext->signal.savestr, sizeof(ext->signal.savestr), "operl '%s'", command);
    snprintf(ext->signal.name, sizeof(ext->signal.name), "%s", command);

    ext->pid = pid;
    ext->from = from[0];
    ext->to = to[1];
    ext->errors = errors[0];
    fcntl(ext->errors, F_SETFL, O_NONBLOCK);

    /* XXX Here we inherit various parameters from channel 0.  These should be set more
     * intelligently.
     */

    if (ch[0].signal != NULL) {
        ext->signal.data = g_new(short, ch[0].signal->width);

        ext->signal.width = ch[0].signal->width;
        ext->signal.rate = ch[0].signal->rate;
        ext->signal.delay = ch[0].signal->delay;
        ext->signal.bits = ch[0].signal->bits;
    }

    ext->next = externals;
    externals = ext;

    recall_on_channel(&ext->signal, ch_select);
    ch[scope.select].show = 1;
}

void start_command_on_channel(const char *command, Channel *ch_select)
{
    /* Check if command string starts with "operl ".  If so, discard any quotes and leading/trailing
     * whitespace and handle the remainder of the string as a Perl function.
     *
     * This is done both for backwards compatibility and to retreive Perl functions from a save file.
     */

    if (strncmp(command, "operl ", 6) == 0) {
        char * function;
        int i;

        for (i=6; isspace(command[i]); i++);
        function = g_strdup(command+i);

        while ((strlen(function) > 0) && isspace(function[strlen(function)-1])) {
            function[strlen(function)-1] = '\0';
        }
        if (((function[0] == '\'') || (function[0] == '\"'))
            && (function[0] == function[strlen(function)-1])) {
            function[strlen(function)-1] = '\0';
            start_perl_function_on_channel(function+1, ch_select);
        } else {
            start_perl_function_on_channel(function, ch_select);
        }
        g_free(function);
    } else {
        start_program_on_channel(command, ch_select);
    }
}

void startcommand(const char *command)
{
    if (scope.select > 1) {
        start_command_on_channel(command, &ch[scope.select]);
        clear();
    }
}

void start_perl_function(const char *command)
{
    if (scope.select > 1) {
        start_perl_function_on_channel(command, &ch[scope.select]);
        clear();
    }
}

void restart_external_commands(void)
{
    struct external *ext;

    for (ext = externals; ext != NULL; ext = ext->next) {
        ext->signal.data = g_renew(short, ext->signal.data, ch[0].signal->width);
        ext->signal.width = ch[0].signal->width;
        ext->signal.num = 0;
    }
}


/* Check everything on the externals list; run what needs to be run, and clean up anything left
 * linguring behind.
 *
 * XXX externals shouldn't depend on having signals on both channels 1 and 2
 */

static void run_externals(void)
{
    struct external *ext;

    short *a, *b, *c;
    int i;
    char error_message[256];

    for (ext = externals; ext != NULL; ext = ext->next) {

        if (ext->signal.listeners > 0) {

            if ((ext->pid > 0) && (ch[0].signal != NULL) && (ch[1].signal != NULL)) {

                /* There's a slight chance that if we change one of the channels, the new channel
                 * may have a frame number identical to the last one, but that shouldn't hurt us too
                 * bad.
                 */

                if ((ch[0].signal->frame != ext->last_frame_ch0) ||
                    (ch[1].signal->frame != ext->last_frame_ch1)) {

                    ext->last_frame_ch0 = ch[0].signal->frame;
                    ext->last_frame_ch1 = ch[1].signal->frame;
                    ext->signal.frame ++;
                    ext->signal.num = 0;
                }

                /* To avoid a race condition that might drop data or error messages, we check first
                 * to see if the process has exited, then read from the pipes.
                 */

                if (waitpid(ext->pid, NULL, WNOHANG) == ext->pid) {
                    ext->pid = 0;
                }

                /* Check for error messages on stderr, and display them to the user */

                i = read(ext->errors, error_message, sizeof(error_message)-1);
                if (i > 0) {
                    error_message[i] = '\0';
                    message(error_message);
                    // XXX any perl errors realistically need this uncommented to debug them
                    // fprintf(stderr, "%s", error_message);
                }

                /* We may already have sent and received part of a frame, so start our pointers at
                 * whatever our last offset was, and keep going until we hit the limit of either
                 * channel 0 or channel 1.
                 *
                 * XXX explain this better
                 * XXX make sure this can't slow the program down!
                 */

                if ((ext->signal.width < ch[0].signal->width) && (ext->signal.width < ch[1].signal->width)) {
                    ext->signal.data = g_renew(short, ext->signal.data, ch[0].signal->width);
                    ext->signal.width = ch[0].signal->width;
                }

                a = ch[0].signal->data + ext->signal.num;
                b = ch[1].signal->data + ext->signal.num;
                c = ext->signal.data + ext->signal.num;

                for (i = ext->signal.num; (i < ch[0].signal->num) && (i < ch[1].signal->num); i++) {
                    if (write(ext->to, a++, sizeof(short)) != sizeof(short))
                        break;
                    if (write(ext->to, b++, sizeof(short)) != sizeof(short))
                        break;
                    if (read(ext->from, c++, sizeof(short)) != sizeof(short))
                        break;
                }
                ext->signal.num = i;

                /* If we earlier determined that the process had exited, close the pipes down now
                 * that we've read everything.
                 */

                if (ext->pid == 0) {
                    close(ext->from);
                    close(ext->to);
                    close(ext->errors);
                    ext->from = -1;
                    ext->to = -1;
                    ext->errors = -1;
                }
            }

        } else {

            /* Nobody listening anymore; close down the pipes and wait for process to exit. */

            if (ext->from != -1) {
                close(ext->from);
                close(ext->to);
                close(ext->errors);
                ext->from = -1;
                ext->to = -1;
                ext->errors = -1;
            }

            if (ext->pid) {
                if (waitpid(ext->pid, NULL, WNOHANG) == ext->pid) {
                    ext->pid = 0;
            /* XXX Delete ext from list and free() it */
                }
            }
        }
    }
}

/* !!! The functions; they take one arg: a Signal ptr to store results in */

/* Invert */
void inv(Signal *dest, Signal *src)
{
    int i;
    short *a, *b;

    if (src == NULL) return;

    dest->rate = src->rate;
    dest->num = src->num;
    dest->volts = src->volts;
    dest->frame = src->frame;

    a = src->data;
    b = dest->data;
    for (i = 0 ; i < src->num; i++) {
        *b++ = -1 * *a++;
    }
}

void inv1(Signal *sig)
{
    inv(sig, ch[0].signal);
}

void inv2(Signal *sig)
{
    inv(sig, ch[1].signal);
}

/* The sum of the two channels */
void sum(Signal *dest)
{
    int i;
    short *a, *b, *c;
    int sum;

    if ((ch[0].signal == NULL) || (ch[1].signal == NULL)) 
        return;

    a = ch[0].signal->data;
    b = ch[1].signal->data;
    c = dest->data;

    dest->frame = ch[0].signal->frame + ch[1].signal->frame;
    dest->num = ch[0].signal->num;

    if (dest->num > ch[1].signal->num) 
        dest->num = ch[1].signal->num;

    for (i = 0 ; i < dest->num ; i++) {
        sum = *a++ + *b++;
        if(sum > SHRT_MAX)
            sum = SHRT_MAX;
        else if(sum < SHRT_MIN)
           sum = SHRT_MIN;
        *c++ = (short)sum;
    }
}

/* The difference of the two channels */
void diff(Signal *dest)
{
    int i;
    short *a, *b, *c;
    int sum;

    if ((ch[0].signal == NULL) || (ch[1].signal == NULL))
        return;

    a = ch[0].signal->data;
    b = ch[1].signal->data;
    c = dest->data;

    dest->frame = ch[0].signal->frame + ch[1].signal->frame;
    dest->num = ch[0].signal->num;

    if (dest->num > ch[1].signal->num) dest->num = ch[1].signal->num;

    for (i = 0 ; i < dest->num ; i++) {
         sum = *a++ - *b++;
        if(sum > SHRT_MAX)
            sum = SHRT_MAX;
        else if(sum < SHRT_MIN)
           sum = SHRT_MIN;
        *c++ = (short)sum;
    }
}


/* The average of the two channels */
void avg(Signal *dest)
{
    int i;
    short *a, *b, *c;

    if ((ch[0].signal == NULL) || (ch[1].signal == NULL)) return;

    a = ch[0].signal->data;
    b = ch[1].signal->data;
    c = dest->data;

    dest->frame = ch[0].signal->frame + ch[1].signal->frame;
    dest->num = ch[0].signal->num;

    if (dest->num > ch[1].signal->num) dest->num = ch[1].signal->num;

    for (i = 0 ; i < dest->num ; i++) {
        *c++ = (*a++ + *b++) / 2;
    }
}

/* Fast Fourier Transform of channels 0 and 1
 *
 * The point of the dest->frame calculation is that the value changes whenever the data changes, but
 * if the data is constant, it doesn't change.  The display code only looks at changes in frame
 * number to decide when to redraw a signal; the actual value doesn't matter.
 */

void fft1(Signal *dest)
{
    if (ch[0].signal == NULL)
        return;
    if (in_progress != 0 || !scope.run)
        return;

    fftW(ch[0].signal->data, dest->data, ch[0].signal->width);
}

#ifndef FFT_TEST
void fft2(Signal *dest)
{
    if (ch[1].signal == NULL)
        return;
       
    if (in_progress != 0 || !scope.run)
        return;

    fftW(ch[1].signal->data, dest->data, ch[1].signal->width);
}

#else
void make_sin(short *data, int size, double freq, int rate)
{
    int i;
    for(i = 0; i < size; i++){
        data[i] = sin( 360.0 * ((1.0/((double)rate/freq)) * i) * M_PI / 180.0) * 100;
    }
}

void fft2(Signal *dest)
{
    int i;
    static short    *testdata = NULL;
    static int      testdataWidth = -1;
    
    if (ch[1].signal == NULL){
        fprintf(stderr, "fft2() ch[1].signal == NULL\n");
        return;
    }

    if (in_progress != 0 || !scope.run){
        return;
    }

    if(testdataWidth != ch[1].signal->width){
        if(testdata != NULL)
            free(testdata);
        testdataWidth = ch[1].signal->width;    
        testdata = malloc(testdataWidth * sizeof(short));
        make_sin(testdata, testdataWidth, 2500.0, ch[1].signal->rate);
    }

    fftW(testdata, dest->data, testdataWidth);

    for(i = 0; i< FFT_DSP_LEN - 20; i+=20){
        dest->data[i] = -80;
        if(i == 400)
            dest->data[i] = 80;
    }
}
#endif

/* isvalid() functions for the various math functions.
 *
 * These functions also have the side effect of setting the volts/rate fields in the Signal
 * structure, something we count on happening whenever we call update_math_signals(), which calls
 * these functions.
 *
 * These functions are also responsible for mallocing the data areas in the math function's
 * associated Signal structures.
 */

int ch1active(Signal *dest)
{
    dest->frame = 0;
    dest->num = 0;

    if (ch[0].signal == NULL) {
        dest->rate = 0;
        dest->volts = 0;
        return 0;
    }

    dest->rate = ch[0].signal->rate;
    dest->volts = ch[0].signal->volts;

    if (dest->width != ch[0].signal->width) {
        dest->width = ch[0].signal->width;
        if (dest->data != NULL)
            free(dest->data);
        dest->data = malloc(ch[0].signal->width * sizeof(short));
        if(dest->data == NULL){
            fprintf(stderr, "malloc failed in ch1active()\n");
            exit(0);
        }
    }

    return 1;
}

int ch2active(Signal *dest)
{
    dest->frame = 0;
    dest->num = 0;

    if (ch[1].signal == NULL) {
        dest->rate = 0;
        dest->volts = 0;
        return 0;
    }

    dest->rate = ch[1].signal->rate;
    dest->volts = ch[1].signal->volts;

    if (dest->width != ch[1].signal->width) {
        dest->width = ch[1].signal->width;
        if (dest->data != NULL)
            free(dest->data);
        dest->data = malloc(ch[1].signal->width * sizeof(short));
        if(dest->data == NULL){
            fprintf(stderr, "malloc failed in ch2active()\n");
            exit(0);
        }
    }

    return 1;
}

int chs12active(Signal *dest)
{
    dest->frame = 0;
    dest->num = 0;

    if ((ch[0].signal == NULL) || (ch[1].signal == NULL)
        || (ch[0].signal->rate != ch[1].signal->rate)
        || (ch[0].signal->volts != ch[1].signal->volts)) {
        dest->rate = 0;
        dest->volts = 0;
        return 0;
    }

    dest->rate = ch[0].signal->rate;
    dest->volts = ch[0].signal->volts;

    /* All of the associated functions (sum, diff, avg) only use the minimum of the samples on
     * Channels 1 and 2, so we can safely base the size of our data array on Channel 1 only... the
     * worst that can happen is that it is too big.
     */

    if (dest->width != ch[0].signal->width) {
        dest->width = ch[0].signal->width;
        if (dest->data != NULL)
            free(dest->data);
        dest->data = malloc(ch[0].signal->width * sizeof(short));
        if(dest->data == NULL){
            fprintf(stderr, "malloc failed in ch12active()\n");
            exit(0);
        }
    }
    return 1;
}

/* special isvalid() functions for FFT
 *
 * First, it allocates memory for the generated fft.
 *
 * Second, it sets the "rate", so that the increment from grid line to grid line is some "nice"
 * value.  (a muliple of 500 Hz if increment is > 1kHz, otherwise a multiple of 100 hZ.
 *
 * Third: this value is stored in the "volts" member of the dest signal structure. It is only
 * displayed in the label.
 */
int ch1FFTactive(Signal *dest)
{
    static int prevWidth = -1;

    if ((ch[0].signal != NULL) && (ch[0].signal->width != prevWidth)) {
        prevWidth = ch[0].signal->width;
        return(FFTactive(ch[0].signal, dest, TRUE));
    }
    return(FFTactive(ch[0].signal, dest, FALSE));
}

int ch2FFTactive(Signal *dest)
{
    static int prevWidth = -1;
    
    if ((ch[1].signal != NULL) && (ch[1].signal->width != prevWidth)) {
        prevWidth = ch[1].signal->width;
        return(FFTactive(ch[1].signal, dest, TRUE));
    }
    return(FFTactive(ch[1].signal, dest, FALSE));
}

struct func {
    void (*func)(Signal *);
    char *name;
    int (*isvalid)(Signal *);   /* returns TRUE if this function is valid */
    Signal signal;
};

struct func funcarray[] = {
    {inv1, "Inv. 1  ", ch1active},
    {inv2, "Inv. 2  ", ch2active},
    {sum,  "Sum  1+2", chs12active},
    {diff, "Diff 1-2", chs12active},
    {avg,  "Avg. 1,2", chs12active},
    {fft1, "FFT. 1  ", ch1FFTactive},
    {fft2, "FFT. 2  ", ch2FFTactive},
};

/* the total number of "functions" */
int funccount = sizeof(funcarray) / sizeof(struct func);

/* Cycle current scope chan to next function, taking heavy advantage of C incrementing pointers by
 * the size of the thing they point to.  Start by finding the current function in the function array
 * that the channel is pointing to, and advancing to the next one, selecting the first item in the
 * array if either we're currently pointing to the end of the array or pointing to something other
 * than a function.  Then keep going, looking for the first function that returns TRUE to an
 * isvalid() test, taking care that none of the functions may currently be valid.
 */

void next_func(void)
{
    struct func *func, *func2;
    Channel *chan = &ch[scope.select];

    for (func = &funcarray[0]; func < &funcarray[funccount]; func++) {
        if (chan->signal == &func->signal) break;
    }

    if (func == &funcarray[funccount]) func = &funcarray[0];
    else if (func == &funcarray[funccount-1]) func = &funcarray[0];
    else func ++;

    /* At this point, func points to the candidate function structure.  See if it's valid, and keep
     * going forward if it isn't
     */

    func2 = func;
    do {
        if (func->isvalid(&func->signal)) {
            recall(&func->signal);
            return;
        }
        func ++;
        if (func == &funcarray[funccount]) func = &funcarray[0];
    } while (func != func2);

    /* If we're here, it's because we went through all the functions without finding one that
     * returned valid.  No choice but to clear the channel.
     */

    recall(NULL);
}

/* Basically the same deal, but moving backwards in the array. */

void prev_func(void)
{
    struct func *func, *func2;
    Channel *chan = &ch[scope.select];

    for (func = &funcarray[0]; func < &funcarray[funccount]; func++) {
        if (chan->signal == &func->signal) break;
    }

    if (func == &funcarray[funccount]) func = &funcarray[funccount-1];
    else if (func == &funcarray[0]) func = &funcarray[funccount-1];
    else func --;

    /* At this point, func points to the candidate function structure.  See if it's valid and go
     * further backwards if it isn't
     */

    func2 = func;
    do {
        if (func->isvalid(&func->signal)) {
            recall(&func->signal);
            return;
        }
        func --;
        if (func < &funcarray[0]) func = &funcarray[funccount-1];
    } while (func != func2);

    /* If we're here, it's because we went through all the functions without finding one that
     * returned valid.  No choice but to clear the channel.
     */

    recall(NULL);
}

int function_bynum_on_channel(int fnum, Channel *ch)
{
    if ((fnum >= 0) && (fnum < funccount)) {
        recall_on_channel(&funcarray[fnum].signal, ch);
        return TRUE;
    }
    return FALSE;
}

/* Initialize math, called once by main at startup, and again whenever we read a file. */

void init_math(void)
{
    static int i;
    static int once = 0;

    for (i = 0 ; i < 26 ; i++) {
        if (once==1 && mem[i].data != NULL) {
            free(mem[i].data);
        }
        mem[i].data = NULL;
        mem[i].num = mem[i].frame = mem[i].volts = 0;
        mem[i].listeners = 0;
        sprintf(mem[i].name, "Memory %c", 'a' + i);
        mem[i].savestr[0] = 'a' + i;
        mem[i].savestr[1] = '\0';

        mem_pending[i] = -1;
    }
    for (i = 0; i < funccount; i++) {
        strcpy(funcarray[i].signal.name, funcarray[i].name);
        funcarray[i].signal.savestr[0] = '0' + i;
        funcarray[i].signal.savestr[1] = '\0';
    }
    once=1;
}

/* update_math_signals() is called whenever 'something' has changed in the scope settings, and we
 * may need to recompute voltage and rate values for the generated math functions.  We do this by
 * calling all the isvalid() functions for those math functions that have listeners, and return 0 if
 * everything's OK, or -1 if some of them are no longer valid.
 *
 * XXX I'm still not completely clear on just what we should do with invalid channels that have
 * listeners; don't want to arbitrarily clear them (I don't think), because then a single
 * inadvertent keystroke on channel 0 or 1 might clear a bunch of math.
 */

int update_math_signals(void)
{
    int i;
    int retval = 0;

    for (i = 0; i < funccount; i++) {
        if (funcarray[i].signal.listeners > 0) {
            if (! funcarray[i].isvalid(&funcarray[i].signal)) retval = -1;
        }
    }

    return retval;
}

/* Perform any math on the software channels, called many times by main loop */

void do_math(void)
{
    static int i;

    for (i = 0; i < funccount; i++) {
        if (funcarray[i].signal.listeners > 0 && funcarray[i].isvalid(&funcarray[i].signal)) {
            funcarray[i].func(&funcarray[i].signal);
        }
    }

    run_externals();

}

/* Perform any math cleanup, called once by cleanup at program exit */

void cleanup_math(void)
{
    EndFFTW();
}

/* measure the given channel */

void measure_data(Channel *sig, struct signal_stats *stats)
{
    int     i, k;
    short   val, prev;
    int     min=0, max=0, midpoint=0;
    int     first = 0, last = 0, count = 0, imax = 0;
#if CALC_RMS
    int     second = 0.0;
#endif	
    stats->min = 0;
    stats->max = 0;
    stats->time = 0;
    stats->freq = 0;
#if CALC_RMS
    stats->rms = 0.0;
#endif

    if ((sig->signal == NULL) || (sig->signal->num == 0))
        return;

    /* XXX these calculations could probably overrun the data[] array if the cursors are not set
     * sensibly
     */

    prev = 1;
    if (scope.curs) {           /* manual cursor measurements */
        if (scope.cursa < scope.cursb) {
            first = scope.cursa;
            last = scope.cursb;
        } else {
            first = scope.cursb;
            last = scope.cursa;
        }
        stats->min = stats->max = sig->signal->data[(int)first];
        if ((val = sig->signal->data[(int)last]) < stats->min)
            stats->min = val;
        else if (val > stats->max)
            stats->max = val;
        count = 2;
    } else {                    /* automatic period measurements */
        min = max = sig->signal->data[0];
        for (i = 0 ; i < sig->signal->num ; i++) {
            val = sig->signal->data[i];
            if (val < min)
                min = val;
            if (val > max) {
                max = val;
                imax = i;
            }
        }

        /* locate and count rising edges
         * tries to handle handle noise by looking at the next 10 values.
         */
        midpoint = (min + max)/2;
        prev = max;
        for (i = 0; i < sig->signal->num; i++) {
            val = sig->signal->data[i];
            if (val > midpoint && prev <= midpoint){
                int rising = 0;
                for(k = i + 1; k < i + 11 && k < sig->signal->num; k++) {
                    if(sig->signal->data[k] > val){
                        rising++;
                     }
                 }   
                if(rising > 5){
                    if (!first)
                        first = i;
#if CALC_RMS
                    else if (!second)
                        second = i;
#endif
                    last = i;
                    i = k;
                    count++;
                }
            }
            prev = val;
        }

#if CALC_RMS
        for (i = first; i < second; i++) {
            stats->rms += (sig->signal->data[i] * sig->signal->data[i]);
        }
        if((second - first) != 0){
            stats->rms = sqrt(stats->rms / (second - first));
        }
        else {
            stats->rms = -1;
        }
#endif

        stats->min = min;
        stats->max = max;
    }

    if (sig->signal->rate < 0) {

        /* Special case for FFT - signal rate will be < 0, the negative of the frequency step for
         * each point in the transform, times 10.  So multiply by the index of the maximum value to
         * get frequency peak.
         */

        stats->freq = (- sig->signal->rate) * imax / 10;
        if (stats->freq > 0)
            stats->time = 1000000 / stats->freq;

    } 
    else if ((sig->signal->rate > 0) && (count > 1)) {
        /* estimate frequency from rising edge count
         * assume a wave: period = length / # periods
         */
        stats->time = (int)(1000000.0 * (last - first) / (count - 1) / sig->signal->rate);
        if (stats->time > 0){
            stats->freq = (int)((1000000.0 / stats->time) +0.5);
        }
    }
}
