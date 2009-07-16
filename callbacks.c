#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "oscope.h"
#include "display.h"

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#define LU(label)	lookup_widget(GTK_WIDGET(window), label)

#ifdef HAVE_LIBCOMEDI

#include <comedilib.h>

/* Various COMEDI driver variables we import into our namespace.  We
 * set variables via the datasrc->set_option interface, but we read
 * the variables directly, primarily because we need to get some
 * information from comedi (like the number of subdevices, their types
 * and flags) that we can't get from the datasrc interface, so we just
 * go and read everything directly.
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
  int subdevice = (int) data;
  int subdev_flags = comedi_get_subdevice_flags(comedi_dev, subdevice);
  char buf[64];

  gui_subdevice = subdevice;

  /* Check with the current subdevice's flags to grey out any arefs it
   * doesn't support.
   */

  gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_ground")), subdev_flags & SDF_GROUND);
  gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_diff")), subdev_flags & SDF_DIFF);
  gtk_widget_set_sensitive(GTK_WIDGET(LU("aref_common")), subdev_flags & SDF_COMMON);

  /* If the buffer size radio button is set to 'default', update the
   * field with the default buffer size (which might be different for
   * this subdevice).
   */

  if (GTK_TOGGLE_BUTTON(LU("bufsize_default"))->active) {
    sprintf(buf, "%d", comedi_get_buffer_size(comedi_dev, comedi_subdevice));
    gtk_entry_set_text(GTK_ENTRY(LU("bufsize_entry")), buf);
  }
}

/* This function isn't registered anywhere as a callback, but we
 * use it both when the dialog is initially created and after
 * the COMEDI device changes.
 */

static GtkWidget *menu = NULL;

void device_changed(void)
{
  char buf[64];
  int i;

  gtk_entry_set_text(GTK_ENTRY(LU("device_entry")), comedi_devname);

  /* If we couldn't open the current COMEDI device (comedi_dev is
   * NULL), we want everything except the device name field (and its
   * label) to be greyed out.  I don't know if GTK's designers planned
   * for gtk_widget_set_sensitive() to be used as a callback, but it
   * works!
   */

  if (comedi_dev == NULL) {
    gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
			  (GtkCallback) gtk_widget_set_sensitive,
			  (gpointer) FALSE);
    gtk_widget_set_sensitive(LU("device_label"), TRUE);
    gtk_widget_set_sensitive(LU("device_entry"), TRUE);
    /* XXX maybe put something here - add "none" to subdevice list */
    return;
  } else {
    gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
			  (GtkCallback) gtk_widget_set_sensitive,
			  (gpointer) TRUE);
  }

  snprintf(buf, sizeof(buf), "%d", comedi_rate);
  gtk_entry_set_text(GTK_ENTRY(LU("rate_entry")), buf);

  /* Set the bufsize radio buttons based on whether we're using the
   * default buffer size or a custom one.  If we're custom, put the
   * custom size we're using in the entry field and make it sensitive.
   * If we're using the default, find the default and put it in the
   * entry field, then grey it out.
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

  /* Build the subdevice option menu by running through COMEDI's list
   * of subdevices.  Grey out any subdevices that doesn't support
   * input.  Attach the menu to the subdevice choice button and
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
			(gpointer) i);
    gtk_menu_append (GTK_MENU(menu), p);

    /* If subdevice doesn't support input, gray it out */
    gtk_widget_set_sensitive(p, (subdev_type==1) || (subdev_type==3) || (subdev_type==5));
    gtk_widget_show (p);
  }
  gtk_option_menu_set_menu(GTK_OPTION_MENU(LU("subdevice_optionmenu")), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(LU("subdevice_optionmenu")), comedi_subdevice);
  subdevice_on_activate(menu, (gpointer) comedi_subdevice);

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
	   gtk_entry_get_text(GTK_ENTRY(LU("device_entry"))));
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

  /* Datasrc API insists we have to do datasrc->reset() now,
   * we also may need to update various stuff like the
   * input fd, so we just clear()
   */

  clear();
}

#if 0

void
on_device_entry_changed                (GtkEditable     *editable,
                                        gpointer         user_data)
{
  if (strcmp(gtk_entry_get_text(GTK_ENTRY(LU("device_entry"))),
	     comedi_devname) == 0) {
    gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
			  (GtkCallback) gtk_widget_set_sensitive,
			  (gpointer) TRUE);
  } else {
    gtk_container_foreach(GTK_CONTAINER(LU("option_table")),
			  (GtkCallback) gtk_widget_set_sensitive,
			  (gpointer) FALSE);
    gtk_widget_set_sensitive(LU("device_label"), TRUE);
    gtk_widget_set_sensitive(LU("device_entry"), TRUE);
  }
}

#endif

#endif

void
comedi_on_ok                           (GtkButton       *button,
                                        gpointer         user_data)
{
#ifdef HAVE_LIBCOMEDI
  comedi_save_options();
  gtk_widget_destroy(window);
  menu = NULL;
#endif
}


void
comedi_on_apply                        (GtkButton       *button,
                                        gpointer         user_data)
{
#ifdef HAVE_LIBCOMEDI
  comedi_save_options();
  device_changed();
#endif
}


void
on_bufsize_custom_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), TRUE);
}


void
on_bufsize_default_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(LU("bufsize_entry")), FALSE);
}

/* This part implements the Bitscope dialog window. */

GtkWidget *window;

gdouble dialog_volts;

void
bitscope_dialog()
{
  GtkWidget *dialog2;
  GtkWidget *hscale;

  add_pixmap_directory (PACKAGE_DATA_DIR "/pixmaps");
  add_pixmap_directory (PACKAGE_SOURCE_DIR "/pixmaps");

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  dialog2 = create_dialog2 ();
  gtk_widget_show (dialog2);
  window = dialog2;

  /* The object of this code is to update the binary "trigger condition"
   * as the analog sliders are moved.
   */
  hscale = lookup_widget(dialog2, "hscale1");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (hscale)->adjustment),
                      "value_changed", GTK_SIGNAL_FUNC (on_value_changed),
                      "0");
  hscale = lookup_widget(dialog2, "hscale2");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (hscale)->adjustment),
                      "value_changed", GTK_SIGNAL_FUNC (on_value_changed),
                      "1");

  /* XXX here we need to initially set all the widgets from bs.r */

  /* force pages to update each other to current values */
  gtk_notebook_set_page(GTK_NOTEBOOK(lookup_widget(dialog2, "notebook1")), 1);
  gtk_notebook_set_page(GTK_NOTEBOOK(lookup_widget(dialog2, "notebook1")), 0);

}

void
propagate_changes()
{

  int x, y;
  /* char 261: +/-;  GTK+ 2 uses UTF-8, so it becomes \302\261  */
  gchar format[] = "\302\261%.4g V", buffer[128];
  gdouble volts[] = {
    0.130, 0.600,  1.200,  3.160,
    1.300, 6.000, 12.000, 31.600,
    0.632, 2.900,  5.800, 15.280,
  };

  if (GTK_TOGGLE_BUTTON(LU("radiobutton24"))->active)
    x = y = 8;
  else {
    x = GTK_TOGGLE_BUTTON(LU("checkbutton2"))->active ? 4 : 0;
    y = GTK_TOGGLE_BUTTON(LU("checkbutton3"))->active ? 4 : 0;
  }

  sprintf(buffer, format, volts[x]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton20")), buffer);

  sprintf(buffer, format, volts[x + 1]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton21")), buffer);

  sprintf(buffer, format, volts[x + 2]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton22")), buffer);

  sprintf(buffer, format, volts[x + 3]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton23")), buffer);

  sprintf(buffer, format, volts[y]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton14")), buffer);

  sprintf(buffer, format, volts[y + 1]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton15")), buffer);

  sprintf(buffer, format, volts[y + 2]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton16")), buffer);

  sprintf(buffer, format, volts[y + 3]);
  gtk_button_set_label(GTK_BUTTON(LU("radiobutton17")), buffer);

  if (GTK_TOGGLE_BUTTON(LU("radiobutton21"))->active) x += 1;
  if (GTK_TOGGLE_BUTTON(LU("radiobutton22"))->active) x += 2;
  if (GTK_TOGGLE_BUTTON(LU("radiobutton23"))->active) x += 3;

  dialog_volts = volts[x];

  gtk_signal_emit_by_name(GTK_OBJECT(GTK_RANGE(LU("hscale1"))->adjustment),
			  "value_changed");
/*    gtk_signal_emit_by_name(GTK_OBJECT(LU("entry1")), "changed"); */

}


void
on_toggled                             (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  propagate_changes();
}


void
on_value_changed                       (GtkAdjustment *adj,
                                        gpointer         user_data)
{
  static gchar bits[] = "10000000";
  static gchar care[] = "00000111";
  static gchar both[] = "10000XXX";
  static gchar num[32], lvl[32];
  static int i, j, k, bit, level = 128;

  if (((gchar *)user_data)[0] == '1') {
    level = adj->value;
    for (i = 0; i < 8; i++) {
      bits[i] = (level & (1 << (7 - i))) ? '1' : '0';
    }
  } else if (adj->value > -1)
    for (i = 0; i < 8; i++) {
      care[i] = i >= adj->value ? '1' : '0';
    }
  bit = 128;
  j = 1;
  k = 0;
  for (i = 0; i < 8; i++) {
    both[i] = (care[i] == '1') ? (bits[i] == '0' ? 'X' : 'Y') : bits[i];
    if (care[i] != '1') {
      j *= 2;
      if (bits[i] == '1')
	k += bit;
    }
    bit >>= 1;
  }
//  bits[i] = '\0';
  /* printf("bits: %s\tcare: %s\tboth: %s\n", bits, care, both); */
  gtk_entry_set_text(GTK_ENTRY(LU("entry1")), both);
  sprintf(num, "%d @ %.4g Volts", j, dialog_volts * 2 / j);
  gtk_label_set_text(GTK_LABEL(LU("label21")), num);
  sprintf(lvl, "%.4f to %.4f Volts (%02x=%d)",
	  dialog_volts * (k - 128) / 128,
	  dialog_volts * ((256 / j + k) - 128) / 128,
	  level, level);
  gtk_label_set_text(GTK_LABEL(LU("label19")), lvl);
}


void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        gint             page_num,
                                        gpointer         user_data)
{
/*    printf("page num: %d\n", page_num); */
  propagate_changes();
}

void
on_ok                                  (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_signal_emit_by_name(GTK_OBJECT(LU("button2")), "clicked");
  printf("ok\n");
  gtk_widget_destroy(LU("dialog2"));
/*    gtk_main_quit(); */
}


void
on_apply                               (GtkButton       *button,
                                        gpointer         user_data)
{
  /* snag the state of all widgets into bs.r & reset BitScope */
  printf("apply\n");
}

void
on_entry1_changed                      (GtkEditable     *editable,
                                        gpointer         user_data)
{
  int bits = 0, care = 0, bit = 1, i;
  gchar s[] = "\0\0\0\0\0\0\0\0\0";

  strncpy(s, gtk_entry_get_text(GTK_ENTRY(editable)), 8);

  for (i = 7; i >= 0; i--) {
    if (s[i] == 'y') s[i] = 'Y';
    if (s[i] == 'x') s[i] = 'X';
    if (s[i] != '0' && s[i] != '1' && s[i] != 'X' && s[i] != 'Y') s[i] = 'X';
    if (s[i] == '1' || s[i] == 'Y') bits += bit;
    if (care >= 0 && s[i] && (s[i] == '0' || s[i] == '1')) care++;
    if (care > 0 && (s[i] == 'X' || s[i] == 'Y')) care = -1;
    bit <<= 1;
  }
  printf("text:%s\tcare=%d\tbits=%d\n", s, care, bits);
  gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(LU("hscale1"))),
			   care);
  gtk_adjustment_set_value(gtk_range_get_adjustment(GTK_RANGE(LU("hscale2"))),
			   bits);
  gtk_entry_set_text(GTK_ENTRY(editable), s);
}

gboolean
on_entry1_focusout                     (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gtk_signal_emit_by_name(GTK_OBJECT(LU("entry1")), "activate");
  return FALSE;
}
