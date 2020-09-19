/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the ESD GUI.  It's just setting the esdrecord option on or off.  The old
 * value is found via a save_option() call (esdrecord is assumed to be option #1).  We keep a
 * temp_rec variable in case the user closes or cancels the dialog without clicking 'Accept'.  On
 * 'Accept', we set the variable by forming an ASCII option string and doing a set_option().
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "xoscope.h"
#include "display.h"

static int temp_rec;

static void esd_save_values(GtkWidget *w, gpointer data)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "esdrecord=%d", temp_rec);
    datasrc->set_option(buf);
    clear();
}

static void esdrecord(GtkWidget *w, gpointer data)
{
    temp_rec = * (int *) data;
}

void esd_gtk_option_dialog()
{
    GtkWidget *window, *label, *accept, *cancel;
    GtkWidget *on, *off;
    char *option;

    window = gtk_dialog_new();
    accept = gtk_button_new_with_label(" Accept ");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), accept,
                       TRUE, TRUE, 0);
    cancel = gtk_button_new_with_label("  Cancel  ");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->action_area), cancel,
                       TRUE, TRUE, 0);

    label = gtk_label_new("  ESD Record Mode:  ");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), label,
                       TRUE, TRUE, 5);

    on = gtk_radio_button_new_with_label(NULL, "On");
    off = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(on)), "Off");

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), on,
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), off,
                       TRUE, TRUE, 0);

    option = datasrc->save_option(1);   /* format will be "esdrecord=%d" */
    if (option) temp_rec = option[10] - '0';

    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(on), temp_rec == 1);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(off), temp_rec == 0);

    gtk_signal_connect(GTK_OBJECT(on), "clicked",
                       GTK_SIGNAL_FUNC(esdrecord), int_to_int_pointer(1));
    gtk_signal_connect(GTK_OBJECT(off), "clicked",
                       GTK_SIGNAL_FUNC(esdrecord), int_to_int_pointer(0));

    gtk_signal_connect_object(GTK_OBJECT(window), "delete_event",
                              GTK_SIGNAL_FUNC(gtk_widget_destroy),
                              GTK_OBJECT(window));
    gtk_signal_connect(GTK_OBJECT(accept), "clicked",
                       GTK_SIGNAL_FUNC(esd_save_values), NULL);
    gtk_signal_connect_object_after(GTK_OBJECT(accept), "clicked",
                                    GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                    GTK_OBJECT(window));
    gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
                              GTK_SIGNAL_FUNC(gtk_widget_destroy),
                              GTK_OBJECT(window));

    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    GTK_WIDGET_SET_FLAGS(accept, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(accept);

    gtk_widget_show(on);
    gtk_widget_show(off);
    gtk_widget_show(accept);
    gtk_widget_show(cancel);
    gtk_widget_show(label);
    gtk_widget_show(window);
}
