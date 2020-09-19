/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the file and command-line I/O
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xoscope.h"            /* program defaults */
#include "display.h"            /* display routines */
#include "func.h"               /* signal math functions */

int backwards_compat_1_10 = 0;  /* TRUE if parsing a pre-1.10 save file */
int backwards_compat_2_0 = 0;   /* TRUE if parsing a pre-2.0 save file */
int backwards_compat_2_1 = 0;   /* TRUE if parsing a pre-2.1 save file */

/* force num to stay within the range lo - hi */
int limit(int num, int lo, int hi)
{
    if (num < lo) {
        num = lo;
    } else if (num > hi) {
        num = hi;
    }
    return(num);
}

/* parse command-line or file opt character, using given optarg string
 *
 * optarg can have a trailing newline, if we're reading from a file
 */

void handle_opt(int opt, char *optarg)
{
    char *p, *q;
    char buf[16];
    Channel *s;

    switch (opt) {
    case 'D':                   /* data source selection */
        if ((p = index(optarg, '\n')) != NULL) {
            *p = '\0';
        }
        if (!datasrc_byname(optarg)) {
            fprintf(stderr, "Couldn't find data source %s\n\n", optarg);
            usage(1);
        }
        break;
    case 'o':                   /* data source specific option */
    case 'O':
        /* Older data file format used -o for BitScope only, and printed BitScope options even if
         * BitScope wasn't enabled, so if we're reading an older file, never process -o
         */
        if ((datasrc != NULL) && !backwards_compat_1_10) {
            if ((datasrc->set_option == NULL)
                || (datasrc->set_option(optarg) == 0)) {
                fprintf(stderr, "Couldn't set option %s\n\n", optarg);
                usage(1);
            }
        }
        break;
    case 'r':                   /* soundcard sample rate - deprecated */
    case 'R':
        if ((datasrc != NULL) && !strcasecmp(datasrc->name, "Soundcard")
            && (datasrc->set_option != NULL)) {
            snprintf(buf, sizeof(buf), "rate=%s", optarg);
            datasrc->set_option(buf);
        }
        break;
    case 's':                   /* scale (zoom) */
    case 'S':
        {
            int num = 1, den = 1;
            if(sscanf(optarg, "%d/%d", &num, &den) > 0){
                num = limit(num, 1, 2000);
                den = limit(den, 1, 500000);
                scope.scale = (double)num / (double)den;
            }
            else{
                fprintf(stderr, "Couldn't set scale to %s\n\n", optarg);
                usage(1);
            }
        }

        break;
    case 't':                   /* trigger */
    case 'T':
        scope.trig = limit(strtol(p = optarg, NULL, 0), 0, 255);
        if ((q = strchr(p, ':')) != NULL) {
            scope.trige = limit(strtol(++q, NULL, 0), 0, 2);
            p = q;
        }
        if ((q = strchr(p, ':')) != NULL) {
            if (*(++q) == 'x' || *q == 'X') {
                scope.trigch = 0;
            } else if (*q == 'y' || *q == 'Y') {
                scope.trigch = 1;
            } else {
                scope.trigch = strtol(q, NULL, 0);
            }
        }
        if (datasrc && datasrc->set_trigger
            && datasrc->set_trigger(scope.trigch,
                                    &scope.trig, scope.trige) == 0) {
            /* Unable to set trigger, so clear it */
            if (datasrc && datasrc->clear_trigger) {
                datasrc->clear_trigger();
            }
            scope.trige = 0;
        }
        break;
    case 'l':                   /* cursor lines */
    case 'L':
        scope.curs = 1;
        scope.cursa = limit(strtol(p = optarg, NULL, 0), 1, MAXWID);
        if ((q = strchr(p, ':')) != NULL) {
            scope.cursb = limit(strtol(++q, NULL, 0), 1, MAXWID);
            p = q;
        }
        if ((q = strchr(p, ':')) != NULL)
            scope.curs = limit(strtol(++q, NULL, 0), 0, 1);
        break;
    case 'd':                   /* dma divisor (backwards compatibility) */
        if (datasrc && !strcasecmp(datasrc->name, "Soundcard")) {
            snprintf(buf, sizeof(buf), "dma=%s", optarg);
            handle_opt('o', buf);
        }
        break;
    case 'f':                   /* font name */
    case 'F':
        strcpy(fontname, optarg);
        break;
    case 'p':                   /* plotting mode */
    case 'P':
        if(backwards_compat_2_1){
            scope.plot_mode = limit(strtol(optarg, NULL, 0), 0, 5) / 2;
            scope.scroll_mode = limit(strtol(optarg, NULL, 0), 0, 5) % 2;
        }
        else{
            scope.plot_mode = limit(strtol(optarg, NULL, 0) / 10, 0, 2);
            scope.scroll_mode = limit(strtol(optarg, NULL, 0) % 10, 0, 2);
        }
        break;
    case 'g':                   /* graticule on/off */
    case 'G':
        scope.grat = limit(strtol(optarg, NULL, 0), 0, 2);
        break;
    case 'b':                   /* behind/front */
    case 'B':
        scope.behind = !DEF_B;
        break;
    case 'v':                   /* verbose display */
    case 'V':
        scope.verbose = !DEF_V;
        break;
    case 'i':                   /* minimum update interval */
    case 'I':
        scope.min_interval = strtol(optarg, NULL, 0);
        break;
    case 'x':                   /* sound card (backwards compatibility) */
    case 'X':
    case 'y':
    case 'Y':
        break;
    case 'z':                   /* serial scope (backwards compatibility) */
    case 'Z':
        break;
    case 'a':                   /* Active (selected) channel */
        scope.select = limit(strtol(optarg, NULL, 0) - 1, 0, CHANNELS - 1);
        break;
#ifdef HAVE_LIBASOUND
    case 'A':
        sprintf(alsaDevice, "%s", optarg);
        break;
#endif
    case 'h':                   /* help */
    case 'H':
        usage(0);
        break;
    case ':':                   /* unknown option */
    case '?':
        usage(1);
        break;
    default:                    /* channel settings */
        if (opt >= '1' && opt <= '0' + CHANNELS) {
            s = &ch[opt - '1'];
            p = optarg;
            s->show = 1;
            if (*p == '+') {
                s->show = 0;
                p++;
            }

            /* Prior to the 2.0 release, signal position was stored as the number of y pixels to
             * offset.  Now it's stored as 100 times the decimal fraction s->pos, which is a
             * floating point number ranging nominally from -1 (bottom of screen) to +1 (top).  The
             * data part of the display used to be 160 pixels less than v_points, so for a 640x480
             * display (the most common), the data part was 320 pixels tall, so we divide by 160,
             * negative since the old number's zero point was at the top.
             */

            s->pos = limit(-strtol(p, NULL, 0), -1280, 1280);

            if (backwards_compat_2_0) {
                s->pos /= -160;
            } else {
                s->pos /= 100;
            }

            if ((q = strchr(p, '.')) != NULL) {
                s->bits = limit(strtol(++q, NULL, 0), 0, 16);
                p = q;
            }

            /* The scale can be specified as either a floating point number
             * (new format), or a ratio like 1/100 (old format).
             */

            if ((q = strchr(p, ':')) != NULL) {
                s->scale = strtod(++q, NULL);
                p = q;
            }
            if ((q = strchr(p, '/')) != NULL) {
                s->scale /= limit(strtol(++q, NULL, 0), 1, 100);
                p = q;
            }

            if ((q = strchr(p, ':')) != NULL) {
                if (*++q >= '0' && *q <= '9') {
                    if (*q >= '0') {
                        /* Older versions used '0' for an empty channel and offset function numbers by 1 */
                        int function_number = strtol(q, NULL, 0);
                        if (! backwards_compat_1_10) {
                            function_bynum_on_channel(function_number, s);
                        } else if (function_number > 0) {
                            function_bynum_on_channel(function_number-1, s);
                        }
                    }
                } else if (*q >= 'a' && *q <= 'z'
                           && (*(q + 1) == '\0' || *(q + 1) == '\n')) {

                    /* Older versions used x, y, z for data channels, now we use a, b, c, etc,
                     * a change required by COMEDI which can have more than three channels.
                     *
                     * Probescope used z exclusively, but we no longer support this device.
                     */

                    if (backwards_compat_1_10) {
                        if (*q >= 'x') {
                            if (datasrc && (*q != 'z')) {
                                recall_on_channel(datasrc->chan(*q - 'x'), s);
                            }
                        } else {
                            recall_on_channel(&mem[*q - 'a'], s);
                        }
                    } else {
                        if (datasrc && ((*q - 'a') < datasrc->nchans())) {
                            recall_on_channel(datasrc->chan(*q - 'a'), s);
                        } else {
                            recall_on_channel(&mem[*q - 'a'], s);
                        }
                    }

                } else {
                    start_command_on_channel(q, s);
                }
            }
        }
        break;
    }
}

char *
formatScale(double fScale)
{
    int         num = 1, den = 1;
    static char buf[32];
    
    if(fScale >= 1.0){
        num = (int)fScale;
    }
    else{
        den = (int)(1.0 / fScale);
    }
     
    sprintf(buf, "%d/%d", num, den);   
    return(buf);
}

   


/* write scope settings and memory buffers to given filename */
void writefile(char *filename)
{
    FILE *file;
    int i, j, k = 0, l = 0, chan[26];
    char *s;
    Channel *p;

    if ((file = fopen(filename, "w")) == NULL) {
        sprintf(error, "%s: can't write %s", progname, filename);
        message(error);
        return;
    }
    fprintf(file, "# %s, version %s\n#\n", progname, version);

    if (datasrc) {
        fprintf(file, "# -D %s\n", datasrc->name);

        if (datasrc->save_option != NULL) {
            for (i=0; (s = datasrc->save_option(i)) != NULL; i++) {
                if (s[0] != '\0') {
                    fprintf(file, "# -o %s\n", s);
                }
            }
        }
    }

    fprintf(file, "# -a %d\n\
# -s %s\n\
# -t %d:%d:%d\n\
# -l %d:%d:%d\n\
# -p %d\n\
# -g %d\n\
%s%s",
            scope.select + 1,
            formatScale(scope.scale),
            scope.trig - 128, scope.trige, scope.trigch,
            scope.cursa, scope.cursb, scope.curs,
            /* XXX fix this - plot_mode not backwards compatable anymore */
            /* XXX fix this - plot_mode now OK, but scope.scroll_mode = 2 not stored in file*/
            /* new pre-2.1 compatibility flag - plot_mode and scope.scroll_mode now OK*/
            (scope.plot_mode * 10) + scope.scroll_mode,
            scope.grat,
            scope.behind ? "# -b\n" : "",
            scope.verbose ? "# -v\n" : "");
    for (i = 0 ; i < CHANNELS ; i++) {
        p = &ch[i];
        if (p->signal) {
            fprintf(file, "# -%d %s%d.%d:%f:%s\n", i + 1, p->show ? "" : "+",
                    -(int)(p->pos*100), p->bits, p->scale, p->signal->savestr);
        }
    }
    /* XXX code need to be carefully checked out */
    for (i = 0 ; i < 26 ; i++) {
        if (mem[i].num > 0) {
            chan[k++] = i;
        }
        if (mem[i].num > l) {
            l = mem[i].num;
        }
    }
    if (k) {
        for (i = 0 ; i < k ; i++) {
            /* XXX color written is channel color (it can't be changed) */
#if 0
            fprintf(file, "%s%c(%02d)", i ? "\t" : "# ",
                    chan[i] + 'a', roloc[mem[chan[i]].color]);
#else
            fprintf(file, "%s%c(%02d)", i ? "\t" : "# ",
                    chan[i] + 'a', 15);
#endif
        }
        for (i = 0 ; i < k ; i++) {
            fprintf(file, "%s%d", i ? "\t" : "\n#:", mem[chan[i]].rate);
        }
        for (i = 0 ; i < k ; i++) {
#if SC_16BIT
            fprintf(file, "%s%f", i ? "\t" : "\n#=", mem[chan[i]].volts);
#else
            fprintf(file, "%s%d", i ? "\t" : "\n#=", mem[chan[i]].volts);
#endif
        }
        fprintf(file, "\n");
        for (j = 0 ; j < l ; j++) {
            for (i = 0 ; i < k ; i++) {
                fprintf(file, "%s%d", i ? "\t" : "", mem[chan[i]].data[j]);
            }
            fprintf(file, "\n");
        }
    }
    fclose(file);
    sprintf(error, "wrote %s", filename);
    message(error);
}

/* read scope settings and memory buffers from given filename */
void readfile(char *filename)
{
    FILE *file;
    char c, *p, *q, buff[256];
    int version[2];
    int i = 0, j = 0, k, valid = 0, chan[26] =
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
#if SC_16BIT
    double lf;
#endif
    if ((file = fopen(filename, "r")) == NULL) {
        sprintf(error, "%s: can't read %s", progname, filename);
        message(error);
        return;
    }
    init_scope();                       /* reset everything */
    init_channels();
    init_math();
    while (fgets(buff, 256, file)) {
        if (buff[0] == '#') {
            if (sscanf(buff, "# %*s version %d.%d", &version[0], &version[1]) == 2) {
                if ((version[0] <= 1) && (version[1] < 10)) {
                    backwards_compat_1_10 = 1;
                }
                if (version[0] == 1) {
                    backwards_compat_2_0 = 1;
                    backwards_compat_2_1 = 1;
                }
                if (version[0] == 2 && version[1] == 0) {
                    backwards_compat_2_1 = 1;
                }
                valid = 1;
            } else if (!strncmp("# -", buff, 3)) {
                /* valid = 1; */
                handle_opt(buff[3], &buff[5]);
            } else if (valid && buff[3] == '(' && buff[6] == ')') {
                j = 0;
                q = buff + 2;
                while (j < 26 && (sscanf(q, "%c(%d) ", &c, &k) == 2)) {
                    if (c >= 'a' && c <= 'z' && k >= 0 && k <= 255) {
                        chan[j++] = c - 'a';
                        //XXX 'k' is color and it is ignored
                        //mem[c - 'a'].color = color[k];
                        mem[c - 'a'].frame ++;
                        /* Guess some multiple of two here for our data size.
                         * We'll adjust this up if needed.
                         */
                        mem[c - 'a'].data = malloc(16 * sizeof(short));
                        mem[c - 'a'].width = 16;
                    }
                    q += 6;
                }
            } else if (!strncmp("#:", buff, 2)) {
                j = 0;
                q = buff + 2;
                while (q && j < 26 && (sscanf(q, "%d ", &k) == 1)) {
                    mem[chan[j++]].rate = k;
                    q = strchr(++q, '\t');
                }
            } else if (!strncmp("#=", buff, 2)) {
                j = 0;
                q = buff + 2;
#if SC_16BIT
                while (q && j < 26 && (sscanf(q, "%lf ", &lf) == 1)) {
                    mem[chan[j++]].volts = lf;
                    q = strchr(++q, '\t');
                }
#else
                while (q && j < 26 && (sscanf(q, "%d ", &k) == 1)) {
                    mem[chan[j++]].volts = k;
                    q = strchr(++q, '\t');
                }
#endif
            }
        } else if (valid &&
                   ((buff[0] >= '0' && buff[0] <= '9') || buff[0] == '-')) {

            /* This is a line of memory data.  Integer values, separated by tabs, and we already
             * read into the chan[] array the mapping between the tab-separated fields and memory
             * channels.  Take care to dynamically resize the memory channel data arrays.
             */

            j = 0;              /* Field number (starts at 0) */
            p = buff;           /* Pointer to tab that starts the field (or buff) */

            while (j < 26 && (!j || ((p = strchr(p, '\t')) != NULL))) {
                if (chan[j] > -1) {
                    if (mem[chan[j]].width <= i) {
                        while (mem[chan[j]].width <= i) {
                            mem[chan[j]].width *= 2;
                        }
                        mem[chan[j]].data = realloc(mem[chan[j]].data,
                                                    mem[chan[j]].width * sizeof(short));
                    }
                    mem[chan[j]].data[i] = strtol(p, NULL, 0);
                    mem[chan[j]].num = i + 1;

                    p ++;
                    j ++;
                }
            }
            i++;
        }
    }
    fclose(file);

    /* If we read any memory channels, set their widths to be exactly the number of data values in
     * them.  We do this so we get an accurate display indication of the number of samples.
     */

    for (j=0; j<26; j++) {
        if (chan[j] != -1) {
            mem[chan[j]].width = mem[chan[j]].num;
        }
    }

    backwards_compat_1_10 = 0;  /* in case we set these during the parse */
    backwards_compat_2_0 = 0;
    backwards_compat_2_1 = 0;
    if (valid) {
        clear();                /* XXX not sure if we need this */
        sprintf(error, "loaded %s", filename);
        message(error);
    } else {
        sprintf(error, "invalid format: %s", filename);
        message(error);
    }
}
