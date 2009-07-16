/* GtkDatabox - An extension to the gtk+ library
 * Copyright (C) 1998 - 2005  Dr. Roland Bock
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

/* basic version created by glib-genmarshal with gtk/gtkdataboxmarshal.list */

#include	<glib-object.h>

#define g_marshal_value_peek_pointer(v)  g_value_get_pointer (v)

void
gtk_databox_marshal_VOID__POINTER_POINTER (GClosure * closure,
					   GValue * return_value,
					   guint n_param_values,
					   const GValue * param_values,
					   gpointer invocation_hint,
					   gpointer marshal_data)
{
   typedef void (*GMarshalFunc_VOID__POINTER_POINTER) (gpointer data1,
						       gpointer arg_1,
						       gpointer arg_2,
						       gpointer data2);
   register GMarshalFunc_VOID__POINTER_POINTER callback;
   register GCClosure *cc = (GCClosure *) closure;
   register gpointer data1, data2;

   g_return_if_fail (n_param_values == 3);

   if (G_CCLOSURE_SWAP_DATA (closure))
   {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
   }
   else
   {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
   }
   callback =
      (GMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->
					    callback);

   callback (data1,
	     g_marshal_value_peek_pointer (param_values + 1),
	     g_marshal_value_peek_pointer (param_values + 2), data2);
}

