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
 * @file gtkdatabox_xyc_graph.h
 * XYC_GRAPHs can display data or other things in a GtkDatabox widget.
 *
 * This class is just the basic interface. Other classes are derived from this 
 * class and implement some real things.
 * 
 */

#ifndef __GTK_DATABOX_XYC_GRAPH_H__
#define __GTK_DATABOX_XYC_GRAPH_H__

#include <gtkdatabox_graph.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_DATABOX_TYPE_XYC_GRAPH		  (gtk_databox_xyc_graph_get_type ())
#define GTK_DATABOX_XYC_GRAPH(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_DATABOX_TYPE_XYC_GRAPH, \
                                           GtkDataboxXYCGraph))
#define GTK_DATABOX_XYC_GRAPH_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_DATABOX_TYPE_XYC_GRAPH, \
                                           GtkDataboxXYCGraphClass))
#define GTK_DATABOX_IS_XYC_GRAPH(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_DATABOX_TYPE_XYC_GRAPH))
#define GTK_DATABOX_IS_XYC_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_DATABOX_TYPE_XYC_GRAPH))
#define GTK_DATABOX_XYC_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_DATABOX_TYPE_XYC_GRAPH, \
                                           GtkDataboxXYCGraphClass))

   typedef struct _GtkDataboxXYCGraph GtkDataboxXYCGraph;

   typedef struct _GtkDataboxXYCGraphClass GtkDataboxXYCGraphClass;

   typedef struct _GtkDataboxXYCGraphPrivate GtkDataboxXYCGraphPrivate;
   
   struct _GtkDataboxXYCGraph
   {
      GtkDataboxGraph parent;

      GtkDataboxXYCGraphPrivate *priv;
   };

   struct _GtkDataboxXYCGraphClass
   {
      GtkDataboxGraphClass parent_class;
   };

   GType gtk_databox_xyc_graph_get_type (void);

   GtkDataboxGraph *gtk_databox_xyc_graph_new (gint len, gfloat *X, gfloat *Y, 
                                    GdkColor *color, gint size);

   gint gtk_databox_xyc_graph_get_length (GtkDataboxXYCGraph *xyc_graph);
   gfloat *gtk_databox_xyc_graph_get_X (GtkDataboxXYCGraph *xyc_graph);
   gfloat *gtk_databox_xyc_graph_get_Y (GtkDataboxXYCGraph *xyc_graph);
#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_XYC_GRAPH_H__ */
