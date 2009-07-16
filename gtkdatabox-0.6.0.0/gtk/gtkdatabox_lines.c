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

#include <gtkdatabox_lines.h>

static void gtk_databox_lines_real_draw (GtkDataboxGraph *lines, 
                                  GtkDataboxCanvas *canvas);

struct _GtkDataboxLinesPrivate
{
   GdkPoint *data;
};

static gpointer parent_class = NULL;

static void lines_finalize (GObject *object) 
{
   GtkDataboxLines *lines = GTK_DATABOX_LINES (object);

   g_free (lines->priv->data);
   g_free (lines->priv);

   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_databox_lines_class_init (gpointer g_class,
                              gpointer g_class_data)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
   GtkDataboxGraphClass *graph_class = GTK_DATABOX_GRAPH_CLASS (g_class);
   GtkDataboxLinesClass *klass = GTK_DATABOX_LINES_CLASS (g_class);

   parent_class = g_type_class_peek_parent (klass);
   
   gobject_class->finalize     = lines_finalize;

   graph_class->draw = gtk_databox_lines_real_draw;
}

static void
gtk_databox_lines_complete (GtkDataboxLines *lines)
{
   lines->priv->data = 
      g_new0 (GdkPoint, 
              gtk_databox_xyc_graph_get_length 
                 (GTK_DATABOX_XYC_GRAPH (lines)));

}

static void
gtk_databox_lines_instance_init (GTypeInstance   *instance,
                                 gpointer         g_class)
{
   GtkDataboxLines *lines = GTK_DATABOX_LINES (instance);

   lines->priv = g_new0 (GtkDataboxLinesPrivate, 1);
   
   g_signal_connect (lines, "notify::length",
                     G_CALLBACK (gtk_databox_lines_complete), NULL);
}

GType
gtk_databox_lines_get_type (void)
{
   static GType type = 0;
   
   if (type == 0) {
      static const GTypeInfo info = 
      {
         sizeof (GtkDataboxLinesClass),
         NULL,                                  /* base_init */
         NULL,                                  /* base_finalize */
         gtk_databox_lines_class_init,          /* class_init */
         NULL,                                  /* class_finalize */
         NULL,                                  /* class_data */
         sizeof (GtkDataboxLines),
         0,                                     /* n_preallocs */
         gtk_databox_lines_instance_init        /* instance_init */
      };
      type = g_type_register_static (GTK_DATABOX_TYPE_XYC_GRAPH,
                                     "GtkDataboxLines",
                                     &info, 0);
   }
   
   return type;
}

GtkDataboxGraph *
gtk_databox_lines_new (gint len, gfloat *X, gfloat *Y, 
                        GdkColor *color, guint size)
{
  GtkDataboxLines *lines;
  g_return_val_if_fail (X, NULL);
  g_return_val_if_fail (Y, NULL);
  g_return_val_if_fail ((len > 0), NULL);

  lines = g_object_new (GTK_DATABOX_TYPE_LINES,
                        "X-Values", X,
                        "Y-Values", Y,
                        "length", len,
                        "color", color, 
                        "size", size,
                        NULL);

  return GTK_DATABOX_GRAPH (lines);
}

static void
gtk_databox_lines_real_draw (GtkDataboxGraph *graph,
                             GtkDataboxCanvas *canvas)
{
   GtkDataboxLines *lines = GTK_DATABOX_LINES (graph);
   GdkPoint *data;
   guint i = 0;
   gfloat *X;
   gfloat *Y;
   gint len;
   gint size = 0;

   g_return_if_fail (GTK_DATABOX_IS_LINES (lines));
   g_return_if_fail (canvas);
   g_return_if_fail (canvas->pixmap);
   g_return_if_fail (canvas->style);

   if (!graph->gc) 
      gtk_databox_graph_create_gc (graph, canvas);

   len = gtk_databox_xyc_graph_get_length (GTK_DATABOX_XYC_GRAPH (graph));
   X = gtk_databox_xyc_graph_get_X (GTK_DATABOX_XYC_GRAPH (graph));
   Y = gtk_databox_xyc_graph_get_Y (GTK_DATABOX_XYC_GRAPH (graph));
   size = gtk_databox_graph_get_size (graph);

   data = lines->priv->data;

   for (i = 0; i < len; i++, data++, X++, Y++)
   {
      data->x =
        (gint16) ((*X - canvas->top_left_visible.x) 
                   * canvas->translation_factor.x);
      data->y =
        (gint16) ((*Y - canvas->top_left_visible.y) 
                   * canvas->translation_factor.y);
   }
   
   /* More than 2^16 lines will cause X IO error on most XServers
      (Hint from Paul Barton-Davis) */
   for (i = 0; i < len; i += 65536)
   {
      gdk_draw_lines (canvas->pixmap, graph->gc, lines->priv->data+ i,
                       MIN (65536, len - i));
   }

   return;
}

