/*
 * @(#)$Id: com_gtk.h,v 2.3 2009/01/17 06:44:55 baccala Exp $
 *
 * Copyright (C) 1996 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * Some GTK widgets and event handlers common to xoscope and xy.
 *
 */

GtkWidget *menubar;
GtkWidget *glade_window;

gint expose_event();
gint key_press_event();
void delete_event();
void hit_key();

GtkWidget* lookup_widget(GtkWidget *widget, const gchar *widget_name);
#define LU(label)       lookup_widget(glade_window, label)
