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

#include <gtkdatabox_cross_simple.h>

static gpointer parent_class = NULL;

static void cross_simple_finalize (GObject *object) 
{
   gpointer pointer; 
   GtkDataboxCrossSimple *cross_simple = GTK_DATABOX_CROSS_SIMPLE (object);

   g_free (cross_simple->priv);

   pointer = gtk_databox_xyc_graph_get_X (GTK_DATABOX_XYC_GRAPH (object));
   if (pointer) g_free (pointer);

   pointer = gtk_databox_xyc_graph_get_Y (GTK_DATABOX_XYC_GRAPH (object));
   if (pointer) g_free (pointer);

   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_databox_cross_simple_class_init (gpointer g_class,
                              gpointer g_class_data)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
   GtkDataboxCrossSimpleClass *klass = GTK_DATABOX_CROSS_SIMPLE_CLASS (g_class);

   parent_class = g_type_class_peek_parent (klass);
   
   gobject_class->finalize  = cross_simple_finalize;
}

static void
gtk_databox_cross_simple_instance_init (GTypeInstance   *instance,
                                 gpointer         g_class)
{
   return;
}

GType
gtk_databox_cross_simple_get_type (void)
{
   static GType type = 0;
   
   if (type == 0) {
      static const GTypeInfo info = 
      {
         sizeof (GtkDataboxCrossSimpleClass),
         NULL,                                  /* base_init */
         NULL,                                  /* base_finalize */
         gtk_databox_cross_simple_class_init,          /* class_init */
         NULL,                                  /* class_finalize */
         NULL,                                  /* class_data */
         sizeof (GtkDataboxCrossSimple),
         0,                                     /* n_preallocs */
         gtk_databox_cross_simple_instance_init /* instance_init */
      };
      type = g_type_register_static (GTK_DATABOX_TYPE_MARKER,
                                     "GtkDataboxCrossSimple",
                                     &info, 0);
   }
   
   return type;
}

GtkDataboxGraph *
gtk_databox_cross_simple_new (GdkColor *color, guint size)
{
  GtkDataboxCrossSimple *cross_simple;
  gfloat *X = g_new0 (gfloat, 2);
  gfloat *Y = g_new0 (gfloat, 2);
  gint len = 2;

  cross_simple = g_object_new (GTK_DATABOX_TYPE_CROSS_SIMPLE, 
                               "marker-type", GTK_DATABOX_MARKER_SOLID_LINE,
                               "X-Values", X,
                               "Y-Values", Y,
                               "length", len,
                               "color", color,
                               "size", size,
                               NULL);

  gtk_databox_marker_set_position (GTK_DATABOX_MARKER (cross_simple), 0,
                                   GTK_DATABOX_MARKER_C);
  gtk_databox_marker_set_label (GTK_DATABOX_MARKER (cross_simple), 0,
                                GTK_DATABOX_TEXT_SW, "0", FALSE);
  gtk_databox_marker_set_position (GTK_DATABOX_MARKER (cross_simple), 1, 
                                   GTK_DATABOX_MARKER_W);

  return GTK_DATABOX_GRAPH (cross_simple);
}

