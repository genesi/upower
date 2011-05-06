/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __UP_QOS_H
#define __UP_QOS_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define UP_TYPE_QOS		(up_qos_get_type ())
#define UP_QOS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), UP_TYPE_QOS, UpQos))
#define UP_QOS_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), UP_TYPE_QOS, UpQosClass))
#define UP_IS_QOS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), UP_TYPE_QOS))
#define UP_IS_QOS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), UP_TYPE_QOS))
#define UP_QOS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), UP_TYPE_QOS, UpQosClass))

typedef struct UpQosPrivate UpQosPrivate;

typedef struct
{
	GObject		  parent;
	UpQosPrivate	 *priv;
} UpQos;

typedef struct
{
	GObjectClass	parent_class;
	void		(* latency_changed)		(UpQos		*qos,
							 const gchar	*type,
							 gint		 value);
	void		(* requests_changed)		(UpQos		*qos);
} UpQosClass;

UpQos		*up_qos_new				(void);
GType		 up_qos_get_type			(void);
void		 up_qos_test				(gpointer	 user_data);

void		 up_qos_request_latency		(UpQos		*qos,
							 const gchar	*type,
							 gint		 value,
							 gboolean	 persistent,
							 DBusGMethodInvocation *context);
void		 up_qos_cancel_request			(UpQos		*qos,
							 guint32	 cookie,
							 DBusGMethodInvocation *context);
void		 up_qos_set_minimum_latency		(UpQos		*qos,
							 const gchar	*type,
							 gint		 value,
							 DBusGMethodInvocation *context);
gboolean	 up_qos_get_latency			(UpQos		*qos,
							 const gchar	*type,
							 gint		*value,
							 GError		**error);
gboolean	 up_qos_get_latency_requests		(UpQos		*qos,
							 GPtrArray	**requests,
							 GError		**error);

G_END_DECLS

#endif	/* __UP_QOS_H */
