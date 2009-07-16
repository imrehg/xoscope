/*
 * @(#)$Id: com_gtk.c,v 2.4 2009/01/07 02:19:03 baccala Exp $
 *
 * Copyright (C) 1996 - 2000 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file holds GTK code that is common to xoscope and xy.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "oscope.h"
#include "display.h"
#include "com_gtk.h"

GdkPixmap *pixmap = NULL;
int fixing_widgets = 0;

/* emulate several libsx function in GTK */
int
OpenDisplay(int argc, char *argv[])
{
  gtk_init(&argc, &argv);
  return(argc);
}

/* set up some common event handlers */

void
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  gtk_main_quit ();
}

gint
key_press_event(GtkWidget *widget, GdkEventKey *event)
{
  if (event->keyval == GDK_Tab) {
    handle_key('\t');
  } else if (event->length == 1) {
    handle_key(event->string[0]);
  }

  return TRUE;
}

/* simple event callback that emulates the user hitting the given key */
void
hit_key(GtkWidget *w, guint data)
{
  if (fixing_widgets) return;
  handle_key(data);
}
