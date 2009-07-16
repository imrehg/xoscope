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

#include <gtkdatabox_graph.h>
#include <gtk/gtkgc.h>

static void gtk_databox_graph_real_draw (GtkDataboxGraph *graph, 
                                  GtkDataboxCanvas *canvas);
static gint gtk_databox_graph_real_calculate_extrema (GtkDataboxGraph *graph,
                                               GtkDataboxValue *min,
                                               GtkDataboxValue *max);
static void gtk_databox_graph_real_create_gc (GtkDataboxGraph *graph,
                                       GtkDataboxCanvas *canvas);

/* IDs of properties */
enum
{
   GRAPH_COLOR = 1,
   GRAPH_SIZE,
   GRAPH_HIDE
};

struct _GtkDataboxGraphPrivate
{
   GdkColor color;
   gint size;
   gboolean hide;
};

static gpointer parent_class = NULL;

static void
gtk_databox_graph_set_property (GObject      *object,
                    guint         property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
   GtkDataboxGraph *graph = GTK_DATABOX_GRAPH (object);

   switch (property_id) 
   {
      case GRAPH_COLOR: 
         {
            gtk_databox_graph_set_color (graph, (GdkColor *) g_value_get_pointer (value));
         }
         break;
      case GRAPH_SIZE: 
         {
            gtk_databox_graph_set_size (graph, g_value_get_int (value));
         }
         break;
      case GRAPH_HIDE: 
         {
            gtk_databox_graph_set_hide (graph, g_value_get_boolean (value));
         }
         break;
      default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
   }
}

static void
gtk_databox_graph_get_property (GObject      *object,
                    guint         property_id,
                    GValue       *value,
                    GParamSpec   *pspec)
{
   GtkDataboxGraph *graph = GTK_DATABOX_GRAPH (object);

   switch (property_id) 
   {
      case GRAPH_COLOR: 
         {
            g_value_set_pointer (value, gtk_databox_graph_get_color (graph));
         }
         break;
      case GRAPH_SIZE:
         {
            g_value_set_int (value, gtk_databox_graph_get_size (graph));
         }
         break;
      case GRAPH_HIDE:
         {
            g_value_set_boolean (value, gtk_databox_graph_get_hide (graph));
         }
         break;
      default:
         /* We don't have any other property... */
         G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
         break;
   }
}

static void
gtk_databox_graph_delete_gc (GtkDataboxGraph *graph)
{
   if (graph->gc)
   {
      GdkColormap *colormap = NULL;

      colormap = gdk_gc_get_colormap (graph->gc);
      gdk_colormap_free_colors (colormap, &graph->priv->color, 1);
      gtk_gc_release (graph->gc);
      graph->gc = NULL;
   }
}  

void
gtk_databox_graph_create_gc (GtkDataboxGraph *graph,
                             GtkDataboxCanvas *canvas)
{
   GTK_DATABOX_GRAPH_GET_CLASS (graph)->create_gc (graph, canvas);
}

static void
gtk_databox_graph_real_create_gc (GtkDataboxGraph *graph,
                                  GtkDataboxCanvas *canvas)
{
   GdkGCValues values;
   GdkGCValuesMask valuesMask;
   GdkColormap *colormap = NULL;

   g_return_if_fail (GTK_DATABOX_IS_GRAPH (graph));

   if (graph->gc)
      gtk_databox_graph_delete_gc (graph);

   colormap = canvas->style->colormap;
   g_return_if_fail (colormap);
   g_return_if_fail (gdk_colormap_alloc_color (colormap,
                                               &graph->priv->color,
                                               FALSE, TRUE));

   valuesMask = GDK_GC_FOREGROUND | GDK_GC_BACKGROUND
                | GDK_GC_FUNCTION
                | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE
                | GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE;

   values.foreground = graph->priv->color;
   values.background = canvas->style->black;
   values.function = GDK_COPY;
   /* I am not sure, why line_width==1 is so much slower than 0, but 
    * it is (at least for my machine with gtk+-2.4) */
   values.line_width = (graph->priv->size > 1) ? graph->priv->size : 0;
   values.line_style = GDK_LINE_SOLID;
   values.cap_style = GDK_CAP_BUTT;
   values.join_style = GDK_JOIN_MITER;

   graph->gc = gtk_gc_get (canvas->style->depth, 
                           canvas->style->colormap, 
                           &values, valuesMask);
}

static void graph_finalize (GObject *object) 
{
   GtkDataboxGraph *graph = GTK_DATABOX_GRAPH (object);

   gtk_databox_graph_delete_gc (graph);
   g_free (graph->priv);

   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_databox_graph_class_init (gpointer g_class,
                              gpointer g_class_data)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
   GtkDataboxGraphClass *klass = GTK_DATABOX_GRAPH_CLASS (g_class);
   GParamSpec *graph_param_spec;

   parent_class = g_type_class_peek_parent (klass);
   
   gobject_class->set_property = gtk_databox_graph_set_property;
   gobject_class->get_property = gtk_databox_graph_get_property;
   gobject_class->finalize     = graph_finalize;

   graph_param_spec = g_param_spec_pointer ("color",
                                           "Graph color",
                                           "Color of graph",
                                           G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    GRAPH_COLOR,
                                    graph_param_spec);
  
   graph_param_spec = g_param_spec_int ("size",
                                           "Graph size",
                                           "Size of displayed items",
                                           G_MININT,
                                           G_MAXINT,
                                           0, /* default value */
                                           G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    GRAPH_SIZE,
                                    graph_param_spec);
   
   graph_param_spec = g_param_spec_boolean ("hide",
                                           "Graph hidden",
                                           "Determine if graph is hidden or not",
                                           FALSE, /* default value */
                                           G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    GRAPH_SIZE,
                                    graph_param_spec);
  
   klass->draw = gtk_databox_graph_real_draw;
   klass->calculate_extrema = gtk_databox_graph_real_calculate_extrema;  
   klass->create_gc = gtk_databox_graph_real_create_gc;  
}

static void
gtk_databox_graph_instance_init (GTypeInstance   *instance,
                                 gpointer         g_class)
{
   GtkDataboxGraph *graph = GTK_DATABOX_GRAPH (instance);

   graph->priv = g_new0 (GtkDataboxGraphPrivate, 1);
}

GType
gtk_databox_graph_get_type (void)
{
   static GType type = 0;
   
   if (type == 0) {
      static const GTypeInfo info = 
      {
         sizeof (GtkDataboxGraphClass),
         NULL,                                  /* base_init */
         NULL,                                  /* base_finalize */
         gtk_databox_graph_class_init,          /* class_init */
         NULL,                                  /* class_finalize */
         NULL,                                  /* class_data */
         sizeof (GtkDataboxGraph),
         0,                                     /* n_preallocs */
         gtk_databox_graph_instance_init        /* instance_init */
      };
      type = g_type_register_static (G_TYPE_OBJECT,
                                     "GtkDataboxGraph",
                                     &info, 0);
   }
   
   return type;
}

GObject *
gtk_databox_graph_new (void)
{
  GtkDataboxGraph *graph;

  graph = g_object_new (GTK_DATABOX_TYPE_GRAPH, NULL);

  return G_OBJECT (graph);


}

void 
gtk_databox_graph_draw (GtkDataboxGraph *graph,
                             GtkDataboxCanvas *canvas)
{
   if (!graph->priv->hide)
      GTK_DATABOX_GRAPH_GET_CLASS (graph)->draw (graph, canvas);
}

gint 
gtk_databox_graph_calculate_extrema (GtkDataboxGraph *graph,
                                     GtkDataboxValue *min,
                                     GtkDataboxValue *max)
{
   return 
      GTK_DATABOX_GRAPH_GET_CLASS (graph)->calculate_extrema (graph, min, max);
}
   
static void
gtk_databox_graph_real_draw (GtkDataboxGraph *graph,
                             GtkDataboxCanvas *canvas)
{
   /* We have no data... */
   return;
}


static gint 
gtk_databox_graph_real_calculate_extrema (GtkDataboxGraph *graph,
                                     GtkDataboxValue *min,
                                     GtkDataboxValue *max)
{
   /* We have no data... */
   return -1;
}

void
gtk_databox_graph_set_color (GtkDataboxGraph *graph, GdkColor *color)
{
   GdkColormap *colormap = NULL;

   g_return_if_fail (GTK_DATABOX_IS_GRAPH (graph));

   if (graph->gc)
   {
      colormap = gdk_gc_get_colormap (graph->gc);
      gdk_colormap_free_colors (colormap, &graph->priv->color, 1);
      gdk_colormap_alloc_color (colormap, color, FALSE, TRUE);
      gdk_gc_set_foreground (graph->gc, color);
   }

   graph->priv->color = *color;

   g_object_notify (G_OBJECT (graph), "color");
}

GdkColor *
gtk_databox_graph_get_color (GtkDataboxGraph *graph)
{
   return &graph->priv->color;
}
      
void 
gtk_databox_graph_set_size (GtkDataboxGraph *graph, gint size)
{
   GdkGCValues values;

   g_return_if_fail (GTK_DATABOX_IS_GRAPH (graph));

   graph->priv->size = MAX (1, size);;

   if (graph->gc)
   {
      values.line_width = graph->priv->size;
      gdk_gc_set_values (graph->gc, &values, GDK_GC_FOREGROUND);
   }

   g_object_notify (G_OBJECT (graph), "size");
}

gint 
gtk_databox_graph_get_size (GtkDataboxGraph *graph)
{
   g_return_val_if_fail (GTK_DATABOX_IS_GRAPH (graph), -1);

   return graph->priv->size;
}

void 
gtk_databox_graph_set_hide (GtkDataboxGraph *graph, gboolean hide)
{
   g_return_if_fail (GTK_DATABOX_IS_GRAPH (graph));

   graph->priv->hide = hide;

   g_object_notify (G_OBJECT (graph), "hide");
}

gboolean 
gtk_databox_graph_get_hide (GtkDataboxGraph *graph)
{
   g_return_val_if_fail (GTK_DATABOX_IS_GRAPH (graph), -1);

   return graph->priv->hide;
}

