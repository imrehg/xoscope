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
 * @file gtkdatabox_graph.h
 * Graphs can display data or other things in a GtkDatabox widget.
 *
 * This class is just the basic interface. Other classes are derived from this 
 * class and implement some real things.
 * 
 */

#ifndef __GTK_DATABOX_GRAPH_H__
#define __GTK_DATABOX_GRAPH_H__

#include <gtk/gtkstyle.h>
#include <gdk/gdk.h>
#include <pango/pango.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_DATABOX_TYPE_GRAPH		  (gtk_databox_graph_get_type ())
#define GTK_DATABOX_GRAPH(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_DATABOX_TYPE_GRAPH, \
                                           GtkDataboxGraph))
#define GTK_DATABOX_GRAPH_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_DATABOX_TYPE_GRAPH, \
                                           GtkDataboxGraphClass))
#define GTK_DATABOX_IS_GRAPH(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_DATABOX_TYPE_GRAPH))
#define GTK_DATABOX_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_DATABOX_TYPE_GRAPH))
#define GTK_DATABOX_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_DATABOX_TYPE_GRAPH, \
                                           GtkDataboxGraphClass))

   typedef struct _GtkDataboxGraph GtkDataboxGraph;

   typedef struct _GtkDataboxGraphClass GtkDataboxGraphClass;

   typedef struct
   {
      gint x;
      gint y;
   }
   GtkDataboxCoord;

   typedef struct
   {
      gfloat x;
      gfloat y;
   }
   GtkDataboxValue;

   typedef struct 
   {
      GtkDataboxValue top_left_total;
      GtkDataboxValue bottom_right_total;
      GtkDataboxValue top_left_visible;
      GtkDataboxValue bottom_right_visible;
      gint width;
      gint height;
      GtkDataboxValue translation_factor;
      GdkPixmap *pixmap;
      GtkStyle *style;
      PangoContext *context;
   } GtkDataboxCanvas;
   
   typedef struct _GtkDataboxGraphPrivate GtkDataboxGraphPrivate;
   
   struct _GtkDataboxGraph
   {
      GObject parent;

      GtkDataboxGraphPrivate *priv;

      GdkGC *gc;
   };

   struct _GtkDataboxGraphClass
   {
      GObjectClass parent_class;

      /*
       * public virtual drawing function 
       */
      void (*draw) (GtkDataboxGraph *graph, 
                    GtkDataboxCanvas *canvas);

      gint (*calculate_extrema) (GtkDataboxGraph *graph, 
                                 GtkDataboxValue *min,
                                 GtkDataboxValue *max);
      void (*create_gc) (GtkDataboxGraph *graph, GtkDataboxCanvas *canvas);
   };

   GType gtk_databox_graph_get_type (void);

   GObject *gtk_databox_graph_new (void);

   void gtk_databox_graph_set_hide (GtkDataboxGraph *graph, gboolean hide);
   gboolean gtk_databox_graph_get_hide (GtkDataboxGraph *graph);

   void gtk_databox_graph_set_color (GtkDataboxGraph *graph, 
                                      GdkColor *color);
   GdkColor *gtk_databox_graph_get_color (GtkDataboxGraph *graph);

   void gtk_databox_graph_set_size (GtkDataboxGraph *graph, 
                                     gint size);
   gint gtk_databox_graph_get_size (GtkDataboxGraph *graph);

   gint gtk_databox_graph_calculate_extrema (GtkDataboxGraph *graph, 
                                             GtkDataboxValue *min,
                                             GtkDataboxValue *max);
   /* This function is called by GtkDatabox */
   void gtk_databox_graph_draw (GtkDataboxGraph *graph, 
                                GtkDataboxCanvas *canvas);

   /* This function is called by derived graph classes */
   void gtk_databox_graph_create_gc (GtkDataboxGraph *graph,
                                     GtkDataboxCanvas *canvas);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_GRAPH_H__ */
