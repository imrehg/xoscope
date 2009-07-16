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

#include <gtkdatabox_marker.h>
#include <pango/pango.h>

static void gtk_databox_marker_real_draw (GtkDataboxGraph *marker, 
                                  GtkDataboxCanvas *canvas);
static void gtk_databox_marker_real_create_gc (GtkDataboxGraph *graph,
                                       GtkDataboxCanvas *canvas);

/* IDs of properties */
enum
{
   PROP_TYPE = 1
};


typedef struct 
{
   GtkDataboxMarkerPosition position;       /* relative to data point */
   gchar *text;
   PangoLayout *label;                      /* the label for marker */
   GtkDataboxTextPosition label_position;   /* position relative to marker */
   gboolean boxed;                          /* label in a box? */
}
GtkDataboxMarkerInfo;

struct _GtkDataboxMarkerPrivate
{
   GtkDataboxMarkerType type;
   GtkDataboxMarkerInfo *marker_info;
   GdkGC *label_gc;
};

static gpointer parent_class = NULL;

static void 
gtk_databox_marker_set_mtype (GtkDataboxMarker *marker, gint type)
{
   g_return_if_fail (GTK_DATABOX_IS_MARKER (marker));

   marker->priv->type = type;
   
   g_object_notify (G_OBJECT (marker), "marker-type");
}

static void
gtk_databox_marker_set_property (GObject      *object,
                    guint         property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (object);

   switch (property_id) 
   {
      case PROP_TYPE: 
         {
            gtk_databox_marker_set_mtype (marker, g_value_get_int (value));
         }
         break;
      default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
   }
}

static gint 
gtk_databox_marker_get_mtype (GtkDataboxMarker *marker)
{
   g_return_val_if_fail (GTK_DATABOX_IS_MARKER (marker), 0);

   return marker->priv->type;
}

static void
gtk_databox_marker_get_property (GObject      *object,
                    guint         property_id,
                    GValue       *value,
                    GParamSpec   *pspec)
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (object);

   switch (property_id) 
   {
      case PROP_TYPE: 
         {
            g_value_set_int (value, gtk_databox_marker_get_mtype (marker));
         }
         break;
      default:
         /* We don't have any other property... */
         G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
         break;
   }
}

static void marker_finalize (GObject *object) 
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (object);

   g_free (marker->priv->marker_info);
   g_free (marker->priv);

   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_databox_marker_real_create_gc (GtkDataboxGraph *graph,
                                  GtkDataboxCanvas *canvas)
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (graph);
   GdkGCValues values;

   g_return_if_fail (GTK_DATABOX_IS_MARKER (graph));

   GTK_DATABOX_GRAPH_CLASS (parent_class)->create_gc (graph, canvas);

   if (marker->priv->type == GTK_DATABOX_MARKER_DASHED_LINE)
   {
      values.line_style = GDK_LINE_ON_OFF_DASH;
      values.cap_style = GDK_CAP_BUTT;
      values.join_style = GDK_JOIN_MITER;
      gdk_gc_set_values (graph->gc, &values, 
                         GDK_GC_LINE_STYLE | 
                         GDK_GC_CAP_STYLE |
                         GDK_GC_JOIN_STYLE);
   }
   
   if (marker->priv->label_gc)
      g_object_unref (marker->priv->label_gc);

   marker->priv->label_gc = gdk_gc_new (canvas->pixmap);
   gdk_gc_copy(marker->priv->label_gc, graph->gc);
   gdk_gc_set_line_attributes (marker->priv->label_gc, 1, 
                               GDK_LINE_SOLID,
                               GDK_CAP_ROUND,
                               GDK_JOIN_ROUND);
}

static void
gtk_databox_marker_class_init (gpointer g_class,
                              gpointer g_class_data)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
   GtkDataboxGraphClass *graph_class = GTK_DATABOX_GRAPH_CLASS (g_class);
   GtkDataboxMarkerClass *klass = GTK_DATABOX_MARKER_CLASS (g_class);
   GParamSpec *marker_param_spec;

   parent_class = g_type_class_peek_parent (klass);
   
   gobject_class->set_property = gtk_databox_marker_set_property;
   gobject_class->get_property = gtk_databox_marker_get_property;
   gobject_class->finalize     = marker_finalize;

   marker_param_spec = g_param_spec_int ("marker-type",
                                         "Type of marker",
                                         "Type of marker for this graph, e.g. triangles or lines",
                                         G_MININT,
                                         G_MAXINT,
                                         0, /*  default value */
                                         G_PARAM_CONSTRUCT |
                                         G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    PROP_TYPE,
                                    marker_param_spec);
   graph_class->draw = gtk_databox_marker_real_draw;
   graph_class->create_gc = gtk_databox_marker_real_create_gc;
}

static void
complete (GtkDataboxMarker *marker)
{
   marker->priv->marker_info = 
      g_new0 (GtkDataboxMarkerInfo, 
              gtk_databox_xyc_graph_get_length 
                 (GTK_DATABOX_XYC_GRAPH (marker)));

}

static void
gtk_databox_marker_instance_init (GTypeInstance   *instance,
                                 gpointer         g_class)
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (instance);

   marker->priv = g_new0 (GtkDataboxMarkerPrivate, 1);
   
   g_signal_connect (marker, "notify::length",
                     G_CALLBACK (complete), NULL);
}

GType
gtk_databox_marker_get_type (void)
{
   static GType type = 0;
   
   if (type == 0) {
      static const GTypeInfo info = 
      {
         sizeof (GtkDataboxMarkerClass),
         NULL,                                  /* base_init */
         NULL,                                  /* base_finalize */
         gtk_databox_marker_class_init,          /* class_init */
         NULL,                                  /* class_finalize */
         NULL,                                  /* class_data */
         sizeof (GtkDataboxMarker),
         0,                                     /* n_preallocs */
         gtk_databox_marker_instance_init        /* instance_init */
      };
      type = g_type_register_static (GTK_DATABOX_TYPE_XYC_GRAPH,
                                     "GtkDataboxMarker",
                                     &info, 0);
   }
   
   return type;
}

GtkDataboxGraph *
gtk_databox_marker_new (gint len, gfloat *X, gfloat *Y, 
                        GdkColor *color, guint size,
                        GtkDataboxMarkerType type)
{
  GtkDataboxMarker *marker;
  g_return_val_if_fail (X, NULL);
  g_return_val_if_fail (Y, NULL);
  g_return_val_if_fail ((len > 0), NULL);

  marker = g_object_new (GTK_DATABOX_TYPE_MARKER, 
                         "X-Values", X,
                         "Y-Values", Y,
                         "length", len,
                         "color", color, 
                         "size", size,
                         "marker-type", type,
                         NULL);

  return GTK_DATABOX_GRAPH (marker);
}

static gint
gtk_databox_label_write_at (GdkPixmap *pixmap, 
                           PangoLayout *pl, 
                           GdkGC *gc, 
                           GtkDataboxCoord coord, 
                           GtkDataboxTextPosition position,
                           gint distance, 
                           gboolean boxed)
{
   gint hdist_text;
   gint vdist_text;
   gint hdist_box;
   gint vdist_box;

   gint width;
   gint height;

   gint offset = (boxed) ? 2 : 0;

   pango_layout_get_pixel_size (pl, &width, &height);

   switch (position)
   {
      case GTK_DATABOX_TEXT_N:  hdist_text = - width/2; 
                                vdist_text = - distance - offset - height;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_NE: hdist_text = + distance + offset;
                                vdist_text = - distance - offset - height;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_E:  hdist_text = + distance + offset;
                                vdist_text = - height/2;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_SE: hdist_text = + distance + offset;
                                vdist_text = + distance + offset;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_S:  hdist_text = - width/2;
                                vdist_text = + distance + offset;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_SW: hdist_text = - distance - offset - width;
                                vdist_text = + distance + offset;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_W:  hdist_text = - distance - offset - width;
                                vdist_text = - height/2;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      case GTK_DATABOX_TEXT_NW: hdist_text = - distance - offset - width;
                                vdist_text = - distance - offset - height;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
                                break;
      default:                  hdist_text = - width/2;
                                vdist_text = - height/2;
                                hdist_box  = hdist_text - offset;
                                vdist_box  = vdist_text - offset;
   }
   
   gdk_draw_layout (pixmap, gc, 
                    coord.x + hdist_text,
                    coord.y + vdist_text,
                    pl);

   if (boxed)
      gdk_draw_rectangle(pixmap, gc, FALSE, 
                         coord.x + hdist_box,
                         coord.y + vdist_box,
                         width + 3, 
                         height + 3);
                    
   return (0);
}

static void
gtk_databox_marker_real_draw (GtkDataboxGraph *graph,
                             GtkDataboxCanvas *canvas)
{
   GtkDataboxMarker *marker = GTK_DATABOX_MARKER (graph);
   GdkPoint points [3];
   gfloat *X;
   gfloat *Y;
   gint len;
   gint16 x;
   gint16 y;
   GtkDataboxCoord coord;
   gint size;
   gint i;
 
   g_return_if_fail (GTK_DATABOX_IS_MARKER (marker));
   g_return_if_fail (canvas);
   g_return_if_fail (canvas->pixmap);
   g_return_if_fail (canvas->context);

   if (!graph->gc) 
      gtk_databox_graph_create_gc (graph, canvas);

   len = gtk_databox_xyc_graph_get_length (GTK_DATABOX_XYC_GRAPH (graph));
   X = gtk_databox_xyc_graph_get_X (GTK_DATABOX_XYC_GRAPH (graph));
   Y = gtk_databox_xyc_graph_get_Y (GTK_DATABOX_XYC_GRAPH (graph));
   size = gtk_databox_graph_get_size (graph);

   for (i = 0; i < len; ++i)
   {
      x = (gint16) ((X[i] - canvas->top_left_visible.x) 
            * canvas->translation_factor.x);
      y = (gint16) ((Y[i] - canvas->top_left_visible.y) 
            * canvas->translation_factor.y);
      
      switch (marker->priv->type)
      {
         case GTK_DATABOX_MARKER_TRIANGLE:
            switch (marker->priv->marker_info[i].position) 
            {
               case GTK_DATABOX_MARKER_C:
                  coord.x = x;
                  coord.y = y;
                  y = y - size / 2;
                  points[0].x = x;
                  points[0].y = y;
                  points[1].x = x - size / 2;
                  points[1].y = y + size;
                  points[2].x = x + size / 2;
                  points[2].y = y + size;
                  break;
               case GTK_DATABOX_MARKER_N:
                  coord.x = x;
                  coord.y = y - 2 - size / 2;
                  y = y - 2;
                  points[0].x = x;
                  points[0].y = y;
                  points[1].x = x - size / 2;
                  points[1].y = y - size;
                  points[2].x = x + size / 2;
                  points[2].y = y - size;
                  break;
               case GTK_DATABOX_MARKER_E:
                  coord.x = x + 2 + size / 2;
                  coord.y = y;
                  x = x + 2;
                  points[0].x = x;
                  points[0].y = y;
                  points[1].x = x + size;
                  points[1].y = y + size / 2;
                  points[2].x = x + size;
                  points[2].y = y - size / 2;
                  break;
               case GTK_DATABOX_MARKER_S:
                  coord.x = x;
                  coord.y = y + 2 + size / 2;
                  y = y + 2;
                  points[0].x = x;
                  points[0].y = y;
                  points[1].x = x - size / 2;
                  points[1].y = y + size;
                  points[2].x = x + size / 2;
                  points[2].y = y + size;
                  break;
               case GTK_DATABOX_MARKER_W:
                  coord.x = x - 2 - size / 2;
                  coord.y = y;
                  x = x - 2;
                  points[0].x = x;
                  points[0].y = y;
                  points[1].x = x - size;
                  points[1].y = y + size / 2;
                  points[2].x = x - size;
                  points[2].y = y - size / 2;
                  break;
            }
            gdk_draw_polygon (canvas->pixmap, graph->gc, 
                              TRUE, points, 3);
            break;
            /* End of GTK_DATABOX_MARKER_TRIANGLE */
         case GTK_DATABOX_MARKER_SOLID_LINE:
         case GTK_DATABOX_MARKER_DASHED_LINE:
            switch (marker->priv->marker_info[i].position) 
            {
               case GTK_DATABOX_MARKER_C:
                  coord.x = x;
                  coord.y = y;
                  points[0].x = x;
                  points[0].y = 0;
                  points[1].x = x;
                  points[1].y = canvas->height;
                  break;
               case GTK_DATABOX_MARKER_N:
                  coord.x = x;
                  points[0].x = x;
                  points[0].y = 0;
                  points[1].x = x;
                  points[1].y = canvas->height;
                  break;
               case GTK_DATABOX_MARKER_E:
                  coord.y = y;
                  points[0].x = 0;
                  points[0].y = y;
                  points[1].x = canvas->width;
                  points[1].y = y;
                  break;
               case GTK_DATABOX_MARKER_S:
                  coord.x = x;
                  points[0].x = x;
                  points[0].y = 0;
                  points[1].x = x;
                  points[1].y = canvas->height;
                  break;
               case GTK_DATABOX_MARKER_W:
                  coord.y = y;
                  points[0].x = 0;
                  points[0].y = y;
                  points[1].x = canvas->width;
                  points[1].y = y;
                  break;
            }

            gdk_draw_line (canvas->pixmap, graph->gc,
                           points[0].x, points[0].y,
                           points[1].x, points[1].y);
            break;
            /* End of GTK_DATABOX_MARKER_LINE */

         case GTK_DATABOX_MARKER_NONE:
         default:
            coord.x = x;
            coord.y = y;
            break;
      }

      if (marker->priv->marker_info[i].text)
      {
         if (!marker->priv->marker_info[i].label)
         {
            marker->priv->marker_info[i].label = 
               pango_layout_new (canvas->context);
            pango_layout_set_text (marker->priv->marker_info[i].label, 
                                   marker->priv->marker_info[i].text, 
                                   -1);
         }

         if (marker->priv->type == GTK_DATABOX_MARKER_SOLID_LINE
               || marker->priv->type == GTK_DATABOX_MARKER_DASHED_LINE)
         {
            gint width;
            gint height;
            pango_layout_get_pixel_size (marker->priv->marker_info[i].label, 
                                         &width, &height);

            width = (width + 1) / 2 + 2;
            height = (height + 1) / 2 + 2;
            size = 0;

            switch (marker->priv->marker_info[i].position)
            {
               case GTK_DATABOX_MARKER_C:
               break;
               case GTK_DATABOX_MARKER_N:
                  coord.y = height;
               break;
               case GTK_DATABOX_MARKER_E:
                  coord.x = canvas->width - width;
               break;
               case GTK_DATABOX_MARKER_S:
                  coord.y = canvas->height - height;
               break;
               case GTK_DATABOX_MARKER_W:
                  coord.x = width;
               break;
            }
         }
         
         gtk_databox_label_write_at (canvas->pixmap, 
                                    marker->priv->marker_info[i].label, 
                                    marker->priv->label_gc, coord,
                                    marker->priv->marker_info[i].label_position,
                                    (size + 1) / 2 + 2,
                                    marker->priv->marker_info[i].boxed);
      }
   }
      
   return;
}

void
gtk_databox_marker_set_position (GtkDataboxMarker *marker, 
                                 guint index, 
                                 GtkDataboxMarkerPosition position)
{
   gint len;
   
   g_return_if_fail (GTK_DATABOX_IS_MARKER (marker));
   len = gtk_databox_xyc_graph_get_length (GTK_DATABOX_XYC_GRAPH (marker));
   g_return_if_fail (index < len);

   marker->priv->marker_info[index].position = position;
}
   
void
gtk_databox_marker_set_label (GtkDataboxMarker *marker,
                              guint index, 
                              GtkDataboxTextPosition label_position,
                              gchar *text,
                              gboolean boxed)
{
   gint len;
   
   g_return_if_fail (GTK_DATABOX_IS_MARKER (marker));
   len = gtk_databox_xyc_graph_get_length (GTK_DATABOX_XYC_GRAPH (marker));
   g_return_if_fail (index < len);

   marker->priv->marker_info[index].label_position = label_position;
   marker->priv->marker_info[index].text = g_strdup (text);
   marker->priv->marker_info[index].boxed = boxed;

   if (marker->priv->marker_info[index].label)
   {
      pango_layout_set_text (marker->priv->marker_info[index].label, 
                             marker->priv->marker_info[index].text, 
                             -1);
   }
}


