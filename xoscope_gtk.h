/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * @(#)$Id: com_gtk.h,v 2.3 2009/01/17 06:44:55 baccala Exp $
 *
 * Copyright (C) 1996 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * Some GTK widgets and event handlers common to xoscope and xy.
 *
 */

extern GtkWidget *menubar;
extern GtkWidget *glade_window;
extern GtkWidget *bitscope_options_dialog;
extern GtkWidget *comedi_options_dialog;
extern GtkWidget *alsa_options_dialog;
extern GtkWidget *databox;

GtkWidget * lookup_widget(const gchar *widget_name);
#define LU(label)       lookup_widget(label)

GtkWidget *create_comedi_dialog (void);

void on_main_window_destroy (GtkObject *object, gpointer user_data);
