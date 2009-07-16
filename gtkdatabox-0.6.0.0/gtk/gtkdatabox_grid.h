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
 * @file gtkdatabox_grid.h
 * GRIDs can display data or other things in a GtkDatabox widget.
 *
 * This class is just the basic interface. Other classes are derived from this 
 * class and implement some real things.
 * 
 */

#ifndef __GTK_DATABOX_GRID_H__
#define __GTK_DATABOX_GRID_H__

#include <gtkdatabox_graph.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

#define GTK_DATABOX_TYPE_GRID		  (gtk_databox_grid_get_type ())
#define GTK_DATABOX_GRID(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           GTK_DATABOX_TYPE_GRID, \
                                           GtkDataboxGrid))
#define GTK_DATABOX_GRID_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                           GTK_DATABOX_TYPE_GRID, \
                                           GtkDataboxGridClass))
#define GTK_DATABOX_IS_GRID(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           GTK_DATABOX_TYPE_GRID))
#define GTK_DATABOX_IS_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                           GTK_DATABOX_TYPE_GRID))
#define GTK_DATABOX_GRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                           GTK_DATABOX_TYPE_GRID, \
                                           GtkDataboxGridClass))

   typedef struct _GtkDataboxGrid GtkDataboxGrid;

   typedef struct _GtkDataboxGridClass GtkDataboxGridClass;

   typedef struct _GtkDataboxGridPrivate GtkDataboxGridPrivate;
   
   typedef enum
   {
      GTK_DATABOX_GRID_DASHED_LINES = 0,   /* Grid drawn with dashed lines */
      GTK_DATABOX_GRID_SOLID_LINES,        /* Grid drawn with solid lines */
      GTK_DATABOX_GRID_DOTTED_LINES        /* Grid drawn with dotted lines */
   }
   GtkDataboxGridLineStyle;
   
   struct _GtkDataboxGrid
   {
      GtkDataboxGraph parent;

      GtkDataboxGridPrivate *priv;
   };

   struct _GtkDataboxGridClass
   {
      GtkDataboxGraphClass parent_class;
   };

   GType gtk_databox_grid_get_type (void);

   GtkDataboxGraph *gtk_databox_grid_new (gint hlines, gint vlines, 
                                    GdkColor *color, guint size);

   void gtk_databox_grid_set_hlines (GtkDataboxGrid *grid, 
                                      gint hlines);
   gint gtk_databox_grid_get_hlines (GtkDataboxGrid *grid);

   void gtk_databox_grid_set_vlines (GtkDataboxGrid *grid, 
                                      gint vlines);
   gint gtk_databox_grid_get_vlines (GtkDataboxGrid *grid);

   void gtk_databox_grid_set_line_style (GtkDataboxGrid *grid, 
                                         gint line_style);
   gint gtk_databox_grid_get_line_style (GtkDataboxGrid *grid);

#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_DATABOX_GRID_H__ */
