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

#include <gtk/gtktable.h>
#include <gtkdatabox.h>
#include <gtkdatabox_marshal.h>
#include <gtk/gtkgc.h>

static gint gtk_databox_configure        (GtkWidget *widget, 
                                          GdkEventConfigure *event);
static gint gtk_databox_expose           (GtkWidget *widget, 
                                          GdkEventExpose *event);
static gint gtk_databox_button_press     (GtkWidget *widget, 
                                          GdkEventButton *event);
static gint gtk_databox_button_release   (GtkWidget *widget, 
                                          GdkEventButton *event);
static gint gtk_databox_motion_notify    (GtkWidget *widget, 
                                          GdkEventMotion *event);

static void gtk_databox_draw (GtkDatabox *box, GdkEventExpose *event);

static void gtk_databox_selection_cancel (GtkDatabox *box);
/*
static void gtk_databox_zoom_to_selection (GtkDatabox * box);
static void gtk_databox_zoom_out (GtkDatabox *box);
static void gtk_databox_zoom_home (GtkDatabox *box);
*/
static void gtk_databox_draw_selection (GtkDatabox *box, 
                                        GdkRectangle * pixmapCopyRect);
static void gtk_databox_hadjustment_changed (GtkAdjustment *adjX, 
                                             GtkDatabox *box);
static void gtk_databox_hadjustment_value_changed (GtkAdjustment *adjX, 
                                                   GtkDatabox *box);
static void gtk_databox_vadjustment_changed (GtkAdjustment *adjY, 
                                             GtkDatabox *box);
static void gtk_databox_vadjustment_value_changed (GtkAdjustment *adjY, 
                                                   GtkDatabox *box);
static void gtk_databox_hruler_update (GtkDatabox *box);
static void gtk_databox_vruler_update (GtkDatabox *box);

/* IDs of signals */
enum
{
   ZOOMED_SIGNAL,
   SELECTION_STARTED_SIGNAL,
   SELECTION_CHANGED_SIGNAL,
   SELECTION_FINALIZED_SIGNAL,
   SELECTION_CANCELLED_SIGNAL,
   LAST_SIGNAL
};

/* signals will be configured during class_init */
static gint gtk_databox_signals[LAST_SIGNAL] = { 0 };


/* IDs of properties */
enum
{
   ENABLE_SELECTION = 1,
   ENABLE_ZOOM,
   H_ADJUSTMENT,
   V_ADJUSTMENT,
   H_RULER,
   V_RULER
};

struct _GtkDataboxPrivate
{
   GtkDataboxCanvas canvas;

   /* Properties */
   gboolean enable_selection;
   gboolean enable_zoom;
   GtkAdjustment *adjX;
   GtkAdjustment *adjY;
   GtkRuler *rulerX;
   GtkRuler *rulerY;

   /* Other private stuff */
   GList *graphs;
   GdkGC *select_gc;
   GtkDataboxCoord marked;
   GtkDataboxCoord select;
   gfloat zoom_limit;

   /* flags */
   gboolean selection_active;
   gboolean selection_finalized;
};

static gpointer parent_class = NULL;

void
gtk_databox_set_enable_selection (GtkDatabox *box, gboolean enable)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   box->priv->enable_selection = enable;
   if (box->priv->selection_active)
   {
      gtk_databox_selection_cancel (box);
   }
   
   g_object_notify (G_OBJECT (box), "enable-selection");
}

void
gtk_databox_set_enable_zoom (GtkDatabox *box, gboolean enable)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   box->priv->enable_zoom = enable;
   
   g_object_notify (G_OBJECT (box), "enable-zoom");
}

void
gtk_databox_set_hadjustment (GtkDatabox *box, GtkAdjustment *adj)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   if (adj)
      g_return_if_fail (GTK_IS_ADJUSTMENT (adj));
   else
      adj = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

   if (box->priv->adjX)
   {
      adj->lower = box->priv->adjX->lower;
      adj->upper = box->priv->adjX->upper;
      adj->page_size = box->priv->adjX->page_size;
      adj->value = box->priv->adjX->value;
      adj->step_increment = box->priv->adjX->step_increment;
      adj->page_increment = box->priv->adjX->page_increment;

      /* @@@ Do we need to disconnect from the signals here? */
      g_object_unref (box->priv->adjX);
   } else
   {
      adj->lower = 0;
      adj->upper = 1.0;
      adj->page_size = 1.0;
      adj->value = 0;
      adj->step_increment = 0.05;
      adj->page_increment = 0.1;
   }
   box->priv->adjX = adj;
   g_object_ref (box->priv->adjX);
   
   gtk_adjustment_changed (box->priv->adjX);
   gtk_adjustment_value_changed (box->priv->adjX);
   
   g_signal_connect (G_OBJECT (box->priv->adjX), "changed",
                     G_CALLBACK (gtk_databox_hadjustment_changed), box);
   g_signal_connect (G_OBJECT (box->priv->adjX), "value_changed",
                     G_CALLBACK (gtk_databox_hadjustment_value_changed), box);

   g_object_notify (G_OBJECT (box), "hadjustment");
}

void
gtk_databox_set_vadjustment (GtkDatabox *box, GtkAdjustment *adj)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   if (adj)
      g_return_if_fail (GTK_IS_ADJUSTMENT (adj));
   else
      adj = (GtkAdjustment*) g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

   if (box->priv->adjY)
   {
      adj->lower = box->priv->adjY->lower;
      adj->upper = box->priv->adjY->upper;
      adj->page_size = box->priv->adjY->page_size;
      adj->value = box->priv->adjY->value;
      adj->step_increment = box->priv->adjY->step_increment;
      adj->page_increment = box->priv->adjY->page_increment;

      /* @@@  Do we need to disconnect from the signals here? */
      g_object_unref (box->priv->adjY);
   } else
   {
      adj->lower = 0;
      adj->upper = 1.0;
      adj->page_size = 1.0;
      adj->value = 0;
      adj->step_increment = 0.05;
      adj->page_increment = 0.1;
   }
   box->priv->adjY = adj;
   g_object_ref (box->priv->adjY);
   
   gtk_adjustment_changed (box->priv->adjY);
   gtk_adjustment_value_changed (box->priv->adjY);

   g_signal_connect (G_OBJECT (box->priv->adjY), "changed",
                     G_CALLBACK (gtk_databox_vadjustment_changed), box);
   g_signal_connect (G_OBJECT (box->priv->adjY), "value_changed",
                     G_CALLBACK (gtk_databox_vadjustment_value_changed), box);
   
   g_object_notify (G_OBJECT (box), "vadjustment");
}

void
gtk_databox_set_hruler (GtkDatabox *box, GtkRuler *ruler)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   if (box->priv->rulerX)
   {
      /* @@@ Do we need to disconnect the signals here? */
   }

   if (GTK_IS_RULER (ruler))
   {
      box->priv->rulerX = ruler;
   
      gtk_ruler_set_metric (GTK_RULER (box->priv->rulerX), GTK_PIXELS);
      gtk_databox_hruler_update (box);
      g_signal_connect_swapped (box, "motion_notify_event",
            G_CALLBACK (GTK_WIDGET_GET_CLASS (box->priv->rulerX)->motion_notify_event),
            G_OBJECT (box->priv->rulerX));
   }
   else if (!ruler)
   {
      box->priv->rulerX = NULL;
   }
   else
   {
      g_warning ("ruler is neither a GtkRuler nor NULL!");
      box->priv->rulerX = NULL;
   }
   
   g_object_notify (G_OBJECT (box), "hruler");
}

void
gtk_databox_set_vruler (GtkDatabox *box, GtkRuler *ruler)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   if (box->priv->rulerY)
   {
      /* @@@ Do we need to disconnect the signals here? */
   }

   if (GTK_IS_RULER (ruler))
   {
      box->priv->rulerY = ruler;
   
      gtk_ruler_set_metric (GTK_RULER (box->priv->rulerY), GTK_PIXELS);
      gtk_databox_vruler_update (box);
      g_signal_connect_swapped (box, "motion_notify_event",
            G_CALLBACK (GTK_WIDGET_GET_CLASS (box->priv->rulerY)->motion_notify_event),
            G_OBJECT (box->priv->rulerY));
   }
   else if (!ruler)
   {
      box->priv->rulerY = NULL;
   }
   else
   {
      g_warning ("ruler is neither a GtkRuler nor NULL!");
      box->priv->rulerY = NULL;
   }
   
   g_object_notify (G_OBJECT (box), "vruler");
}

static void
gtk_databox_set_property (GObject      *object,
                    guint         property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
   GtkDatabox *box = GTK_DATABOX (object);

   switch (property_id) 
   {
      case ENABLE_SELECTION: 
         gtk_databox_set_enable_selection (box, g_value_get_boolean (value));
         break;
      case ENABLE_ZOOM: 
         gtk_databox_set_enable_zoom (box, g_value_get_boolean (value));
         break;
      case H_ADJUSTMENT: 
         gtk_databox_set_hadjustment (box, g_value_get_object (value));
         break;
      case V_ADJUSTMENT: 
         gtk_databox_set_vadjustment (box, g_value_get_object (value));
         break;
      case H_RULER: 
         gtk_databox_set_hruler (box, g_value_get_object (value));
         break;
      case V_RULER: 
         gtk_databox_set_vruler (box, g_value_get_object (value));
         break;
      default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
   }
}

gboolean
gtk_databox_get_enable_selection (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), FALSE);

   return box->priv->enable_selection;
}

gboolean
gtk_databox_get_enable_zoom (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), FALSE);

   return box->priv->enable_zoom;
}

GtkAdjustment *
gtk_databox_get_hadjustment (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), NULL);

   return box->priv->adjX;
}

GtkAdjustment *
gtk_databox_get_vadjustment (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), NULL);

   return box->priv->adjY;
}

GtkRuler *
gtk_databox_get_hruler (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), NULL);

   return box->priv->rulerX;
}

GtkRuler *
gtk_databox_get_vruler (GtkDatabox *box)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), NULL);

   return box->priv->rulerY;
}

static void
gtk_databox_get_property (GObject      *object,
                    guint         property_id,
                    GValue       *value,
                    GParamSpec   *pspec)
{
   GtkDatabox *box = GTK_DATABOX (object);

   switch (property_id) 
   {
      case ENABLE_SELECTION: 
         {
            g_value_set_boolean (value, gtk_databox_get_enable_selection (box));
         }
         break;
      case ENABLE_ZOOM: 
         {
            g_value_set_boolean (value, gtk_databox_get_enable_zoom (box));
         }
         break;
      case H_ADJUSTMENT: 
         {
            g_value_set_object (value, 
                                G_OBJECT( gtk_databox_get_hadjustment (box)));
         }
         break;
      case V_ADJUSTMENT: 
         {
            g_value_set_object (value, 
                                G_OBJECT( gtk_databox_get_vadjustment (box)));
         }
         break;
      case H_RULER: 
         {
            g_value_set_object (value, 
                                G_OBJECT( gtk_databox_get_hruler (box)));
         }
         break;
      case V_RULER: 
         {
            g_value_set_object (value, 
                                G_OBJECT( gtk_databox_get_vruler (box)));
         }
         break;
      default:
         /* We don't have any other property... */
         G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
         break;
   }
}

static void databox_finalize (GObject *object) 
{
   GtkDatabox *box = GTK_DATABOX (object);

   if (box->priv->canvas.pixmap)
      g_object_unref (box->priv->canvas.pixmap);
   if (box->priv->select_gc)
      gtk_gc_release (box->priv->select_gc);
   if (box->priv->adjX)
      g_object_unref (box->priv->adjX);
   if (box->priv->adjY)
      g_object_unref (box->priv->adjY);
   if (box->priv->graphs)
      g_list_free (box->priv->graphs);

   g_free (box->priv);

   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_databox_class_init (gpointer g_class,
                              gpointer g_class_data)
{
   GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
   GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (g_class);
   GtkDataboxClass *klass = GTK_DATABOX_CLASS (g_class);
   GParamSpec *databox_param_spec;

   parent_class = g_type_class_peek_parent (klass);
   
   gobject_class->set_property = gtk_databox_set_property;
   gobject_class->get_property = gtk_databox_get_property;
   gobject_class->finalize     = databox_finalize;

   databox_param_spec = g_param_spec_boolean ("enable-selection",
                                              "Enable Selection",
                                              "Enable selection of areas via mous (TRUE/FALSE)",
                                              TRUE, /* default value */
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    ENABLE_SELECTION,
                                    databox_param_spec);

   databox_param_spec = g_param_spec_boolean ("enable-zoom",
                                              "Enable Zoom",
                                              "Enable zooming in or out via mouse click (TRUE/FALSE)",
                                              TRUE, /* default value */
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    ENABLE_ZOOM,
                                    databox_param_spec);

   databox_param_spec = g_param_spec_object ("hadjustment",
                                              "Horizontal Adjustment",
                                              "GtkAdjustment for horizontal scrolling",
                                              GTK_TYPE_ADJUSTMENT,  
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    H_ADJUSTMENT,
                                    databox_param_spec);

   databox_param_spec = g_param_spec_object ("vadjustment",
                                              "Vertical Adjustment",
                                              "GtkAdjustment for vertical scrolling",
                                              GTK_TYPE_ADJUSTMENT,
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    V_ADJUSTMENT,
                                    databox_param_spec);

   databox_param_spec = g_param_spec_object ("hruler",
                                              "Horizontal Ruler",
                                              "GtkHRuler, if defined",
                                              GTK_TYPE_RULER,  
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    H_RULER,
                                    databox_param_spec);

   databox_param_spec = g_param_spec_object ("vruler",
                                              "Vertical Ruler",
                                              "GtkVRuler, if defined",
                                              GTK_TYPE_RULER,
                                              G_PARAM_CONSTRUCT
                                              | G_PARAM_READWRITE);

   g_object_class_install_property (gobject_class,
                                    V_RULER,
                                    databox_param_spec);


   widget_class->configure_event = gtk_databox_configure;
   widget_class->expose_event = gtk_databox_expose;
   widget_class->button_press_event = gtk_databox_button_press;
   widget_class->button_release_event = gtk_databox_button_release;
   widget_class->motion_notify_event = gtk_databox_motion_notify;

   /* Install the signals of GtkDatabox */
   gtk_databox_signals[ZOOMED_SIGNAL] 
      = g_signal_new ("databox_zoomed", 
                      G_TYPE_FROM_CLASS (gobject_class), 
                      G_SIGNAL_RUN_FIRST, 
                      G_STRUCT_OFFSET (GtkDataboxClass, 
                                       zoomed), 
                      NULL,        /* accumulator */
                      NULL,        /* accumulator_data */
                      gtk_databox_marshal_VOID__POINTER_POINTER,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_POINTER,
                      G_TYPE_POINTER);
   gtk_databox_signals[SELECTION_STARTED_SIGNAL] 
      = g_signal_new ("databox_selection_started", 
                      G_TYPE_FROM_CLASS (gobject_class), 
                      G_SIGNAL_RUN_FIRST, 
                      G_STRUCT_OFFSET (GtkDataboxClass, 
                                       selection_started), 
                      NULL,        /* accumulator */
                      NULL,        /* accumulator_data */
                      gtk_databox_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);
   gtk_databox_signals[SELECTION_CHANGED_SIGNAL] 
      = g_signal_new ("databox_selection_changed", 
                      G_TYPE_FROM_CLASS (gobject_class), 
                      G_SIGNAL_RUN_FIRST, 
                      G_STRUCT_OFFSET (GtkDataboxClass, 
                                       selection_changed), 
                      NULL,        /* accumulator */
                      NULL,        /* accumulator_data */
                      gtk_databox_marshal_VOID__POINTER_POINTER,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_POINTER,
                      G_TYPE_POINTER);
   gtk_databox_signals[SELECTION_FINALIZED_SIGNAL] 
      = g_signal_new ("databox_selection_stopped", 
                      G_TYPE_FROM_CLASS (gobject_class), 
                      G_SIGNAL_RUN_FIRST, 
                      G_STRUCT_OFFSET (GtkDataboxClass, 
                                       selection_finalized), 
                      NULL,        /* accumulator */
                      NULL,        /* accumulator_data */
                      gtk_databox_marshal_VOID__POINTER_POINTER,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_POINTER,
                      G_TYPE_POINTER);
   gtk_databox_signals[SELECTION_CANCELLED_SIGNAL] 
      = g_signal_new ("databox_selection_cancelled", 
                      G_TYPE_FROM_CLASS (gobject_class), 
                      G_SIGNAL_RUN_FIRST, 
                      G_STRUCT_OFFSET (GtkDataboxClass, 
                                       selection_cancelled), 
                      NULL,        /* accumulator */
                      NULL,        /* accumulator_data */
                      gtk_databox_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
   
   klass->zoomed = NULL;
   klass->selection_started = NULL;
   klass->selection_changed = NULL;
   klass->selection_finalized = NULL;
   klass->selection_cancelled = NULL;
}

static void
gtk_databox_instance_init (GTypeInstance   *instance,
                                 gpointer         g_class)
{
   GtkWidget *widget = GTK_WIDGET (instance);
   GtkDatabox *box = GTK_DATABOX (instance);

   box->priv = g_new0 (GtkDataboxPrivate, 1);

   gtk_widget_set_events (widget,
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_POINTER_MOTION_HINT_MASK);
   
   box->priv->canvas.top_left_total.x = -1.;
   box->priv->canvas.top_left_total.y = 1.;
   box->priv->canvas.top_left_visible.x = -1.;
   box->priv->canvas.top_left_visible.y = 1.;
   box->priv->canvas.bottom_right_total.x = 1.;
   box->priv->canvas.bottom_right_total.y = -1.;
   box->priv->canvas.bottom_right_visible.x = 1.;
   box->priv->canvas.bottom_right_visible.y = -1.;
   box->priv->zoom_limit = 0.01;
}

GType
gtk_databox_get_type (void)
{
   static GType type = 0;
   
   if (type == 0) {
      static const GTypeInfo info = 
      {
         sizeof (GtkDataboxClass),
         NULL,                                  /* base_init */
         NULL,                                  /* base_finalize */
         gtk_databox_class_init,                /* class_init */
         NULL,                                  /* class_finalize */
         NULL,                                  /* class_data */
         sizeof (GtkDatabox),
         0,                                     /* n_preallocs */
         gtk_databox_instance_init              /* instance_init */
      };
      type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
                                     "GtkDatabox",
                                     &info, 0);
   }
   
   return type;
}

GtkWidget *
gtk_databox_new (void)
{
   GtkWidget *databox;

   databox = gtk_widget_new (GTK_TYPE_DATABOX, NULL);

   return databox;
}

static void
gtk_databox_calculate_hcanvas (GtkDatabox *box)
{
   if (!GTK_WIDGET_VISIBLE (box))
      return;
   
   if (box->priv->adjX->page_size == 1.0)
   {
      box->priv->canvas.top_left_visible.x = 
         box->priv->canvas.top_left_total.x;
      box->priv->canvas.bottom_right_visible.x = 
         box->priv->canvas.bottom_right_total.x;
   }
   else
   {
      box->priv->canvas.top_left_visible.x = 
         box->priv->canvas.top_left_total.x 
         + (box->priv->canvas.bottom_right_total.x 
            - box->priv->canvas.top_left_total.x) 
         * box->priv->adjX->value;
      box->priv->canvas.bottom_right_visible.x =
         box->priv->canvas.top_left_visible.x 
         + (box->priv->canvas.bottom_right_total.x 
            - box->priv->canvas.top_left_total.x) 
         * box->priv->adjX->page_size;
   }

   box->priv->canvas.translation_factor.x = 
      box->priv->canvas.width 
      / (box->priv->canvas.bottom_right_visible.x 
         - box->priv->canvas.top_left_visible.x);

   gtk_databox_hruler_update (box);

   return;
}

static void
gtk_databox_calculate_vcanvas (GtkDatabox *box)
{
   if (!GTK_WIDGET_VISIBLE (box))
      return;
   
   if (box->priv->adjY->page_size == 1.0)
   {
      box->priv->canvas.top_left_visible.y = 
         box->priv->canvas.top_left_total.y;
      box->priv->canvas.bottom_right_visible.y = 
         box->priv->canvas.bottom_right_total.y;
   }
   else
   {
      box->priv->canvas.top_left_visible.y = 
         box->priv->canvas.top_left_total.y 
         + (box->priv->canvas.bottom_right_total.y 
            - box->priv->canvas.top_left_total.y) 
         * box->priv->adjY->value;
      box->priv->canvas.bottom_right_visible.y = 
         box->priv->canvas.top_left_visible.y 
         + (box->priv->canvas.bottom_right_total.y 
            - box->priv->canvas.top_left_total.y) 
         * box->priv->adjY->page_size;
   }

   box->priv->canvas.translation_factor.y = 
      box->priv->canvas.height
      / (box->priv->canvas.bottom_right_visible.y 
         - box->priv->canvas.top_left_visible.y);
   
   gtk_databox_vruler_update (box);

   return;
}

static gint
gtk_databox_configure (GtkWidget *widget, GdkEventConfigure *event)
{
   GtkDatabox *box = GTK_DATABOX (widget);
   gint width;
   gint height;

   box->priv->canvas.width = event->width;
   box->priv->canvas.height = event->height;

   if (box->priv->canvas.pixmap)
   {
      gdk_drawable_get_size (GDK_DRAWABLE (box->priv->canvas.pixmap), 
                             &width, &height);
      
      if (width <event->width || height < event->height)
      {
         g_object_unref (box->priv->canvas.pixmap);
         box->priv->canvas.pixmap = NULL;
      }
   }

   if (!box->priv->canvas.pixmap) 
   {
      box->priv->canvas.pixmap =
         gdk_pixmap_new (widget->window, event->width, event->height, -1);
   }

   if (box->priv->selection_active)
   {
      gtk_databox_selection_cancel (box);
   }

   box->priv->canvas.context = gtk_widget_get_pango_context (widget);
   gtk_databox_calculate_hcanvas (box);
   gtk_databox_calculate_vcanvas (box);

   return FALSE;
}

static gint
gtk_databox_expose (GtkWidget *widget, GdkEventExpose *event)
{
   GtkDatabox *box = GTK_DATABOX (widget);

   gtk_databox_draw (box, event);

   gdk_draw_drawable (widget->window,
                      widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                      box->priv->canvas.pixmap, event->area.x, event->area.y,
                      event->area.x, event->area.y, event->area.width,
                      event->area.height);

   return FALSE;
}

static gint 
gtk_databox_button_press     (GtkWidget *widget, 
                              GdkEventButton *event)
{
   GtkDatabox *box = GTK_DATABOX (widget);

   if (event->type != GDK_BUTTON_PRESS)
      return FALSE;

   box->priv->selection_finalized = FALSE;

   if ((event->button == 1 || event->button == 2))
   {
      if (box->priv->selection_active)
      {
         if (   event->x > MIN (box->priv->marked.x, box->priv->select.x)
             && event->x < MAX (box->priv->marked.x, box->priv->select.x)
             && event->y > MIN (box->priv->marked.y, box->priv->select.y)
             && event->y < MAX (box->priv->marked.y, box->priv->select.y))
         {
            gtk_databox_zoom_to_selection (box);
         }
         else
         {
            gtk_databox_selection_cancel (box);
         }
         box->priv->marked.x = event->x;
         box->priv->marked.y = event->y;
      }
   }

   if (event->button == 3)
   {
      if (event->state & GDK_SHIFT_MASK)
      {
         gtk_databox_zoom_home (box);
      }
      else
      {
         gtk_databox_zoom_out (box);
      }
   }

   return FALSE;
}

static gint 
gtk_databox_button_release   (GtkWidget *widget, 
                              GdkEventButton *event)
{
   GtkDatabox *box = GTK_DATABOX (widget);

   if (event->type != GDK_BUTTON_RELEASE)
      return FALSE;

   if (box->priv->selection_active)
   {
      box->priv->selection_finalized = TRUE;

    g_signal_emit (G_OBJECT (box),
                   gtk_databox_signals[SELECTION_FINALIZED_SIGNAL],
                   0, &box->priv->marked, &box->priv->select);
   }

   return FALSE;
}

static gint 
gtk_databox_motion_notify    (GtkWidget *widget, 
                              GdkEventMotion *event)
{
   GtkDatabox *box = GTK_DATABOX (widget);
   gint x = event->x;
   gint y = event->y;

   if (event->is_hint || (event->window != widget->window))
      gdk_window_get_pointer (widget->window, 
                              &x, &y, &event->state);

   if (event->state & GDK_BUTTON1_MASK
       && box->priv->enable_selection
       && !box->priv->selection_finalized)
   {
      GdkRectangle rect;
      gint width;
      gint height;

      gdk_drawable_get_size (widget->window, &width, &height);
      x = MAX (0, MIN (width - 1, x));
      y = MAX (0, MIN (height - 1, y));

      if (box->priv->selection_active)
      {
         /* Clear current selection from pixmap */
         gtk_databox_draw_selection (box, NULL);
      }
      else
      {
         box->priv->selection_active = TRUE;
         box->priv->marked.x = x;
         box->priv->marked.y = y;
         box->priv->select.x = x;
         box->priv->select.y = y;
         g_signal_emit (G_OBJECT (box),
                        gtk_databox_signals[SELECTION_STARTED_SIGNAL], 0,
                        &box->priv->marked);
      }

      /* Determine the exposure rectangle (covering old selection and new) */
      rect.x = MIN (MIN (box->priv->marked.x, box->priv->select.x), x);
      rect.y = MIN (MIN (box->priv->marked.y, box->priv->select.y), y);
      rect.width = MAX (MAX (box->priv->marked.x, box->priv->select.x), x) 
                   - rect.x + 1;
      rect.height = MAX (MAX (box->priv->marked.y, box->priv->select.y), y) 
                    - rect.y + 1;

      box->priv->select.x = x;
      box->priv->select.y = y;

      /* Draw new selection */
      gtk_databox_draw_selection (box, &rect);

      g_signal_emit (G_OBJECT (box),
                     gtk_databox_signals[SELECTION_CHANGED_SIGNAL],
                     0, &box->priv->marked, &box->priv->select);
   }

   return FALSE;
}

void
gtk_databox_redraw (GtkDatabox * box)
{
   GtkWidget *widget = GTK_WIDGET (box);

   g_return_if_fail (GTK_IS_DATABOX (box));

   gtk_widget_queue_draw_area (widget, 0, 0, 
                               box->priv->canvas.width, 
                               box->priv->canvas.height);
   
   return;
}

static void
gtk_databox_draw (GtkDatabox *box,
                  GdkEventExpose *event)
{
   GList *list;
   GtkWidget *widget = GTK_WIDGET (box);

   g_return_if_fail (GTK_IS_DATABOX (box));
   g_return_if_fail (GTK_WIDGET_VISIBLE (widget));

   gdk_draw_rectangle (box->priv->canvas.pixmap, widget->style->bg_gc[0], 
                       TRUE, 0, 0,
                       box->priv->canvas.width,
                       box->priv->canvas.height);
   
   list = g_list_last (box->priv->graphs);

   box->priv->canvas.style = widget->style;

   while (list)
   {
      if (list->data)
      {
         gtk_databox_graph_draw (GTK_DATABOX_GRAPH (list->data), 
                                 &box->priv->canvas);
      }
      else
      {
         /* Do nothing if data == NULL */
      }
      list = g_list_previous (list);
   }
   
   if (box->priv->selection_active)
   {
      gtk_databox_draw_selection (box, NULL);
   }

   return;
}




static void
gtk_databox_selection_cancel (GtkDatabox *box)
{
   GdkRectangle rect;

   /* There is no active selection after cancellation */
   box->priv->selection_active = FALSE;

   /* Only active selections can be stopped */
   box->priv->selection_finalized = FALSE;

   /* Remove selection box */
   rect.x = MIN (box->priv->marked.x, box->priv->select.x);
   rect.y = MIN (box->priv->marked.y, box->priv->select.y);
   rect.width = ABS (box->priv->marked.x - box->priv->select.x) + 1;
   rect.height = ABS (box->priv->marked.y - box->priv->select.y) + 1;

   gtk_databox_draw_selection (box, &rect);

   /* Let everyone know that the selection has been cancelled */
   g_signal_emit (G_OBJECT (box),
                  gtk_databox_signals[SELECTION_CANCELLED_SIGNAL], 0);
}


static void
gtk_databox_zoomed (GtkDatabox *box)
{
   box->priv->selection_active = FALSE;
   box->priv->selection_finalized = FALSE;
   
   /* The other zoom functions set the crucial values, we set the 
    * basics here 
    */

   /* We always scroll from 0 to 1.0 */
   box->priv->adjX->lower = 0;
   box->priv->adjX->upper = 1.0;
   box->priv->adjY->step_increment = box->priv->adjY->page_size / 20;
   box->priv->adjY->page_increment = box->priv->adjY->page_size * 0.9;
   box->priv->adjX->step_increment = box->priv->adjX->page_size / 20;
   box->priv->adjX->page_increment = box->priv->adjX->page_size * 0.9;

   gtk_adjustment_changed (box->priv->adjX);
   gtk_adjustment_changed (box->priv->adjY);
   gtk_adjustment_value_changed (box->priv->adjX);
   gtk_adjustment_value_changed (box->priv->adjY);

   g_signal_emit (G_OBJECT (box),
                  gtk_databox_signals[ZOOMED_SIGNAL], 0,
                  &box->priv->canvas.top_left_visible, 
                  &box->priv->canvas.bottom_right_visible);
}

void
gtk_databox_zoom_to_selection (GtkDatabox * box)
{
   if (!box->priv->enable_zoom)
   {
      gtk_databox_selection_cancel (box);
      return;
   }

   box->priv->adjX->value += (gfloat) (MIN (box->priv->marked.x, 
                                            box->priv->select.x)) 
                                    * box->priv->adjX->page_size 
                                    / box->priv->canvas.width;
   box->priv->adjY->value += (gfloat) (MIN (box->priv->marked.y, 
                                            box->priv->select.y)) 
                                    * box->priv->adjY->page_size 
                                    / box->priv->canvas.height;

   box->priv->adjX->page_size *=
      (gfloat) (ABS (box->priv->marked.x - box->priv->select.x) + 1) 
               / box->priv->canvas.width;

   box->priv->adjY->page_size *=
      (gfloat) (ABS (box->priv->marked.y - box->priv->select.y) + 1) 
               / box->priv->canvas.height;

   /* If we zoom too far into the data, we will get funny results, because
    * of overflow effects. Therefore zooming is limited to box->zoom_limit.
    */
   if (box->priv->adjX->page_size < box->priv->zoom_limit)
   {
      box->priv->adjX->value = (gfloat) MAX (0,
                                       box->priv->adjX->value
                                       - (box->priv->zoom_limit -
                                          box->priv->adjX->page_size) / 2.0);
      box->priv->adjX->page_size = box->priv->zoom_limit;
   }

   if (box->priv->adjY->page_size < box->priv->zoom_limit)
   {
      box->priv->adjY->value = (gfloat) MAX (0,
                                       box->priv->adjY->value
                                       - (box->priv->zoom_limit -
                                          box->priv->adjY->page_size) / 2.0);
      box->priv->adjY->page_size = box->priv->zoom_limit;
   }

   gtk_databox_zoomed (box);
}

void
gtk_databox_zoom_out (GtkDatabox * box)
{
   if (!box->priv->enable_zoom)
   {
      return;
   }

   box->priv->adjX->page_size = MIN (1.0, box->priv->adjX->page_size * 2);
   box->priv->adjY->page_size = MIN (1.0, box->priv->adjY->page_size * 2);
   box->priv->adjX->value =
      (box->priv->adjX->page_size == 1.0) 
      ? 0 
      : MAX (0, MIN (box->priv->adjX->value - box->priv->adjX->page_size / 4,
                      1.0 - box->priv->adjX->page_size));
   box->priv->adjY->value =
      (box->priv->adjY->page_size == 1.0) 
      ? 0 
      : MAX (0, MIN (box->priv->adjY->value - box->priv->adjY->page_size / 4,
                      1.0 - box->priv->adjY->page_size));

   gtk_databox_zoomed (box);
}

void
gtk_databox_zoom_home (GtkDatabox * box)
{
   if (!box->priv->enable_zoom)
   {
      return;
   }

   box->priv->adjX->value = 0;
   box->priv->adjY->value = 0;
   box->priv->adjX->page_size = 1.0;
   box->priv->adjY->page_size = 1.0;

   gtk_databox_zoomed (box);
}

static void
gtk_databox_draw_selection (GtkDatabox *box,
                            GdkRectangle *pixmapCopyRect)
{
   GtkWidget *widget = GTK_WIDGET (box);

   if (!box->priv->select_gc)
   {
      GdkGCValues values;

      values.foreground = widget->style->white;
      values.function = GDK_XOR;
      box->priv->select_gc = gtk_gc_get (widget->style->depth,
                                         widget->style->colormap,
                                         &values,
                                         GDK_GC_FUNCTION | GDK_GC_FOREGROUND);
   }


   /* Draw a selection box in XOR mode onto the buffer pixmap */
   gdk_draw_rectangle (box->priv->canvas.pixmap, box->priv->select_gc,
                       FALSE,
                       MIN (box->priv->marked.x, box->priv->select.x),
                       MIN (box->priv->marked.y, box->priv->select.y),
                       ABS (box->priv->marked.x - box->priv->select.x),
                       ABS (box->priv->marked.y - box->priv->select.y));

   /* Copy a part of the pixmap to the screen */
   if (pixmapCopyRect)
      gdk_draw_drawable (widget->window,
                         widget->style->fg_gc[GTK_WIDGET_STATE (box)],
                         box->priv->canvas.pixmap,
                         pixmapCopyRect->x,
                         pixmapCopyRect->y,
                         pixmapCopyRect->x,
                         pixmapCopyRect->y,
                         pixmapCopyRect->width, pixmapCopyRect->height);
}

static void
gtk_databox_hadjustment_value_changed (GtkAdjustment *adjX, GtkDatabox *box)
{
   gtk_databox_calculate_hcanvas (box);
   
   gtk_databox_redraw (box);
}

static void
gtk_databox_vadjustment_value_changed (GtkAdjustment *adjY, GtkDatabox *box)
{
   gtk_databox_calculate_vcanvas (box);
   
   gtk_databox_redraw (box);
}

static void
gtk_databox_hadjustment_changed (GtkAdjustment *adjX, GtkDatabox *box)
{
   gtk_databox_calculate_hcanvas (box);
}

static void
gtk_databox_vadjustment_changed (GtkAdjustment *adjY, GtkDatabox *box)
{
   gtk_databox_calculate_vcanvas (box);
}

static void
gtk_databox_hruler_update (GtkDatabox *box)
{
   if (box->priv->rulerX)
   {
      gtk_ruler_set_range (GTK_RULER (box->priv->rulerX), 
                           box->priv->canvas.top_left_visible.x,
                           box->priv->canvas.bottom_right_visible.x,
                           0.5 * 
                           (box->priv->canvas.top_left_visible.x
                            + box->priv->canvas.bottom_right_visible.x), 20);
   }
}

static void
gtk_databox_vruler_update (GtkDatabox *box)
{
   if (box->priv->rulerY)
   {
      gtk_ruler_set_range (GTK_RULER (box->priv->rulerY), 
                           box->priv->canvas.top_left_visible.y,
                           box->priv->canvas.bottom_right_visible.y,
                           0.5 * 
                           (box->priv->canvas.top_left_visible.y
                            + box->priv->canvas.bottom_right_visible.y), 20);
   }
}

gint
gtk_databox_auto_rescale (GtkDatabox *box, gfloat border)
{
   GtkDataboxValue min;
   GtkDataboxValue max;
   gfloat buffer;
   
   if (0 > gtk_databox_calculate_extrema (GTK_DATABOX (box), 
                                          &min, &max))
   {
      g_warning ("Calculating extrema failed. Resorting to default values");
      min.x = -100.;
      min.y = -100.;
      max.x = +100.;
      max.y = +100.;
   }
   else
   {
      gfloat width = max.x - min.x;
      gfloat height = max.y - min.y;
      
      min.x -= border * width;
      min.y -= border * height;
      max.x += border * width;
      max.y += border * height;
   }

   buffer = min.y;
   min.y = max.y;
   max.y = buffer;
   
   gtk_databox_set_canvas (GTK_DATABOX(box), min, max);

   return 0;
}


gint
gtk_databox_calculate_extrema (GtkDatabox *box,
                               GtkDataboxValue *min,
                               GtkDataboxValue *max)
{
   GList *list;
   gint return_val = -1;
   gboolean first = TRUE;

   g_return_val_if_fail (GTK_IS_DATABOX (box), -1);

   list = g_list_last (box->priv->graphs);

   while (list)
   {
      GtkDataboxValue graph_min;
      GtkDataboxValue graph_max;
      gint value = -1;

      if (list->data)
      {
         value = gtk_databox_graph_calculate_extrema (
                    GTK_DATABOX_GRAPH (list->data), &graph_min, &graph_max);
      }
      else
      {
         /* Do nothing if data == NULL */
      }

      if (value >= 0)
      {
         return_val = 0;

         if (first)
         {
            /* The min and max values need to be initialized with the 
             * first valid values from the graph
             */
            *min = graph_min;
            *max = graph_max;

            first = FALSE;
         }
         else
         {
            min->x = MIN (min->x, graph_min.x);
            min->y = MIN (min->y, graph_min.y);
            max->x = MAX (max->x, graph_max.x);
            max->y = MAX (max->y, graph_max.y);
         }
      }
      
      list = g_list_previous (list);
   }
 
   return return_val;
}

void
gtk_databox_set_canvas (GtkDatabox *box,
                         GtkDataboxValue top_left,
                         GtkDataboxValue bottom_right)
{
   g_return_if_fail (GTK_IS_DATABOX (box));
   g_return_if_fail (top_left.x != bottom_right.x 
                  && top_left.y != bottom_right.y);

   box->priv->canvas.top_left_total = top_left;
   box->priv->canvas.bottom_right_total = bottom_right;

   gtk_databox_zoom_home (box);
}

void
gtk_databox_set_visible_canvas (GtkDatabox *box,
                                 GtkDataboxValue top_left,
                                 GtkDataboxValue bottom_right)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   /* The corners of the visible canvas have to be inside the total canvas
    * and they have to be ordered the same way (e.g. ascending left to right
    */
   g_return_if_fail ((box->priv->canvas.top_left_total.x <= top_left.x 
                  && top_left.x < bottom_right.x
                  && bottom_right.x <= box->priv->canvas.bottom_right_total.x)
                  || (box->priv->canvas.top_left_total.x >= top_left.x 
                  && top_left.x > bottom_right.x
                  && bottom_right.x >= box->priv->canvas.bottom_right_total.x));
   g_return_if_fail ((box->priv->canvas.top_left_total.y <= top_left.y 
                  && top_left.y < bottom_right.y
                  && bottom_right.y <= box->priv->canvas.bottom_right_total.y)
                  || (box->priv->canvas.top_left_total.y >= top_left.y 
                  && top_left.y > bottom_right.y
                  && bottom_right.y >= box->priv->canvas.bottom_right_total.y));

   box->priv->adjX->value = (top_left.x - box->priv->canvas.top_left_total.x)
                          / (box->priv->canvas.bottom_right_total.x 
                             - box->priv->canvas.top_left_total.x);

   box->priv->adjY->value = (top_left.y - box->priv->canvas.top_left_total.y)
                          / (box->priv->canvas.bottom_right_total.y 
                             - box->priv->canvas.top_left_total.y);

   box->priv->adjX->page_size = (top_left.x - bottom_right.x)
                              / (box->priv->canvas.top_left_total.x
                                 - box->priv->canvas.bottom_right_total.x);

   box->priv->adjY->page_size = (top_left.y - bottom_right.y)
                              / (box->priv->canvas.top_left_total.y
                                 - box->priv->canvas.bottom_right_total.y);

   gtk_databox_zoomed (box);
}

void
gtk_databox_get_canvas (GtkDatabox *box,
                         GtkDataboxValue *top_left,
                         GtkDataboxValue *bottom_right)
{
   g_return_if_fail (GTK_IS_DATABOX (box));
   g_return_if_fail (top_left);
   g_return_if_fail (bottom_right);


   *top_left = box->priv->canvas.top_left_total;
   *bottom_right = box->priv->canvas.bottom_right_total;
}

void
gtk_databox_get_visible_canvas (GtkDatabox *box,
                                 GtkDataboxValue *top_left,
                                 GtkDataboxValue *bottom_right)
{
   g_return_if_fail (GTK_IS_DATABOX (box));
   g_return_if_fail (top_left);
   g_return_if_fail (bottom_right);

   *top_left = box->priv->canvas.top_left_visible;
   *bottom_right = box->priv->canvas.bottom_right_visible;
}


gboolean 
gtk_databox_graph_add (GtkDatabox *box, GtkDataboxGraph *graph)
{
   g_return_val_if_fail (GTK_IS_DATABOX (box), FALSE);
   g_return_val_if_fail (GTK_DATABOX_IS_GRAPH (graph), FALSE);

   box->priv->graphs = g_list_append (box->priv->graphs, graph);

   return (box->priv->graphs != NULL);
}

void
gtk_databox_graph_remove (GtkDatabox *box, GtkDataboxGraph *graph)
{
   GList *list;
   
   g_return_if_fail (GTK_IS_DATABOX (box));

   list = g_list_find (box->priv->graphs, graph);
   g_return_if_fail (list);

   box->priv->graphs = g_list_delete_link (box->priv->graphs, list);
}

void
gtk_databox_graph_remove_all (GtkDatabox *box)
{
   g_return_if_fail (GTK_IS_DATABOX (box));

   g_list_free (box->priv->graphs);

   box->priv->graphs = 0;
}

GtkDataboxValue
gtk_databox_value_from_coord (GtkDatabox *box, GtkDataboxCoord coord)
{
   GtkDataboxValue value = {0.0, 0.0};

   g_return_val_if_fail (GTK_IS_DATABOX (box), value);

   value.x = box->priv->canvas.top_left_visible.x 
              + coord.x / box->priv->canvas.translation_factor.x;
   value.y = box->priv->canvas.top_left_visible.y 
              + coord.y / box->priv->canvas.translation_factor.y;

   return value;
}

GtkDataboxCoord
gtk_databox_coord_from_value (GtkDatabox *box, GtkDataboxValue value)
{
   GtkDataboxCoord coord = {0, 0};

   g_return_val_if_fail (GTK_IS_DATABOX (box), coord);

   coord.x = (gint16) ((value.x - box->priv->canvas.top_left_visible.x)
                            * box->priv->canvas.translation_factor.x);

   coord.y = (gint16) ((value.y - box->priv->canvas.top_left_visible.y)
                            * box->priv->canvas.translation_factor.y);

   return coord;
}

#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkhruler.h>
#include <gtk/gtkvruler.h>
void
gtk_databox_create_box_with_scrollbars_and_rulers (GtkWidget **p_box, 
                                                   GtkWidget **p_table,
                                                   gboolean hscrollbar,
                                                   gboolean vscrollbar,
                                                   gboolean hruler,
                                                   gboolean vruler)
{
   GtkWidget *table;
   GtkWidget *box;
   GtkWidget *scrollbar;
   GtkWidget *ruler;

   *p_table = table = gtk_table_new (3, 3, FALSE);
   *p_box = box = gtk_databox_new ();

   gtk_table_attach (GTK_TABLE (table), box, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

   if (hscrollbar)
   {
      scrollbar = gtk_hscrollbar_new (NULL);
      gtk_databox_set_hadjustment (GTK_DATABOX (box), 
                           gtk_range_get_adjustment (GTK_RANGE (scrollbar)));
      gtk_table_attach (GTK_TABLE (table), scrollbar, 1, 2, 2, 3,
                        GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
   }

   if (vscrollbar)
   {
      scrollbar = gtk_vscrollbar_new (NULL);
      gtk_databox_set_vadjustment (GTK_DATABOX (box), 
                           gtk_range_get_adjustment (GTK_RANGE (scrollbar)));
      gtk_table_attach (GTK_TABLE (table), scrollbar, 2, 3, 1, 2,
                        GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
   }

   if (hruler)
   {
      ruler = gtk_hruler_new ();
      gtk_table_attach (GTK_TABLE (table), ruler, 1, 2, 0, 1,
                       GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
      gtk_databox_set_hruler (GTK_DATABOX (box), GTK_RULER (ruler));
   }

   if (vruler)
   {
      ruler = gtk_vruler_new ();
      gtk_table_attach (GTK_TABLE (table), ruler, 0, 1, 1, 2,
                        GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
      gtk_databox_set_vruler (GTK_DATABOX (box), GTK_RULER (ruler));
   }
}
