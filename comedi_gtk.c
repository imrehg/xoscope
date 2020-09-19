/* -*- mode: C++; indent-tabs-mode: nil; fill-column: 100; c-basic-offset: 4; -*-
 *
 * Author: Brent Baccala <baccala@freesoft.org>
 *
 * Public domain.
 *
 * This file implements the COMEDI GUI component.
 *
 */

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "xoscope.h"
#include "display.h"
#include "xoscope_gtk.h"

GtkWidget *window;

#ifdef HAVE_LIBCOMEDI

#include <comedilib.h>

/* Various COMEDI driver variables we import into our namespace.  We set variables via the
 * datasrc->set_option interface, but we read the variables directly, primarily because we need to
 * get some information from comedi (like the number of subdevices, their types and flags) that we
 * can't get from the datasrc interface, so we just go and read everything directly.
 */

extern comedi_t *comedi_dev;
extern char *comedi_devname;
extern int comedi_subdevice;
extern int comedi_bufsize;
extern int comedi_rate;
extern int comedi_aref;

static int gui_subdevice = 0;

char *subdevice_types[]={
    "unused",
    "analog input",
    "analog output",
    "digital input",
    "digital output",
    "digital I/O",
    "counter",
    "timer",
    "memory",
    "calibration",
    "processor"
};

void subdevice_on_activate(GtkWidget *widget, gpointer data)
{
    int subdevice = * (int *) data;
    int subdev_flags = comedi_get_subdevice_flags(comedi_dev, subdevice);
    char buf[64];

    gui_subdevice = subdevice;

    /* Check with the current subdevice's flags to grey out any arefs it doesn't support. */

    gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_ground")), subdev_flags & SDF_GROUND);
    gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_diff")), subdev_flags & SDF_DIFF);
    gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_common")), subdev_flags & SDF_COMMON);

    /* If the buffer size radio button is set to 'default', update the field with the default buffer
     * size (which might be different for this subdevice).
     */

    if (GTK_TOGGLE_BUTTON(LU("bufsize_default"))->active) {
        sprintf(buf, "%d", comedi_get_buffer_size(comedi_dev, comedi_subdevice));
        gtk_entry_set_text(GTK_ENTRY(LU("bufsize_entry")), buf);
    }
}

/* This function isn't registered anywhere as a callback, but we use it both when the dialog is
 * initially created and after the COMEDI device changes.
 */

static GtkWidget *menu = NULL;

void device_changed(void)
{
    char buf[64];
    int i;

    gtk_entry_set_text(GTK_ENTRY(LU("comedi_device_entry")), comedi_devname);

    /* If we couldn't open the current COMEDI device (comedi_dev is NULL), we want everything except
     * the device name field (and its label) to be greyed out.  I don't know if GTK's designers
     * planned for gtk_widget_set_sensitive() to be used as a callback, but it works!
     */

    if (comedi_dev == NULL) {
        gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
                              (GtkCallback) gtk_widget_set_sensitive,
                              (gpointer) FALSE);
        gtk_widget_set_sensitive(LU("device_label"), TRUE);
        gtk_widget_set_sensitive(LU("comedi_device_entry"), TRUE);
        /* XXX maybe put something here - add "none" to subdevice list */
        return;
    } else {
        gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
                              (GtkCallback) gtk_widget_set_sensitive,
                              (gpointer) TRUE);
    }

    snprintf(buf, sizeof(buf), "%d", comedi_rate);
    gtk_entry_set_text(GTK_ENTRY(LU("rate_entry")), buf);

    /* Set the bufsize radio buttons based on whether we're using the default buffer size or a
     * custom one.  If we're custom, put the custom size we're using in the entry field and make it
     * sensitive.  If we're using the default, find the default and put it in the entry field, then
     * grey it out.
     */

    if (comedi_bufsize > 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LU("bufsize_custom")), TRUE);
        sprintf(buf, "%d", comedi_bufsize);
        gtk_entry_set_text(GTK_ENTRY(LU("bufsize_entry")), buf);
        gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LU("bufsize_default")), TRUE);
        sprintf(buf, "%d", comedi_get_buffer_size(comedi_dev, comedi_subdevice));
        gtk_entry_set_text(GTK_ENTRY(LU("bufsize_entry")), buf);
        gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), FALSE);
    }

    /* Build the subdevice option menu by running through COMEDI's list of subdevices.  Grey out any
     * subdevices that doesn't support input.  Attach the menu to the subdevice choice button and
     * set_history to select the current subdevice.
     */

    if (menu) gtk_widget_destroy(menu);
    menu = gtk_menu_new();
    for (i=0; i<comedi_get_n_subdevices(comedi_dev); i++) {
        GtkWidget *p;
        int subdev_type = comedi_get_subdevice_type(comedi_dev, i);
        int nchans = comedi_get_n_channels(comedi_dev, i);

        snprintf(buf, sizeof(buf), "%d (%s; %d chan%s)",
                 i, subdevice_types[subdev_type], nchans, nchans > 1 ? "s" : "");
        p = gtk_menu_item_new_with_label (buf);
        gtk_signal_connect (GTK_OBJECT(p), "activate",
                            GTK_SIGNAL_FUNC(subdevice_on_activate),
                            int_to_int_pointer(i));
        gtk_menu_append (GTK_MENU(menu), p);

        /* If subdevice doesn't support input, gray it out */
        gtk_widget_set_sensitive(p, (subdev_type==1) || (subdev_type==3) || (subdev_type==5));
        gtk_widget_show (p);
    }
    gtk_option_menu_set_menu(GTK_OPTION_MENU(LU("subdevice_optionmenu")), menu);
    gtk_option_menu_set_history(GTK_OPTION_MENU(LU("subdevice_optionmenu")), comedi_subdevice);
    subdevice_on_activate(menu, int_to_int_pointer(comedi_subdevice));

    /* Set the aref radio buttons based on current aref setting. */

    switch (comedi_aref) {
    case AREF_GROUND:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LU("aref_ground")), TRUE);
        break;
    case AREF_DIFF:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LU("aref_diff")), TRUE);
        break;
    case AREF_COMMON:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(LU("aref_common")), TRUE);
        break;
    }

}

void comedi_gtk_options(void)
{
    GtkWidget *comedi_dialog;

    comedi_dialog = create_comedi_dialog();
    window = comedi_dialog;

    device_changed();

    gtk_widget_show(comedi_dialog);
}

void comedi_save_options(void)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "device=%s",
             gtk_entry_get_text(GTK_ENTRY(LU("comedi_device_entry"))));
    datasrc->set_option(buf);

    snprintf(buf, sizeof(buf), "rate=%s",
             gtk_entry_get_text(GTK_ENTRY(LU("rate_entry"))));
    datasrc->set_option(buf);

    snprintf(buf, sizeof(buf), "subdevice=%d", gui_subdevice);
    datasrc->set_option(buf);

    if (GTK_TOGGLE_BUTTON(LU("bufsize_default"))->active) {
        datasrc->set_option("bufsize=default");
    } else {
        snprintf(buf, sizeof(buf), "bufsize=%s",
                 gtk_entry_get_text(GTK_ENTRY(LU("bufsize_entry"))));
        datasrc->set_option(buf);
    }

    if (GTK_TOGGLE_BUTTON(LU("aref_ground"))->active) {
        datasrc->set_option("aref=ground");
    } else if (GTK_TOGGLE_BUTTON(LU("aref_diff"))->active) {
        datasrc->set_option("aref=diff");
    } else if (GTK_TOGGLE_BUTTON(LU("aref_common"))->active) {
        datasrc->set_option("aref=common");
    }

    /* Datasrc API insists we have to do datasrc->reset() now, we also may need to update various
     * stuff like the input fd, so we just clear()
     */

    clear();
}

#if 0

void
on_device_entry_changed (GtkEditable *editable, gpointer user_data)
{
    if (strcmp(gtk_entry_get_text(GTK_ENTRY(LU("comedi_device_entry"))),
               comedi_devname) == 0) {
        gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
                              (GtkCallback) gtk_widget_set_sensitive,
                              (gpointer) TRUE);
    } else {
        gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
                              (GtkCallback) gtk_widget_set_sensitive,
                              (gpointer) FALSE);
        gtk_widget_set_sensitive(LU("device_label"), TRUE);
        gtk_widget_set_sensitive(LU("comedi_device_entry"), TRUE);
    }
}

#endif

#endif

void
comedi_on_ok(GtkButton *button, gpointer user_data)
{
#ifdef HAVE_LIBCOMEDI
    comedi_save_options();
    gtk_widget_hide(window);
    menu = NULL;
#endif
}


void
comedi_on_apply(GtkButton *button, gpointer user_data)
{
#ifdef HAVE_LIBCOMEDI
    comedi_save_options();
    device_changed();
#endif
}


void
on_bufsize_custom_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), TRUE);
}


void
on_bufsize_default_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), FALSE);
}
