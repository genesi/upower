/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2010 Richard Hughes <richard@hughsie.com>
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

#if !defined (__UPOWER_H_INSIDE__) && !defined (UP_COMPILATION)
#error "Only <upower.h> can be included directly."
#endif

#ifndef __UP_QOS_ITEM_H
#define __UP_QOS_ITEM_H

#include <glib-object.h>
#include <libupower-glib/up-types.h>

G_BEGIN_DECLS

#define UP_TYPE_QOS_ITEM		(up_qos_item_get_type ())
#define UP_QOS_ITEM(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), UP_TYPE_QOS_ITEM, UpQosItem))
#define UP_QOS_ITEM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), UP_TYPE_QOS_ITEM, UpQosItemClass))
#define UP_IS_QOS_ITEM(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), UP_TYPE_QOS_ITEM))
#define UP_IS_QOS_ITEM_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), UP_TYPE_QOS_ITEM))
#define UP_QOS_ITEM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), UP_TYPE_QOS_ITEM, UpQosItemClass))

typedef struct UpQosItemPrivate UpQosItemPrivate;

typedef struct
{
	 GObject		 parent;
	 UpQosItemPrivate	*priv;
} UpQosItem;

typedef struct
{
	GObjectClass		 parent_class;
} UpQosItemClass;

GType		 up_qos_item_get_type	(void);
UpQosItem	*up_qos_item_new			(void);

/* accessors */
guint		 up_qos_item_get_uid			(UpQosItem		*qos_item);
void		 up_qos_item_set_uid			(UpQosItem		*qos_item,
							 guint			 uid);
guint		 up_qos_item_get_pid			(UpQosItem		*qos_item);
void		 up_qos_item_set_pid			(UpQosItem		*qos_item,
							 guint			 pid);
const gchar	*up_qos_item_get_sender			(UpQosItem		*qos_item);
void		 up_qos_item_set_sender			(UpQosItem		*qos_item,
							 const gchar		*sender);
const gchar	*up_qos_item_get_cmdline		(UpQosItem		*qos_item);
void		 up_qos_item_set_cmdline		(UpQosItem		*qos_item,
							 const gchar		*cmdline);
guint		 up_qos_item_get_cookie			(UpQosItem		*qos_item);
void		 up_qos_item_set_cookie			(UpQosItem		*qos_item,
							 guint			 cookie);
guint64		 up_qos_item_get_timespec		(UpQosItem		*qos_item);
void		 up_qos_item_set_timespec		(UpQosItem		*qos_item,
							 guint64		 timespec);
gboolean	 up_qos_item_get_persistent		(UpQosItem		*qos_item);
void		 up_qos_item_set_persistent		(UpQosItem		*qos_item,
							 gboolean		 persistent);
UpQosKind	 up_qos_item_get_kind			(UpQosItem		*qos_item);
void		 up_qos_item_set_kind			(UpQosItem		*qos_item,
							 UpQosKind		 type);
gint		 up_qos_item_get_value			(UpQosItem		*qos_item);
void		 up_qos_item_set_value			(UpQosItem		*qos_item,
							 gint			 value);

G_END_DECLS

#endif /* __UP_QOS_ITEM_H */

