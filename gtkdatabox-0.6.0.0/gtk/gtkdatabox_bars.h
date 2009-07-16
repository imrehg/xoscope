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
 * @file gtkdatabox_bars.h
 * BARSs can display data or other things in a GtkDatabox widget.
 *
 * This class is just the basic interface. Other classes are derived from this 
 * class and implement some real things.
 * 
 */

#ifndef __GTK_DATABOX_BARS_H__
#define __GTK_DATABOX_BARS_H__

#include <gtkdatabox_xyc_graph.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_DATABOX_TYPE_BARS		  (gtk_databox_bars_get_type ())
#define GTK_DATABOX_BARS(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_DATABOX_TYPE_BARS, \
                                           GtkDataboxBars))
#define GTK_DATABOX_BARS_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_DATABOX_TYPE_BARS, \
                                           GtkDataboxBarsClass))
#define GTK_DATABOX_IS_BARS(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_DATABOX_TYPE_BARS))
#define GTK_DATABOX_IS_BARS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_DATABOX_TYPE_BARS))
#define GTK_DATABOX_BARS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_DATABOX_TYPE_BARS, \
                                           GtkDataboxBarsClass))

   typedef struct _GtkDataboxBars GtkDataboxBars;

   typedef struct _GtkDataboxBarsClass GtkDataboxBarsClass;

   typedef struct _GtkDataboxBarsPrivate GtkDataboxBarsPrivate;
   
   struct _GtkDataboxBars
   {
      GtkDataboxXYCGraph parent;

      GtkDataboxBarsPrivate *priv;
   };

   struct _GtkDataboxBarsClass
   {
      GtkDataboxXYCGraphClass parent_class;
   };

   GType gtk_databox_bars_get_type (void);

   GtkDataboxGraph *gtk_databox_bars_new (gint len, gfloat *X, gfloat *Y, 
                                    GdkColor *color, guint size);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_BARS_H__ */
