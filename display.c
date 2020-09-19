/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the UI-independent display code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <ctype.h>
#include "xoscope.h"            /* program defaults */
#include "display.h"
#include "func.h"

#include "xoscope_gtk.h"
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gtkdatabox.h>
#include <gtkdatabox_points.h>
#include <gtkdatabox_lines.h>
#include <gtkdatabox_grid.h>
#include <gtkdatabox_markers.h>

extern GtkWidget *databox;

#define DEBUG 0

int     triggered = 0;          /* whether we've triggered or not */
void    *font;
int     math_warning = 0;       /* TRUE if math has a problem */

struct signal_stats stats;

/* message() - draw a temporary one-line message to center of screen
 *
 * XXX actually draws into the databox, which means that if we scroll the databox, the message
 * moves.
 *
 * XXX multiple messages should be centered individually
 *
 * XXX multiple messages should timeout independently
 *
 * XXX allow user configuration of timeout period (current 2 sec)
 *
 * XXX possible threading race condition if we're in clear_message_callback() and message()
 * simultaneously
 */

GtkDataboxGraph *databox_message = NULL;
char *databox_message_text = NULL;
int databox_message_text_alloced_size = 0;
int databox_message_text_used = 0;
gfloat databox_message_X = 0.0;
gfloat databox_message_Y = 0.0;
int databox_message_timeout_ms = 2000;

gboolean clear_message_callback(gpointer ignored)
{
    gtk_databox_graph_remove (GTK_DATABOX(databox), databox_message);
    databox_message_text_used = 0;
    gtk_widget_queue_draw (databox);
    return FALSE;
}

void message(const char *message)
{
    if (databox_message == NULL) {
        GdkColor gcolor;
        gcolor.red = gcolor.green = gcolor.blue = 65535;
        databox_message = gtk_databox_markers_new(1, &databox_message_X,
                                                  &databox_message_Y, &gcolor, 0,
                                                  GTK_DATABOX_MARKERS_NONE);
    }

    if (databox_message_text_alloced_size < databox_message_text_used + strlen(message) + 2) {
        databox_message_text_alloced_size = databox_message_text_used + strlen(message) + 2;
        databox_message_text = realloc(databox_message_text, databox_message_text_alloced_size);
    }

    if (databox_message_text_used > 0) {
    databox_message_text[databox_message_text_used ++] = '\n';
    }
    strcpy(databox_message_text + databox_message_text_used, message);
    databox_message_text_used += strlen(message);

    /* XXX gtk_databox_markers_set_label() should take a const char pointer */

    gtk_databox_markers_set_label(GTK_DATABOX_MARKERS(databox_message), 0,
                                  GTK_DATABOX_MARKERS_TEXT_CENTER, (char *)databox_message_text, FALSE);

    if (databox_message_text_used == strlen(message)) {
        gtk_databox_graph_add (GTK_DATABOX(databox), databox_message);
    g_timeout_add (databox_message_timeout_ms, clear_message_callback, NULL);
}

    gtk_widget_queue_draw (databox);
}

/* Format a number into a string using SI prefixes.  "fmt" should contain a "%g" for the number and
 * a "%s" for the prefix, i.e:
 *          "%g %sV"
 */

void SIformat(char *buf, const char *fmt, double num, int roundoff)
{
    int power=0;
    int sign = num >= 0.0 ? 1 : -1;

    num *= sign;

    /* Round off num to nearest .1% */

    if (num > 0) {
        while (num > 1000) num /= 10, power ++;
        while (num < 100) num *= 10, power --;
    }
    if(roundoff)
        num = rint(num);

    /* Special case to make sure we get "1 ms/div" and not "1000 us/div" */
    if (num == 1000) num /= 10, power ++;

    /* 'num' is now between 100 and 1000; original num is (num * 10^power) */

    switch (power+1) {
    case -13:
    case -12:
    case -11:
        sprintf(buf, fmt, num * pow(10.0, power+12) * sign, "p");
        break;

    case -10:
    case -9:
    case -8:
        sprintf(buf, fmt, num * pow(10.0, power+9) * sign, "n");
        break;

    case -7:
    case -6:
    case -5:
        /* use UTF-8 micro sign */
        sprintf(buf, fmt, num * pow(10.0, power+6) * sign, "\302\265");
        break;

    case -4:
    case -3:
    case -2:
        sprintf(buf, fmt, num * pow(10.0, power+3) * sign, "m");
        break;

    case -1:
    case 0:
    case 1:
    default:
        /* This is a reasonable default, since %g will use scientific
         * notation if the exponent is less than -4 or greater than 5.
         *
         * To avoid the "pulsating" of text that is updated at every
         * sweep when a value oscillates around the change of magnitude,
         * I want a constant string length.
         * Part of this is approach is to keep the length of the unit constant.
         * Therfore I insert a blank after the basic unit if we dont use a prefix.
         */
        {
            char tmp_fmt[80], *cp;

            strcpy(tmp_fmt, fmt);
            cp = strstr(tmp_fmt, "%s") + 2;
            while(*cp && isalpha(*cp++))
            ;
            if(*cp)
                cp--;
            memmove(cp + 1, cp, strlen(cp) + 1);
            *cp = ' ';
            sprintf(buf, tmp_fmt, num * pow(10.0, power) * sign, "");
        }
        break;

    case 2:
    case 3:
    case 4:
        sprintf(buf, fmt, num * pow(10.0, power-3) * sign, "k");
        break;

    case 5:
    case 6:
    case 7:
        sprintf(buf, fmt, num * pow(10.0, power-6) * sign, "M");
        break;
    }
}

void make_help_text_visible(GtkWidget *widget, gpointer ignored)
{
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), make_help_text_visible, NULL);
    } else {
        const gchar * name = gtk_widget_get_name(widget);
        if ((name != NULL) && (strlen(name) >= 11) &&
            (!strcmp(name + strlen(name) - 11, "_help_label")
             || !strcmp(name + strlen(name) - 10, "_key_label"))) {
            gtk_label_set_text(GTK_LABEL(widget), g_object_get_data(G_OBJECT(widget), "visible-text"));
        }
    }
}

void make_help_text_invisible(GtkWidget *widget, gpointer ignored)
{
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), make_help_text_invisible, NULL);
    } else {
        const gchar * name = gtk_widget_get_name(widget);
        if ((name != NULL) && (strlen(name) >= 11) &&
            (!strcmp(name + strlen(name) - 11, "_help_label")
             || !strcmp(name + strlen(name) - 10, "_key_label"))) {
            gtk_label_set_markup(GTK_LABEL(widget), g_object_get_data(G_OBJECT(widget), "invisible-text"));
        }
    }
}

/* We have to copy the saved_text as well as the modified_text because the text will be changed
 * later (as we make it visible or invisible) and that will invalidate the pointer returned from
 * gtk_label_get_label().
 *
 * XXX free old data please
 */

void setup_help_text(GtkWidget *widget, gpointer ignored)
{
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), setup_help_text, NULL);
    } else {
        const gchar * name = gtk_widget_get_name(widget);
        if ((name != NULL) && (strlen(name) >= 11) &&
            (!strcmp(name + strlen(name) - 11, "_help_label")
             || !strcmp(name + strlen(name) - 10, "_key_label"))) {
            const gchar * text = gtk_label_get_label(GTK_LABEL(widget));
            gchar * saved_text = malloc(sizeof(gchar) * 80);
            gchar * modified_text = malloc(sizeof(gchar) * 80);

            sprintf(modified_text, "<span foreground=\"black\">%s</span>",
                    g_markup_escape_text(text, -1));
            strcpy(saved_text, text);
            g_object_set_data(G_OBJECT(widget), "visible-text", saved_text);
            g_object_set_data(G_OBJECT(widget), "invisible-text", modified_text);

        }
    }
}

/* Formatting of dynamic text:
 * 
 * If a value fluctuates around the change of the magnitude and/or the change 
 * of the prefix of the unit, the lentht of the period_label and the min_max_label
 * changes which makes it kind of "pulsating".
 * Beside from poor visual, it makes it hard to read.
 * Several factors contribute to this effect:
 * - Incorrect or missing width specifictions for printf (easy to eliminate)
 * - Difference in length between bas unit and unit with prefix ("V" compared to "mV")
 *   I modified SIformat to eliminate this.
 * - Use of a proportional font. 
 *   Even with the above factors eliminated a blank ist "shorter" than a digit or letter.
 *   The only solution that came to my mind was using a monospace font
 * 
 * Additionally the period_label now uses SIformat to the display period length.
 * This avoids huge numbers on very long periods (i.e. 20 ms instead of 20000 us). 
 * Perhaps using SIformat for the frequency too is not a very good idea, as the highest frequency
 * when using a sound card as input device is in the kHz range. 
 * But I dont know what frquency range to expect from a comedi device.
 *
 * If signal->volts is 0, we display steps for min, max and peak/peak
 * The required width for printf differs in 8 and 16 bit mode 
 * The defines below set the width accordingly.
 */

#if SC_16BIT
#define INTW    6   // min, max: 5 digits plus + or - sign
#define UINTW   5   // peak/peak: 5 digits no sign as this value is always positve
#define FLW     8   // 5 digits plus + or - sign, decimal point and 2 digits precision
#define FLPREC  2   // 2 digits precision
#else
#define INTW    4   // min, max: 3 digits plus + or - sign
#define UINTW   3   // peak/peak: 3 digits no sign as this value is always positve
#define FLW     6   // 3 digits plus + or - sign, decimal point and 2 digits precision
#define FLPREC  2   // 2 digits precision
#endif

/* Text update - the 'dynamic' text is unpredictable and is updated on every sweep.  Most of the
 * text only changes when the user hits a key or something; updating it requires a call to
 * update_text()
 */

void update_dynamic_text(void)
{
    static time_t prev = 0;
    static int frames = 0;
    char string[81], widget[81], *cp;
    const char *s;
    int i;
    time_t sec;
    Channel *p;

    p = &ch[scope.select];

    /* always draw the dynamic text, if signal is analog (bits == 0) */
    if (p->signal && (p->signal->bits == 0) && (p->signal->rate > 0)) {
        cp = string;
        SIformat(cp, "<tt>Period of %#5.4g %ss = ", (double)stats.time / 1000000.0, FALSE);
        cp = string + strlen(string);
        SIformat(cp, "%#5.4g %sHz</tt>", (double)stats.freq, FALSE);
        sprintf(cp, "%5d Hz</tt>", stats.freq);
        gtk_label_set_markup(GTK_LABEL(LU("period_label")), string);

        cp = string;
        if (p->signal->volts){
            SIformat(cp, "<tt>%+#.4g %sV - ", 
                    (double)stats.max * p->signal->volts / (320 * 1000), FALSE);
            cp = string + strlen(string);
            SIformat(cp, "%+#.4g %sV = ", 
                    (double)stats.min * p->signal->volts / (320 * 1000), FALSE);
            cp = string + strlen(string);
            SIformat(cp, "%#.4g %sV", 
                    ((double)stats.max - stats.min) * p->signal->volts / (320 * 1000), FALSE);
#if CALC_RMS
            cp = string + strlen(string);
            if(stats.rms > 0.0){
                SIformat(cp, " %#.4g %sV RMS", 
                        stats.rms * p->signal->volts / (320 * 1000), FALSE);
            }
            else
                sprintf(cp, "      --- RMS");
#endif
           strcat(cp, "</tt>");
        }
        else{
            sprintf(cp, "<tt>Max:%+*d - Min:%+*d = %*d Pk-Pk",
                        INTW, stats.max, INTW, stats.min, UINTW, stats.max - stats.min);
#if CALC_RMS
            cp = string + strlen(string);
            if(stats.rms > 0)
                sprintf(cp, " %*.*f RMS", FLW, FLPREC, stats.rms);
            else
                sprintf(cp, " %*s RMS", FLW, "---");
#endif
           strcat(cp, "</tt>");
        }
        gtk_label_set_markup(GTK_LABEL(LU("min_max_label")), string);
    }
    else if (p->signal && (p->signal->bits == 0) && (p->signal->rate < 0)) {
         /* Special case for a Fourier Transform.  ch[i].signal->rate is negative.  The
          * x-scaling for a FFT (Hz/div) is calculated in chXFFTactive() and rounded to some
          * "nice" value.  This value is stored in the volts member ofthe signal structure.
          * Not nice, but I didn't want to add a new member.
          */
        sprintf(string, "FFT");
        gtk_label_set_text(GTK_LABEL(LU("period_label")), string);

        if (p->signal->volts)
            SIformat(string, "%g %sHz/div", p->signal->volts, TRUE);
        else
            string[0] = '\0';
        gtk_label_set_text(GTK_LABEL(LU("min_max_label")), string);

    } 
    else {
        gtk_label_set_text(GTK_LABEL(LU("period_label")), "");
        gtk_label_set_text(GTK_LABEL(LU("min_max_label")), "");
    }

    if (math_warning) {
#if 0
#if 0
        sprintf(string, "WARNING: math(%d,%d) is bogus!",
                ch[0].signal->rate, ch[1].signal->rate);
        text_write(string, 40, 4, 0, KEY_FG, TEXT_BG, ALIGN_CENTER);
#else
        text_write("WARNING: math is bogus!", 40, 4,
                   0, KEY_FG, TEXT_BG, ALIGN_CENTER);
#endif
#endif
    }

    /* We don't know when our data source might change its status lines, so we update these widgets
     * every time through.
     */

    if (datasrc && (datasrc->option1str != NULL)
        && ((s = datasrc->option1str()) != NULL)) {
        gtk_label_set_text(GTK_LABEL(LU("data_source_opt1_label")), s);
    } else {
        gtk_label_set_text(GTK_LABEL(LU("data_source_opt1_label")), "");
    }

    if (datasrc && (datasrc->option2str != NULL)
        && ((s = datasrc->option2str()) != NULL)) {
        gtk_label_set_text(GTK_LABEL(LU("data_source_opt2_label")), s);
    } else {
        gtk_label_set_text(GTK_LABEL(LU("data_source_opt2_label")), "");
    }

    if (datasrc && datasrc->status_str != NULL) {
        for (i=0; i<8; i++) {
            sprintf(widget, "status%d_label", i);
            if ((s = datasrc->status_str(i)) != NULL) {
                gtk_label_set_text(GTK_LABEL(LU(widget)), s);
            } else {
                gtk_label_set_text(GTK_LABEL(LU(widget)), "");
            }
        }
    } else {
        for (i=0; i<8; i++) {
            sprintf(widget, "status%d_label", i);
            gtk_label_set_text(GTK_LABEL(LU(widget)), "");
        }
    }

    /* Recompute frames per second once every second */

    time(&sec);
    if (sec != prev) {

        if (prev != 0) {
            sprintf(string, "fps:%3d", frames);
            gtk_label_set_text(GTK_LABEL(LU("fps_label")), string);
        } else {
            gtk_label_set_text(GTK_LABEL(LU("fps_label")), "");
        }

        frames = 0;
        if (datasrc) {
            prev = sec;
        } else {
            prev = 0;
        }
    }

    frames++;
}

void update_text(void)
{
    char string[81], widget[81];
    int i;
    Channel *p;
    static char *plot_styles[] = {
        "Point",
        "Line",
        "Step",
    };
    static char *scroll_styles[] = {
        "",
        "Accum",
        "Strip",
    };
    static char *trigs[] = {
        "Auto",
        "Rising",
        "Falling"
    };

    p = &ch[scope.select];

    /* above graticule */

    /* progname and version dynamic */

    /* setting help text is special */
    gtk_label_set_text(GTK_LABEL(LU("graticule_position_help_label")),
                       scope.behind ? "Behind" : "In Front");
    setup_help_text(GTK_WIDGET(LU("graticule_position_help_label")), NULL);


    if (!datasrc) {
        gtk_label_set_text(GTK_LABEL(LU("trigger_label")), "");
        gtk_label_set_text(GTK_LABEL(LU("trigger_source_label")), "");
    } else if (scope.trige) {
        Signal *trigsig = datasrc->chan(scope.trigch);

        if (trigsig->volts > 0) {
            char minibuf[256];
            SIformat(minibuf, "%g %sV",
                     (scope.trig) * trigsig->volts / 320000, TRUE);
            sprintf(string, "%s Trigger @ %s", trigs[scope.trige], minibuf);
        } else {
            sprintf(string, "%s Trigger @ %d",
                    trigs[scope.trige], scope.trig);
        }
        gtk_label_set_text(GTK_LABEL(LU("trigger_label")), string);
        gtk_label_set_text(GTK_LABEL(LU("trigger_source_label")), trigsig->name);
    } else {
        gtk_label_set_text(GTK_LABEL(LU("trigger_label")), "No Trigger");
        gtk_label_set_text(GTK_LABEL(LU("trigger_source_label")), "");
    }

    gtk_label_set_text(GTK_LABEL(LU("data_source_label")),
                       datasrc ? datasrc->name : "No source");

    gtk_label_set_text(GTK_LABEL(LU("line_style_label")),
                       plot_styles[scope.plot_mode]);
    gtk_label_set_text(GTK_LABEL(LU("scroll_mode_label")),
                       scroll_styles[scope.scroll_mode]);

    if (datasrc) {
        strcpy(string, scope.run ? (scope.run > 1 ? "WAIT" : " RUN") : "STOP");
        gtk_label_set_text(GTK_LABEL(LU("run_stop_label")), string);
    } else {
        gtk_label_set_text(GTK_LABEL(LU("run_stop_label")), "");
    }


    /* sides of graticule */
    for (i = 0 ; i < CHANNELS ; i++) {

        if (ch[i].signal) {
            if (ch[i].signal->rate > 0) {
                if (!ch[i].bits && ch[i].signal->volts)
//                    SIformat(string, "%g %sV/div", 
//                        (double)ch[i].signal->volts / ch[i].scale / 10000, TRUE);

#if SC_16BIT
                    SIformat(string, "%g %sV/div", 
                        (double)(ch[i].signal->volts * 256.0) / (ch[i].scale * 10000), TRUE);
#else
                    SIformat(string, "%g %sV/div", 
                        (double)ch[i].signal->volts / (ch[i].scale * 10000 ), TRUE);
#endif


                else if (ch[i].scale > 1.0)
                    sprintf(string, "%d:1", (int) rint(ch[i].scale));
                else
                    sprintf(string, "1:%d", (int) rint(1.0/ch[i].scale));
            } else if(ch[i].signal->volts){
                /* Special case for a Fourier Transform.  The scaling (Hz/div)
                 * is stored in the volts member of the signal structure.
                 * It is displayed in the "min_max_label" widget.
                 */
                if (ch[i].scale > 1.0)
                    sprintf(string, "%d:1", (int) rint(ch[i].scale));
                else
                    sprintf(string, "1:%d", (int) rint(1.0/ch[i].scale));
            } else {
                strcpy(string, "");
            }

            sprintf(widget, "Ch%1d_scale_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), string);

            sprintf(string, "%d @ %.1g", ch[i].bits, ch[i].pos);
            sprintf(widget, "Ch%1d_position_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), string);

            sprintf(widget, "Ch%1d_source_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), ch[i].signal->name);

            // Not much point in doing this, since the rc file doesn't give us enough control over
            // insensitive rendering.

            // gtk_widget_set_sensitive(LU(widget), ch[i].show);

        } else {

            sprintf(widget, "Ch%1d_scale_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), "");
            sprintf(widget, "Ch%1d_position_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), "");
            sprintf(widget, "Ch%1d_source_label", i+1);
            gtk_label_set_text(GTK_LABEL(LU(widget)), "");

        }

        sprintf(widget, "Ch%1d_frame", i+1);
        if (scope.select == i) {
            gtk_frame_set_shadow_type(GTK_FRAME(LU(widget)), GTK_SHADOW_ETCHED_IN);
        } else {
            gtk_frame_set_shadow_type(GTK_FRAME(LU(widget)), GTK_SHADOW_NONE);
        }

    }

    /* below graticule */
    if (scope.verbose) {

        /* setting help text is special */
        gtk_label_set_text(GTK_LABEL(LU("tab_help_label")), p->show ? "Visible" : "HIDDEN");
        setup_help_text(GTK_WIDGET(LU("tab_help_label")), NULL);
#if 0
        if (scope.select > 1) {
            text_write("($)", 72, 25,
                       0, KEY_FG, TEXT_BG, ALIGN_RIGHT);
            text_write("Extern", 78, 25,
                       0, p->color, TEXT_BG, ALIGN_RIGHT);
            text_write("(:)    (;)", 79, 26,
                       0, KEY_FG, TEXT_BG, ALIGN_RIGHT);
            text_write("Math", 76, 26,
                       0, p->color, TEXT_BG, ALIGN_RIGHT);
        }
#endif

    }

    SIformat(string, "%g %ss/div", scope.scale / 1000.0, TRUE);
    gtk_label_set_text(GTK_LABEL(LU("timebase_label")), string);

    if (p->signal) {

        /* XXX what do we want here - frame samples, samples per screen? */

        /* I cut and changed this line a half dozen times trying to decide what number I wanted
         * displayed as the "Samples" - p->signal->num would give us the actual number of samples in
         * the signal, but that changes during the course of a sweep.  Now I've got the number of
         * samples per sweep, which isn't quite acceptable if there's no data on the screen, or if
         * we're displaying a memory channel with a fixed number of sample
         */

        /* sprintf(string, "%d Samples", p->signal->num); */
        /* sprintf(string, "%d Samples", samples(p->signal->rate)); */
        sprintf(string, "%d Samples/frame", p->signal->width);
        gtk_label_set_text(GTK_LABEL(LU("samples_per_frame_label")), string);

        if (p->signal->rate > 0) {

            SIformat(string, "%g %sS/s", (float)p->signal->rate, TRUE);
            gtk_label_set_text(GTK_LABEL(LU("sample_rate_label")), string);

        } else if (p->signal->rate < 0) {
            /* scaling i.e. Hz/div is now displayed at sides of the graticule.  Here we just display
             * the sample rate of the input to the FFT
             */
            SIformat(string, "%g %sS/s", (float)-p->signal->rate, TRUE);
            gtk_label_set_text(GTK_LABEL(LU("sample_rate_label")), string);

        } else {

            gtk_label_set_text(GTK_LABEL(LU("sample_rate_label")), "");

        }

    } else {

        gtk_label_set_text(GTK_LABEL(LU("samples_per_frame_label")), "");
        gtk_label_set_text(GTK_LABEL(LU("sample_rate_label")), "");

    }

    /* List of available registers */
    for (i = 0 ; i < 26 ; i++) {
        if (datasrc && i < datasrc->nchans()) {
            /* XXX Maybe here we should show color by channel if sig displayed */
            string[i] = i + 'a';
        } else if (mem[i].num > 0) {
            /* XXX different color here for memory? */
            string[i] = i + 'a';
        } else {
            string[i ] = ' ';
        }
    }
    string[i] = '\0';
    gtk_label_set_text(GTK_LABEL(LU("registers_label")), string);

    if ((datasrc != NULL) && (datasrc->option1str != NULL)) {
        gtk_widget_show(LU("data_source_opt1_label"));
        gtk_widget_show(LU("asterisk_key_label"));
    } else {
        gtk_widget_hide(LU("data_source_opt1_label"));
        gtk_widget_hide(LU("asterisk_key_label"));
    }

    if ((datasrc != NULL) && (datasrc->option2str != NULL)) {
        gtk_widget_show(LU("data_source_opt2_label"));
        gtk_widget_show(LU("caret_key_label"));
    } else {
        gtk_widget_hide(LU("data_source_opt2_label"));
        gtk_widget_hide(LU("caret_key_label"));
    }

    if (datasrc && datasrc->nchans() > 0) {
        /* setting help text is special */
        sprintf(string, "(a-%c)", 'a' + datasrc->nchans() - 1);
        gtk_label_set_text(GTK_LABEL(LU("signal_key_label")), string);
        setup_help_text(GTK_WIDGET(LU("signal_key_label")), NULL);

        gtk_widget_show(GTK_WIDGET(LU("signal_key_label")));
        gtk_widget_show(GTK_WIDGET(LU("signal_help_label")));
    } else {
        gtk_widget_hide(GTK_WIDGET(LU("signal_key_label")));
        gtk_widget_hide(GTK_WIDGET(LU("signal_help_label")));
    }

    /* setting help text is special */
    sprintf(string, "(%c-Z)", 'A' + (datasrc ? datasrc->nchans() : 0));
    gtk_label_set_text(GTK_LABEL(LU("store_key_label")), string);
    setup_help_text(GTK_WIDGET(LU("store_key_label")), NULL);

    /* setting help text is special */
    sprintf(string, "(%c-z)", 'a' + (datasrc ? datasrc->nchans() : 0));
    gtk_label_set_text(GTK_LABEL(LU("recall_key_label")), string);
    setup_help_text(GTK_WIDGET(LU("recall_key_label")), NULL);

    if (scope.verbose) {
        make_help_text_visible(glade_window, NULL);
    } else {
        make_help_text_invisible(glade_window, NULL);
    }

    update_dynamic_text();
    fix_widgets();
}


/* The Graticule - we create it as graphs within the databox, then add them as necessary with calls
 * to draw_graticule().  The reason we remove and then re-add them is to make sure they appear
 * either above or below the data, which is controlled by the call ordering to draw_data() and
 * draw_graticule() from show_data().
 */

GtkDataboxGraph *graticule_major_graph = NULL;
GtkDataboxGraph *graticule_minor_graph = NULL;

int total_horizontal_divisions = 10;

int major_graticule_displayed = 0;
int minor_graticule_displayed = 0;

void recompute_graticule(void)
{
    if (graticule_major_graph != NULL) {
        gtk_databox_grid_set_vlines(GTK_DATABOX_GRID(graticule_minor_graph),
                                    total_horizontal_divisions - 1);
        gtk_databox_grid_set_vlines(GTK_DATABOX_GRID(graticule_major_graph),
                                    total_horizontal_divisions/5 - 1);
    }
}

void create_graticule(void)
{
    GtkStyle *style;
    GdkColor gcolor;

#if 0
    static int i, j;
    static int tilt[] = {
        0, -10, 10
    };

    /* a mark where the trigger level is, if the triggered channel is shown */
    if (scope.trige) {
        i = -1;
        for (j = 7 ; j >= 0 ; j--) {
            if (ch[j].show && ch[j].signal == datasrc->chan(scope.trigch))
                i = j;
        }
        if (i > -1) {
            j = offset + ch[i].pos - scope.trig * ch[i].mult / ch[i].div;
            SetColor(ch[i].color);
            DrawLine(90, j + tilt[scope.trige], 110, j - tilt[scope.trige]);
        }
    }
#endif

    /* Use the same color for the graticule that is used for the frame around the databox, which is
     * its BACKGROUND color.
     */

    style = gtk_widget_get_style(GTK_WIDGET(LU("databox_aspectframe")));
    gcolor = style->bg[GTK_STATE_NORMAL];

    /* the minor graticule grid - the scope display is divided into a 10x10 grid with 9x9 lines
     */

    graticule_minor_graph = gtk_databox_grid_new (9, 9, &gcolor, 1);
    graticule_major_graph = gtk_databox_grid_new (1, 1, &gcolor, 1);

#ifdef HAVE_GRID_LINESTYLE
    gtk_databox_grid_set_line_style(GTK_DATABOX_GRID(graticule_major_graph),
                                    GTK_DATABOX_GRID_SOLID_LINES);
    gtk_databox_grid_set_line_style(GTK_DATABOX_GRID(graticule_minor_graph),
                                    GTK_DATABOX_GRID_DOTTED_LINES);
#endif

    recompute_graticule();
}

/* clear_databox() - very similar to
 *    gtk_databox_graph_remove_all(GTK_DATABOX(databox))
 * except that we don't remove quite EVERYTHING (we leave the graticule and the cursors), and we
 * free all the associated data structures
 */

void free_signalline(SignalLine *sl)
{
    while (sl != NULL) {
        SignalLine *slnext = sl->next;

        if (sl->graph != NULL) {
            gtk_databox_graph_remove(GTK_DATABOX(databox), sl->graph);
            g_object_unref(G_OBJECT(sl->graph));
        }
        g_free(sl->X);
        g_free(sl->Y);

        g_free(sl);
        sl = slnext;
    }
}

void clear_databox(void)
{
    int j, bit;

    for (j = 0 ; j < CHANNELS ; j++) {
        Channel *p = &ch[j];
        for (bit = 0; bit < 16 ; bit++) {
            if (p->signalline[bit] != NULL) {
                free_signalline(p->signalline[bit]);
                p->signalline[bit] = NULL;
            }
        }
    }
}

/* configure_databox() - this function takes care of figuring out the various settings needed on the
 * databox to display a particular timebase.  Since the floating point values plotted within the
 * databox are always stored in seconds, selecting a new timebase means setting things on the
 * databox more than anything else.
 *
 * We also figure whether various set-able properties exist that we'll use if they're there, or
 * emultate otherwise.
 */

static gboolean x_offset_property_exists = 0;
static gboolean y_offset_property_exists = 0;
static gboolean y_factor_property_exists = 0;

void configure_databox(void)
{
    GtkDataboxValueRectangle rect;
    gfloat upper_time_limit;
    int j;

    /* The first thing we want to figure out is the maximum time span of any of our displayed
     * signals.  The scope's base rate (scope.scale) is in ms/div; 10 (minor) divs are visible on
     * the screen at once, so the maximum time span (in seconds) is at least...
     */

    upper_time_limit = 10 * 0.001 * scope.scale;

    /* But it might be more, if we have stuff stored... */

    for (j = 0 ; j < CHANNELS ; j++) {
        Channel *p = &ch[j];

        /* XXX for an FFT channel, p->signal->rate will be negative */

        if (p->show && p->signal) {
            if ((p->signal->rate > 0) &&
                (gfloat) p->signal->num / p->signal->rate > upper_time_limit) {
                upper_time_limit = (gfloat) p->signal->num / p->signal->rate;
            }
        }
    }

    /* Now figure how many total divisions wide we'll make the databox.  Since we sample a little
     * past the end of the trace (to fill the entire visible area), we ignore any trailing part of a
     * trace that takes less than half a division to display.  We start with ten divisions, and jump
     * up in increments of five because five minor divisions make a major division and we want to
     * stay on a major division boundary to make our graticule grid nice and neat.
     *
     * XXX If we have an enormous signal (relative to our timebase), this calculation could overflow
     * int total_horizontal_divisions.
     */

    for (total_horizontal_divisions = 10;
         upper_time_limit > (total_horizontal_divisions + 0.5)
             * 0.001 * scope.scale;
         total_horizontal_divisions += 5);

    /* Now set the total canvas size of the databox */

    rect.x1 = 0;
    rect.y1 = 1;

    rect.x2 = total_horizontal_divisions * 0.001 * scope.scale;
    rect.y2 = -1;

    gtk_databox_set_total_limits(GTK_DATABOX(databox), rect.x1, rect.x2, rect.y1, rect.y2);
    /* A slight adjustment gets us our visible area.  Note that this call also resets the databox
     * viewport to its left most position.
     */

    rect.x2 = 10 * 0.001 * scope.scale;
    gtk_databox_set_visible_limits(GTK_DATABOX(databox), rect.x1, rect.x2, rect.y1, rect.y2);

    /* Temporary message is always centered on screen */
    databox_message_X = rect.x2 / 2;

    /* Decide if we need a scrollbar or not */

    if (total_horizontal_divisions > 10) {
        gtk_widget_show(GTK_WIDGET(LU("databox_hscrollbar")));
    } else {
        gtk_widget_hide(GTK_WIDGET(LU("databox_hscrollbar")));
    }

    /* Figure out if we can set offsets on databox lines, or whether we'll have to add offsets to
     * the points when we load them into the array.
     *
     * XXX could be done by 'configure'
     */

    {
        gfloat X, Y;
        GdkColor gcolor;
        GtkDataboxGraph *line = gtk_databox_lines_new(1, &X, &Y, &gcolor, 1);

        x_offset_property_exists = (g_object_class_find_property(G_OBJECT_GET_CLASS(line), "x-offset") != NULL);
        y_offset_property_exists = (g_object_class_find_property(G_OBJECT_GET_CLASS(line), "y-offset") != NULL);
        y_factor_property_exists = (g_object_class_find_property(G_OBJECT_GET_CLASS(line), "y-factor") != NULL);

        g_object_unref(line);
    }

    /* And recompute the graticule grids */

    recompute_graticule();
}

void timebase_changed(void)
{
    /* If the scope is running, then clear the screen traces and reset the capture.  We don't do
     * this if the scope isn't running so that the user can change timebases on a frozen sweep
     * without it disappearing.
     */

    if (datasrc && scope.run) {

        clear_databox();

        /* In xoscope.h, I wrote "Only after reset() has been called are the rate and volts fields in
         * the Signal structures guaranteed valid".  So... we reset() once to make sure the rate and
         * volts fields are valid, then use the rate field in the first active channel to set the
         * capture width to the number of samples required to fill the screen at that rate, then
         * reset() again to (re)start the capture.
         *
         * XXX Probably reset() needs to be split into two functions - say reset() and
         * start_sweep(), so then our sequence is reset(), set_width(), start_sweep()
         *
         * XXX Also seems a little hokey the way we run through the channels.  Implicit here is the
         * code's current design that all the channels for a data source have the same rate and
         * frame width.
         */

        datasrc->reset();
        if (datasrc->set_width) {
            int i;
            for (i=0; i<datasrc->nchans(); i++) {
                if (datasrc->chan(i)->listeners > 0) {
                    datasrc->set_width(samples(datasrc->chan(i)->rate));
                    break;
                }
            }
            datasrc->reset();
        }
        setinputfd(datasrc->fd());
    }

    restart_external_commands();
    configure_databox();
    update_math_signals();
    update_text();
}

/* clear() - one of the most important functions in the program, called whenever something 'changes'
 *
 * Clear the display, clear data history on all display channels, and redraw all text.  Since this
 * clears data history (both on the screen and in memory), it should only be called when needed.
 *
 * XXX These two goals are incompatible - to call this function whenever something changes, and to
 * call it only when needed - and it shows.  We don't want to clear the screen's history unless
 * necessary.  In particular, we want to be able to change time bases with a frozen trace on the
 * screen.
 */

void clear(void)
{
    int i;

    clear_databox();

    if (datasrc) {

        /* In xoscope.h, I wrote "Only after reset() has been called are the rate and volts fields in
         * the Signal structures guaranteed valid".  So... we reset() once to make sure the rate and
         * volts fields are valid, then use the rate field in the first active channel to set the
         * capture width to the number of samples required to fill the screen at that rate, then
         * reset() again to (re)start the capture.
         *
         * XXX Probably reset() needs to be split into two functions - say reset() and
         * start_sweep(), so then our sequence is reset(), set_width(), start_sweep()
         *
         * XXX Also seems a little hokey the way we run through the channels.  Implicit here is the
         * code's current design that all the channels for a data source have the same rate and
         * frame width.
         */

        datasrc->reset();
        if (datasrc->set_width) {
            int i;
            for (i=0; i<datasrc->nchans(); i++) {
                if (datasrc->chan(i)->listeners > 0) {
                    datasrc->set_width(samples(datasrc->chan(i)->rate));
                    break;
                }
            }
            datasrc->reset();
        }
        setinputfd(datasrc->fd());
    }

    configure_databox();

    /* This also updates the 'volts' and 'rate' fields in the math signals */

    math_warning = update_math_signals();

    for (i = 0; i < CHANNELS; i++) {
        ch[i].old_frame = 0;

        /* XXX Might be nice to set 'default scale' to be the largest so that the signal's range
         * still fits within the display window.
         */

        if (ch[i].signal) {
            if (ch[i].signal->volts != 0 && ch[i].signal->rate > 0){
#if SC_16BIT
                ch[i].scale = roundoff(ch[i].scale, 1.0 / (ch[i].signal->volts * 256.0));
#else
                ch[i].scale = roundoff(ch[i].scale, 1.0 / ch[i].signal->volts);
#endif
            }
            else
                ch[i].scale = roundoff(ch[i].scale, 1);
        }
    }

    memset((void *)&stats, 0, sizeof(stats));

    show_data();
    update_text();
}

void draw_graticule(void)
{
    if (graticule_minor_graph == NULL) {
        create_graticule();
    }

    if (major_graticule_displayed) {
        gtk_databox_graph_remove(GTK_DATABOX(databox), graticule_major_graph);
        major_graticule_displayed = 0;
    }

    if (minor_graticule_displayed) {
        gtk_databox_graph_remove(GTK_DATABOX(databox), graticule_minor_graph);
        minor_graticule_displayed = 0;
    }

    if (scope.grat) {
        gtk_databox_graph_add (GTK_DATABOX (databox), graticule_minor_graph);
        minor_graticule_displayed = 1;
    }

    if (scope.grat > 1) {
        gtk_databox_graph_add (GTK_DATABOX (databox), graticule_major_graph);
        major_graticule_displayed = 1;
    }
}

/* draw_data()
 *
 * Writes the signals into the databox.  Called from show_data(), which will queue an expose event
 * for the databox after this function is done.
 */

gfloat cursoraX[2], cursoraY[2], cursorbX[2], cursorbY[2];

GtkDataboxGraph *cursora = NULL;
GtkDataboxGraph *cursorb = NULL;

void draw_data(void)
{
    static int i, j, bit, start, end;
    gfloat num, left_offset;
    Channel *p;
    SignalLine *sl;
    short *samp;
    gchar widget[80];
    GtkStyle *style;
    GdkColor gcolor;
    SignalLine *prevSL;
    double x_offset;

    /* Remove the cursors.  We'll put them back in later if they're active. */

    if (cursora != NULL) {
        gtk_databox_graph_remove(GTK_DATABOX(databox), cursora);
        g_object_unref(G_OBJECT(cursora));
        cursora = NULL;
    }
    if (cursorb != NULL) {
        gtk_databox_graph_remove(GTK_DATABOX(databox), cursorb);
        g_object_unref(G_OBJECT(cursorb));
        cursorb = NULL;
    }

    for (j = 0 ; j < CHANNELS ; j++) { /* plot each visible channel */
        p = &ch[j];
        if(p->signal && p->signal->rate < 0 && in_progress != 0){
            continue;
        }

        if (!p->bits)           /* analog display mode: draw one line */
            start = end = -1;
        else {                  /* logic analyzer mode: draw bits lines */
            start = 0;
            end = p->bits - 1;
        }

        if (p->show && p->signal) {

            /* Figure out color to use for this channel by fetching foreground color of its label */

            sprintf(widget, "Ch%d_label", j+1);
            style = gtk_widget_get_style(GTK_WIDGET(LU(widget)));
            gcolor = style->fg[GTK_STATE_NORMAL];

            samp = p->signal->data;

            /* Compute num, the number of seconds per sample, based on the signal's rate (in
             * samples/sec). If the signal rate is zero (unspecified) or negative (a special case
             * for Fourier Transforms, meaning the x scale is in Hz), we use a base rate of one
             * millisecond per sample.
             */

            if (p->signal->rate > 0) {
                num = (gfloat) 1 / p->signal->rate;
            } 
            else if (p->signal->rate < 0) {
                num = (gfloat) -1 / p->signal->rate;
            } 
            else {
                num = (gfloat) 1 / 1000;
            }

            /* Compute left_offset based on delay specified by the signal (which is in
             * ten-thousandths of samples).
             */

            left_offset = p->signal->delay * num / 10000;

            /* Draw the cursors, if needed.
             *
             * There's several things I don't like about the cursors.  First, the cursor positions
             * are stored in number of samples (1 based), which means that if we change to a
             * different signal with a different sampling rate, the cursors move around on the
             * screen!  Also, we should be able to pick whether they "snap to data points" or not.
             */

            if (scope.curs && j == scope.select) {
                cursoraX[0] = cursoraX[1] = left_offset + (scope.cursa-1) * num;
                cursorbX[0] = cursorbX[1] = left_offset + (scope.cursb-1) * num;
                cursoraY[0] = cursorbY[0] = -1;
                cursoraY[1] = cursorbY[1] = +1;

                cursora = gtk_databox_lines_new(2, cursoraX, cursoraY, &gcolor, 1);
                cursorb = gtk_databox_lines_new(2, cursorbX, cursorbY, &gcolor, 1);
                gtk_databox_graph_add(GTK_DATABOX(databox), cursora);
                gtk_databox_graph_add(GTK_DATABOX(databox), cursorb);
            }

            /* XXX make sure that if we're displaying a digital signal, we go into digital display
             * mode.  Should be elsewhere.
             */
#if 0
            if (p->bits == 0 && p->signal->bits != 0) {
                p->bits = p->signal->bits;
            }
#endif

            for (bit = start ; bit <= end ; bit++) {

                /* SignalLine structures contain all the stored information about the (x,y)
                 * coordinates we've drawn already and may need to erase
                 */
                sl = p->signalline[bit < 0 ? 0 : bit];

                if ((sl == NULL) ||
                    (p->signal->frame != p->old_frame) || (p->old_frame == 0)) {
                    /* New signal line, so we need a new SignalLine structure */

                    sl = g_new0(SignalLine, 1);

                    sl->next = p->signalline[bit < 0 ? 0 : bit];
                    p->signalline[bit < 0 ? 0 : bit] = sl;

                    /* we double the size of these array in case we're in step mode, when we draw
                     * two vertices for every data point
                     */

                    sl->X = g_new0(gfloat, 2 * p->signal->width);
                    sl->Y = g_new0(gfloat, 2 * p->signal->width);
                    sl->data = g_new0(short, p->signal->width);

                    sl->y_scale = 1.0;
                }


                /* If we're continuing a running sweep, remove the existing trace from the databox.
                 * We'll put it back in later, with more data points.
                 */

                if (sl->graph != NULL) {
                    gtk_databox_graph_remove(GTK_DATABOX(databox), sl->graph);
                    g_object_unref(G_OBJECT(sl->graph));
                    sl->graph = NULL;
                }

                /* Compute the points we want to draw on the current trace and write them into the
                 * SignalLine arrays.  The only thing a little bit strange is that we might be
                 * updating a trace that's already partially drawn; that's why we start at
                 * sl->next_point and not 0.
                 */
                for (i = sl->next_point; i < p->signal->num; i++) {

                    if (bit < 0) {
                        sl->data[sl->next_point] = samp[i];
                    } else {
                        sl->data[sl->next_point] = (samp[i] >> bit) & 1;
                    }

                    sl->X[sl->next_point] = left_offset + i * num;
                    sl->Y[sl->next_point] = sl->data[sl->next_point];
                    sl->next_point ++;
                }

                /* Depending on the scroll mode, manage previous traces */

                switch (scope.scroll_mode) {

                case 0:

                    /* Sweep mode - erase anything lingering in the databox except the next to last
                     * trace, because we want to leave the trailing part of it drawn if we're in the
                     * middle of a sweep.  We remove it from the databox, and put it back in with
                     * fewer data points.
                     */

                    if (sl->next != NULL && sl->next->graph != NULL) {
                        gtk_databox_graph_remove(GTK_DATABOX(databox), sl->next->graph);
                        g_object_unref(G_OBJECT(sl->next->graph));
                        sl->next->graph = NULL;
                    }

                    if (sl->next != NULL && sl->next->next != NULL) {
                        free_signalline(sl->next->next);
                        sl->next->next = NULL;
                    }

                    /* XXX I'd like the old trace to start at the same x-coordinate that the new
                     * trace ends at, but that creates a special case if the "new" trace is
                     * zero-length.  Just shows how badly this code needs a cleanup.
                     */

#if 0
                    if ((sl->next != NULL)
                        && (sl->next_point < sl->next->next_point)) {
                        sl->next->graph
                            = gtk_databox_lines_new (sl->next->next_point-sl->next_point+1,
                                                     sl->next->X + sl->next_point - 1,
                                                     sl->next->Y + sl->next_point - 1,
                                                     &gcolor, 1);
                        gtk_databox_graph_add (GTK_DATABOX (databox), sl->next->graph);
                    }
#else
                    if ((sl->next != NULL)
                        && (sl->next_point < sl->next->next_point)) {
                        switch (scope.plot_mode) {
                        case 0: /* points */
                            sl->next->graph
                                = gtk_databox_points_new (sl->next->next_point-sl->next_point,
                                                          sl->next->X + sl->next_point,
                                                          sl->next->Y + sl->next_point,
                                                          &gcolor, 1);
                            break;
                        case 1: /* lines */
                            sl->next->graph
                                = gtk_databox_lines_new (sl->next->next_point-sl->next_point,
                                                         sl->next->X + sl->next_point,
                                                         sl->next->Y + sl->next_point,
                                                         &gcolor, 1);
                            break;
                        case 2: /* step */
                            sl->next->graph
                                = gtk_databox_lines_new (2*(sl->next->next_point - sl->next_point) - 1,
                                                         sl->next->X + 2 * sl->next_point,
                                                         sl->next->Y + 2 * sl->next_point,
                                                         &gcolor, 1);
                            break;
                        }
                        gtk_databox_graph_add (GTK_DATABOX (databox), sl->next->graph);
                    }
#endif

                    break;

                case 1:

                    /* Accumulate mode - do nothing, letting traces pile up in the databox.
                     *
                     * XXX this can lead to memory and CPU exhaustion with thousands of traces
                     * piling up on a fast timebase
                     */

                    break;

                case 2:

                    /* Stripchart mode - position this trace at the right of the databox and line up
                     * any previous traces to its left
                     */

                    x_offset = total_horizontal_divisions * 0.001 * scope.scale
                        - num * (sl->next_point - 1);
                    for (prevSL = sl; prevSL != NULL; prevSL = prevSL->next) {

                        /* If x_offset is negative at this point, we've just drawn a SignalLine
                         * partially off the left-hand side of the screen, so anything older has
                         * scrolled completely out of view.
                         */

                        prevSL->x_offset = x_offset;
// prevSL->y_offset = 0.25;
                        if ((x_offset < 0) && prevSL->next) {
                            free_signalline(prevSL->next);
                            prevSL->next = NULL;
                        }

                        x_offset -= num * p->signal->width;
                    }
                }

                /* The scale is applied first, then the offset 
                 * Full range is scaled to 4/5 of the screen.
                 * Therefor we scale it to 127*1,25 in 8-bit mode 
                 * and 32767*1,25 in 16-bit mode.
                 */
#if SC_16BIT
                sl->y_scale = (double)p->scale / 40959;
                sl->y_offset = (double)p->pos;
#else
                sl->y_scale = (double)p->scale / 160;
                sl->y_offset = (double)p->pos;
#endif
                /* If we're in digital mode, increase the scale by eight and shift the offset by
                 * sixteen for each bit.  This hardwires eight as the height of a digital line and
                 * sixteen as the inter-line spacing.  We also shift the entire digital plot by the
                 * number of bits times eight plus four to center it.
                 */

                if (bit >= 0) {
                    int bitoff = bit * 16 - end * 8 + 4;

#if SC_16BIT
                    sl->y_offset += bitoff * sl->y_scale * 256;
                    sl->y_scale *= (8 * 256);
#else
                    sl->y_offset += bitoff * sl->y_scale;
                    sl->y_scale *= 8;
#endif
                }
                /* Add the current trace to the databox */

                if (sl->next_point > 0) {

                    switch (scope.plot_mode) {
                    case 0: /* points */
                        sl->graph = gtk_databox_points_new (sl->next_point,
                                                            sl->X, sl->Y, &gcolor, 1);
                        break;
                    case 1: /* lines */
                        sl->graph = gtk_databox_lines_new (sl->next_point,
                                                           sl->X, sl->Y, &gcolor, 1);
                        break;
                    case 2: /* step */
                        sl->graph = gtk_databox_lines_new (2 * sl->next_point - 1,
                                                           sl->X, sl->Y, &gcolor, 1);
                        break;
                    }

                    gtk_databox_graph_add (GTK_DATABOX (databox), sl->graph);

                }

                /* Run through all of the SignalLines associated with this trace and set the scaling
                 * factors and offsets for all of them.  This ensures that if we're in accumulate
                 * mode and change the scale or position of the channel, all of the accumulated
                 * traces move together.  Not quite what you'd expect from a real scope, but I think
                 * this makes the most sense.
                 *
                 * XXX save left_offset in the SignalLine structure rather than use the one for the
                 * current signal
                 */

                for (prevSL = sl; prevSL != NULL; prevSL = prevSL->next) {
                    if (prevSL->graph != NULL) {
                        if (x_offset_property_exists && y_offset_property_exists && y_factor_property_exists) {

                            GValue gvalue;

                            bzero(&gvalue, sizeof(GValue));
                            g_value_init(&gvalue, G_TYPE_DOUBLE);

                            g_value_set_double(&gvalue, prevSL->x_offset);
                            g_object_set_property((GObject *) prevSL->graph, "x-offset", &gvalue);

                            g_value_set_double(&gvalue, prevSL->y_scale);
                            g_object_set_property((GObject *) prevSL->graph, "y-factor", &gvalue);

                            g_value_set_double(&gvalue, prevSL->y_offset);
                            g_object_set_property((GObject *) prevSL->graph, "y-offset", &gvalue);

                            g_value_unset(&gvalue);
                            //g_object_set_property((GObject *) prevSL->graph, "plot-style", &plotstyle);
                        } else {
                            for (i = 0; i < prevSL->next_point; i++) {
                                if ((scope.plot_mode != 2) || (i == 0)) {
                                    prevSL->X[i] = prevSL->x_offset + left_offset + i * num;
                                    prevSL->Y[i] = prevSL->y_offset + prevSL->data[i] * prevSL->y_scale;
                                } else {
                                    prevSL->X[2*i] = prevSL->x_offset + left_offset + i * num;
                                    prevSL->Y[2*i] = prevSL->y_offset + prevSL->data[i] * prevSL->y_scale;
                                    prevSL->X[2*i - 1] = prevSL->X[2*i - 2];
                                    prevSL->Y[2*i - 1] = prevSL->Y[2*i];
                                }
                            }
                        }
                    }
                }

            }

            p->old_frame = p->signal->frame;

#if 0
            /* Draw tick marks on left and right sides of display showing zero pos */
            SetColor(p->color);
            DrawLine(90, off, 100, off);
            DrawLine(h_points - 100, off, h_points - 90, off);
#endif
        }

        /* If we're not showing a channel, make sure that we've removed any traces that might still
         * be lingering around on the screen.
         */

        if (! p->show) {
            for (bit = start ; bit <= end ; bit++) {
                free_signalline(p->signalline[bit < 0 ? 0 : bit]);
                p->signalline[bit < 0 ? 0 : bit] = NULL;
            }
        }

        if (!p->bits) end=0;
        for (bit = end+1; bit < 16; bit++) {
            free_signalline(p->signalline[bit < 0 ? 0 : bit]);
            p->signalline[bit < 0 ? 0 : bit] = NULL;
        }
    }

}

/* calculate any math and plot the results and the graticule */

void show_data(void)
{
    /* Run any math functions, then measure statistics to be displayed, like min, max, frequency.
     * If the timebase is fast enough (less than 100 ms/div) do this only at the end of a frame.
     */

    do_math();

    if ((scope.scale >= 100) || !in_progress)
        measure_data(&ch[scope.select], &stats);

    update_dynamic_text();

    if (scope.behind) {
        draw_graticule();               /* plot data on top of graticule */
        draw_data();
    } else {
        draw_data();            /* plot graticule on top of data */
        draw_graticule();
    }

    gtk_widget_queue_draw (databox);
}

/* animate() - get and plot some data */

void animate(void *data)
{
    static struct timeval current_time, prev_time;

    /* To avoid hammering the X server, don't do anything if it's been less than scope.min_interval
     * milliseconds (default 50) since the last time we ran this function.  If we do skip
     * processing, then set a timeout to make sure we run again scope.min_interval milliseconds from
     * now.
     * An intervall longer than SND_QUERY_INTERVAL (10 ms) is applied only AFTER 
     * a sweep has completed.
     */

    gettimeofday(&current_time, NULL);

    if ((!in_progress && scope.min_interval > SND_QUERY_INTERVALL)
        && (prev_time.tv_sec <= current_time.tv_sec)
        && (prev_time.tv_sec + 10 > current_time.tv_sec)
        && (1000000 * (current_time.tv_sec - prev_time.tv_sec)
            + current_time.tv_usec - prev_time.tv_usec
            < 1000 * scope.min_interval)) {
        settimeout(scope.min_interval);
        setinputfd(-1);
        return;
    }

    prev_time = current_time;
    if (datasrc) setinputfd(datasrc->fd());
    settimeout(SND_QUERY_INTERVALL);

    clip = 0;
    if (datasrc) {
        if (scope.run) {
            triggered = datasrc->get_data();
            if (triggered && scope.run > 1) { /* auto-stop single-shot wait */
                scope.run = 0;
                update_text();
            }
        } else if (in_progress && (scope.scroll_mode != 2)) {
            /* If we're in strip chart mode (scroll mode 2), stop immediately, otherwise wait for
             * the running trace to complete.
             */
            datasrc->get_data();
        } else {
            //usleep(100000);           /* no need to suck all CPU cycles */
            setinputfd(-1);             /* scope not running, so why listen? */
        }
    }
    show_data();
}
