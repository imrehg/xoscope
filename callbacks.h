#include <gtk/gtk.h>


GtkWidget*
create_databox (gchar *widget_name, gchar *string1, gchar *string2,
                gint int1, gint int2);


gint key_press_event(GtkWidget *widget, GdkEventKey *event);

void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);

/* COMEDI */

void
comedi_on_ok                           (GtkButton       *button,
                                        gpointer         user_data);

void
comedi_on_apply                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_bufsize_custom_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_bufsize_default_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_device_entry_changed                (GtkEditable     *editable,
                                        gpointer         user_data);

/* Bitscope */

extern GtkWidget *window;
extern guchar dialog_r[];

void
bitscope_dialog();

void
on_toggled                             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_value_changed                       (GtkAdjustment *adj,
                                        gpointer         user_data);

void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        gint             page_num,
                                        gpointer         user_data);

void
on_ok                                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_apply                               (GtkButton       *button,
                                        gpointer         user_data);

void
on_entry1_changed                      (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_entry1_focusout                     (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_databox_button_press_event          (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
