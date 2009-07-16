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

#ifndef __gtk_databox_marshal_MARSHAL_H__
#define __gtk_databox_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS
/* VOID:VOID (/dev/stdin:1) */
#define gtk_databox_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID
/* VOID:POINTER (/dev/stdin:2) */
#define gtk_databox_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER
/* VOID:POINTER,POINTER (/dev/stdin:3) */
extern void gtk_databox_marshal_VOID__POINTER_POINTER (GClosure * closure,
						       GValue * return_value,
						       guint n_param_values,
						       const GValue *
						       param_values,
						       gpointer
						       invocation_hint,
						       gpointer marshal_data);

G_END_DECLS
#endif /* __gtk_databox_marshal_MARSHAL_H__ */
