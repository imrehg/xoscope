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
 * @file gtkdatabox_marker.h
 * MARKERs can display data or other things in a GtkDatabox widget.
 *
 * This class is just the basic interface. Other classes are derived from this 
 * class and implement some real things.
 * 
 */

#ifndef __GTK_DATABOX_MARKER_H__
#define __GTK_DATABOX_MARKER_H__

#include <gtkdatabox_xyc_graph.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_DATABOX_TYPE_MARKER		  (gtk_databox_marker_get_type ())
#define GTK_DATABOX_MARKER(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_DATABOX_TYPE_MARKER, \
                                           GtkDataboxMarker))
#define GTK_DATABOX_MARKER_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_DATABOX_TYPE_MARKER, \
                                           GtkDataboxMarkerClass))
#define GTK_DATABOX_IS_MARKER(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_DATABOX_TYPE_MARKER))
#define GTK_DATABOX_IS_MARKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_DATABOX_TYPE_MARKER))
#define GTK_DATABOX_MARKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_DATABOX_TYPE_MARKER, \
                                           GtkDataboxMarkerClass))

   typedef struct _GtkDataboxMarker GtkDataboxMarker;

   typedef struct _GtkDataboxMarkerClass GtkDataboxMarkerClass;

   typedef struct _GtkDataboxMarkerPrivate GtkDataboxMarkerPrivate;
   
   typedef enum 
   {
      GTK_DATABOX_MARKER_C    = 0, /* centered on data */
      GTK_DATABOX_MARKER_N,        /* marker north of data */
      GTK_DATABOX_MARKER_E,        /* marker east  of data */
      GTK_DATABOX_MARKER_S,        /* marker south of data */
      GTK_DATABOX_MARKER_W         /* marker west  of data */
   } 
   GtkDataboxMarkerPosition;
   
   typedef enum
   {
      GTK_DATABOX_TEXT_CENTER       = 0, /* text centered   on marker */
      GTK_DATABOX_TEXT_N,                /* text north      of marker */
      GTK_DATABOX_TEXT_NE,               /* text north-east of marker */
      GTK_DATABOX_TEXT_E,                /* text east       of marker */
      GTK_DATABOX_TEXT_SE,               /* text south-east of marker */
      GTK_DATABOX_TEXT_S,                /* text south      of marker */
      GTK_DATABOX_TEXT_SW,               /* text south-west of marker */
      GTK_DATABOX_TEXT_W,                /* text west       of marker */
      GTK_DATABOX_TEXT_NW                /* text north-west of marker */
   }
   GtkDataboxTextPosition;

   typedef enum
   {
      GTK_DATABOX_MARKER_NONE = 0,     /* No Marker (just text) */
      GTK_DATABOX_MARKER_TRIANGLE,     /* Marker is a triangle */
      GTK_DATABOX_MARKER_SOLID_LINE,   /* Marker is a solid line */
      GTK_DATABOX_MARKER_DASHED_LINE   /* Marker is a dashed line */
   }
   GtkDataboxMarkerType;
   
   struct _GtkDataboxMarker
   {
      GtkDataboxXYCGraph parent;

      GtkDataboxMarkerPrivate *priv;
   };

   struct _GtkDataboxMarkerClass
   {
      GtkDataboxXYCGraphClass parent_class;
   };

   GType gtk_databox_marker_get_type (void);

   GtkDataboxGraph *gtk_databox_marker_new (gint len, gfloat *X, gfloat *Y, 
                                    GdkColor *color, guint size,
                                    GtkDataboxMarkerType type);
   void gtk_databox_marker_set_position (GtkDataboxMarker *marker,
                                         guint index, 
                                         GtkDataboxMarkerPosition position);
   
   void gtk_databox_marker_set_label (GtkDataboxMarker *marker,
                                      guint index, 
                                      GtkDataboxTextPosition label_position,
                                      gchar *text,
                                      gboolean boxed);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_MARKER_H__ */
