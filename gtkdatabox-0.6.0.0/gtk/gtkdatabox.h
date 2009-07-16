/* GtkDatabox - An extension to the gtk+ library
 * Copyright (C) 1998 - 2006  Dr. Roland Bock
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
 * @file gtkdatabox.h
 * Main interface to the GtkDatabox library
 * 
 */

#ifndef __GTK_DATABOX_H__
#define __GTK_DATABOX_H__

#include <gtkdatabox_graph.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkruler.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_TYPE_DATABOX		  (gtk_databox_get_type ())
#define GTK_DATABOX(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_TYPE_DATABOX, \
                                           GtkDatabox))
#define GTK_DATABOX_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_TYPE_DATABOX, \
                                           GtkDataboxClass))
#define GTK_IS_DATABOX(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_TYPE_DATABOX))
#define GTK_IS_DATABOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_TYPE_DATABOX))
#define GTK_DATABOX_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_TYPE_DATABOX, \
                                           GtkDataboxClass))

   typedef struct _GtkDatabox GtkDatabox;

   typedef struct _GtkDataboxClass GtkDataboxClass;

   typedef struct _GtkDataboxPrivate GtkDataboxPrivate;
   
   struct _GtkDatabox
   {
      GtkDrawingArea box;

      GtkDataboxPrivate *priv;
   };

   struct _GtkDataboxClass
   {
      GtkDrawingAreaClass parent_class;

      void (*zoomed) (GtkDatabox *box,
                      GtkDataboxValue *top_left,
                      GtkDataboxValue *bottom_right);
      void (*selection_started) (GtkDatabox *box,
                                 GtkDataboxCoord *marked);
      void (*selection_changed) (GtkDatabox *box,
                                 GtkDataboxCoord *marked,
                                 GtkDataboxCoord *select);
      void (*selection_finalized) (GtkDatabox *box,
                                 GtkDataboxCoord *marked,
                                 GtkDataboxCoord *select);
      void (*selection_cancelled) (GtkDatabox *box);
   };
   
   GType gtk_databox_get_type (void);
   
   GtkWidget *gtk_databox_new (void);
   
   gboolean gtk_databox_graph_add (GtkDatabox *box, GtkDataboxGraph *graph);
   void gtk_databox_graph_remove (GtkDatabox *box, GtkDataboxGraph *graph);
   void gtk_databox_graph_remove_all (GtkDatabox *box);
   
   void gtk_databox_set_enable_selection (GtkDatabox *box, gboolean enable);
   void gtk_databox_set_enable_zoom (GtkDatabox *box, gboolean enable);
   
   gboolean gtk_databox_get_enable_selection (GtkDatabox *box);
   gboolean gtk_databox_get_enable_zoom (GtkDatabox *box);

   void gtk_databox_set_hadjustment (GtkDatabox *box, GtkAdjustment *adj);
   void gtk_databox_set_vadjustment (GtkDatabox *box, GtkAdjustment *adj);
   GtkAdjustment *gtk_databox_get_hadjustment (GtkDatabox *box);
   GtkAdjustment *gtk_databox_get_vadjustment (GtkDatabox *box);
   
   void gtk_databox_set_hruler (GtkDatabox *box, GtkRuler *ruler);
   void gtk_databox_set_vruler (GtkDatabox *box, GtkRuler *ruler);
   GtkRuler *gtk_databox_get_hruler (GtkDatabox *box);
   GtkRuler *gtk_databox_get_vruler (GtkDatabox *box);
   
   gint gtk_databox_auto_rescale (GtkDatabox *box, gfloat border);
   gint gtk_databox_calculate_extrema (GtkDatabox *box,
                                 GtkDataboxValue *min,
                                 GtkDataboxValue *max);
   
   void gtk_databox_set_canvas (GtkDatabox *box,
                                 GtkDataboxValue top_left,
                                 GtkDataboxValue bottom_right);
   void gtk_databox_set_visible_canvas (GtkDatabox *box,
                                 GtkDataboxValue top_left,
                                 GtkDataboxValue bottom_right);
   void gtk_databox_get_canvas (GtkDatabox *box,
                                 GtkDataboxValue *top_left,
                                 GtkDataboxValue *bottom_right);
   void gtk_databox_get_visible_canvas (GtkDatabox *box,
                                 GtkDataboxValue *top_left,
                                 GtkDataboxValue *bottom_right);

   void gtk_databox_zoom_to_selection (GtkDatabox *box);
   void gtk_databox_zoom_out (GtkDatabox *box);
   void gtk_databox_zoom_home (GtkDatabox *box);
   
   void gtk_databox_redraw (GtkDatabox *box);
   
   GtkDataboxValue gtk_databox_value_from_coord (GtkDatabox *box, 
                                                 GtkDataboxCoord coord);
   
   GtkDataboxCoord gtk_databox_coord_from_value (GtkDatabox *box, 
                                                 GtkDataboxValue value);
   
   /* For your convenience, especially for developers who have used older 
    * versions (GtkDatabox was a composite widget with scrollbars and 
    * rulers, then) */
   void gtk_databox_create_box_with_scrollbars_and_rulers (GtkWidget **p_box, 
                                                        GtkWidget **p_table,
                                                        gboolean hscrollbar,
                                                        gboolean vscrollbar,
                                                        gboolean hruler,
                                                        gboolean vruler);
#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_H__ */
