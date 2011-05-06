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

/**
 * SECTION:up-qos-item
 * @short_description: Helper object representing one item of QOS data.
 *
 * This object represents one item of data which may be returned from the
 * daemon in response to a query.
 *
 * See also: #UpDevice, #UpClient
 */

#include "config.h"

#include <glib.h>

#include "up-qos-item.h"

static void	up_qos_item_class_init	(UpQosItemClass	*klass);
static void	up_qos_item_init		(UpQosItem		*qos_item);
static void	up_qos_item_finalize		(GObject		*object);

#define UP_QOS_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_QOS_ITEM, UpQosItemPrivate))

struct UpQosItemPrivate
{
	guint			 uid;
	guint			 pid;
	gchar			*sender;
	gchar			*cmdline;
	guint			 cookie;
	guint64			 timespec;
	gboolean		 persistent;
	UpQosKind		 type;
	gint			 value;
};

enum {
	PROP_0,
	PROP_UID,
	PROP_PID,
	PROP_SENDER,
	PROP_CMDLINE,
	PROP_COOKIE,
	PROP_TIMESPEC,
	PROP_PERSISTENT,
	PROP_TYPE,
	PROP_VALUE,
	PROP_LAST
};

G_DEFINE_TYPE (UpQosItem, up_qos_item, G_TYPE_OBJECT)


/**
 * up_qos_item_get_uid:
 * @qos_item: #UpQosItem
 *
 * Gets the item uid.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
guint
up_qos_item_get_uid (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT);
	return qos_item->priv->uid;
}

/**
 * up_qos_item_set_uid:
 * @qos_item: #UpQosItem
 * @uid: the new value
 *
 * Sets the item uid.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_uid (UpQosItem *qos_item, guint uid)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->uid = uid;
	g_object_notify (G_OBJECT(qos_item), "uid");
}

/**
 * up_qos_item_get_pid:
 * @qos_item: #UpQosItem
 *
 * Gets the item pid.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
guint
up_qos_item_get_pid (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT);
	return qos_item->priv->pid;
}

/**
 * up_qos_item_set_pid:
 * @qos_item: #UpQosItem
 * @pid: the new value
 *
 * Sets the item pid.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_pid (UpQosItem *qos_item, guint pid)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->pid = pid;
	g_object_notify (G_OBJECT(qos_item), "pid");
}

/**
 * up_qos_item_get_sender:
 * @qos_item: #UpQosItem
 *
 * Gets the item sender.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
const gchar *
up_qos_item_get_sender (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), NULL);
	return qos_item->priv->sender;
}

/**
 * up_qos_item_set_sender:
 * @qos_item: #UpQosItem
 * @sender: the new value
 *
 * Sets the item sender.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_sender (UpQosItem *qos_item, const gchar *sender)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	g_free (qos_item->priv->sender);
	qos_item->priv->sender = g_strdup (sender);
	g_object_notify (G_OBJECT(qos_item), "sender");
}

/**
 * up_qos_item_get_cmdline:
 * @qos_item: #UpQosItem
 *
 * Gets the item cmdline.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
const gchar *
up_qos_item_get_cmdline (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), NULL);
	return qos_item->priv->cmdline;
}

/**
 * up_qos_item_set_cmdline:
 * @qos_item: #UpQosItem
 * @cmdline: the new value
 *
 * Sets the item cmdline.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_cmdline (UpQosItem *qos_item, const gchar *cmdline)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	g_free (qos_item->priv->cmdline);
	qos_item->priv->cmdline = g_strdup (cmdline);
	g_object_notify (G_OBJECT(qos_item), "cmdline");
}

/**
 * up_qos_item_get_cookie:
 * @qos_item: #UpQosItem
 *
 * Gets the item cookie.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
guint
up_qos_item_get_cookie (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT);
	return qos_item->priv->cookie;
}

/**
 * up_qos_item_set_cookie:
 * @qos_item: #UpQosItem
 * @cookie: the new value
 *
 * Sets the item cookie.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_cookie (UpQosItem *qos_item, guint cookie)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->cookie = cookie;
	g_object_notify (G_OBJECT(qos_item), "cookie");
}

/**
 * up_qos_item_get_timespec:
 * @qos_item: #UpQosItem
 *
 * Gets the item timespec.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
guint64
up_qos_item_get_timespec (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT64);
	return qos_item->priv->timespec;
}

/**
 * up_qos_item_set_timespec:
 * @qos_item: #UpQosItem
 * @timespec: the new value
 *
 * Sets the item timespec.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_timespec (UpQosItem *qos_item, guint64 timespec)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->timespec = timespec;
	g_object_notify (G_OBJECT(qos_item), "timespec");
}

/**
 * up_qos_item_get_persistent:
 * @qos_item: #UpQosItem
 *
 * Gets the item persistent.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
gboolean
up_qos_item_get_persistent (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT);
	return qos_item->priv->persistent;
}

/**
 * up_qos_item_set_persistent:
 * @qos_item: #UpQosItem
 * @persistent: the new value
 *
 * Sets the item persistent.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_persistent (UpQosItem *qos_item, gboolean persistent)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->persistent = persistent;
	g_object_notify (G_OBJECT(qos_item), "persistent");
}

/**
 * up_qos_item_get_kind:
 * @qos_item: #UpQosItem
 *
 * Gets the item type.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
UpQosKind
up_qos_item_get_kind (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXUINT);
	return qos_item->priv->type;
}

/**
 * up_qos_item_set_kind:
 * @qos_item: #UpQosItem
 * @type: the new value
 *
 * Sets the item type.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_kind (UpQosItem *qos_item, UpQosKind type)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->type = type;
	g_object_notify (G_OBJECT(qos_item), "type");
}

/**
 * up_qos_item_get_value:
 * @qos_item: #UpQosItem
 *
 * Gets the item value.
 *
 * Return value: the value
 *
 * Since: 0.9.0
 **/
gint
up_qos_item_get_value (UpQosItem *qos_item)
{
	g_return_val_if_fail (UP_IS_QOS_ITEM (qos_item), G_MAXINT);
	return qos_item->priv->value;
}

/**
 * up_qos_item_set_value:
 * @qos_item: #UpQosItem
 * @value: the new value
 *
 * Sets the item value.
 *
 * Since: 0.9.0
 **/
void
up_qos_item_set_value (UpQosItem *qos_item, gint value)
{
	g_return_if_fail (UP_IS_QOS_ITEM (qos_item));
	qos_item->priv->value = value;
	g_object_notify (G_OBJECT(qos_item), "value");
}

/**
 * up_qos_item_set_property:
 **/
static void
up_qos_item_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	UpQosItem *qos_item = UP_QOS_ITEM (object);

	switch (prop_id) {
	case PROP_UID:
		qos_item->priv->uid = g_value_get_uint (value);
		break;
	case PROP_PID:
		qos_item->priv->pid = g_value_get_uint (value);
		break;
	case PROP_SENDER:
		g_free (qos_item->priv->sender);
		qos_item->priv->sender = g_strdup (g_value_get_string (value));
		break;
	case PROP_CMDLINE:
		g_free (qos_item->priv->cmdline);
		qos_item->priv->cmdline = g_strdup (g_value_get_string (value));
		break;
	case PROP_COOKIE:
		qos_item->priv->cookie = g_value_get_uint (value);
		break;
	case PROP_TIMESPEC:
		qos_item->priv->timespec = g_value_get_uint64 (value);
		break;
	case PROP_PERSISTENT:
		qos_item->priv->persistent = g_value_get_boolean (value);
		break;
	case PROP_TYPE:
		qos_item->priv->type = g_value_get_uint (value);
		break;
	case PROP_VALUE:
		qos_item->priv->value = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_qos_item_get_property:
 **/
static void
up_qos_item_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	UpQosItem *qos_item = UP_QOS_ITEM (object);

	switch (prop_id) {
	case PROP_UID:
		g_value_set_uint (value, qos_item->priv->uid);
		break;
	case PROP_PID:
		g_value_set_uint (value, qos_item->priv->pid);
		break;
	case PROP_SENDER:
		g_value_set_string (value, qos_item->priv->sender);
		break;
	case PROP_CMDLINE:
		g_value_set_string (value, qos_item->priv->cmdline);
		break;
	case PROP_COOKIE:
		g_value_set_uint (value, qos_item->priv->cookie);
		break;
	case PROP_TIMESPEC:
		g_value_set_uint64 (value, qos_item->priv->timespec);
		break;
	case PROP_PERSISTENT:
		g_value_set_boolean (value, qos_item->priv->persistent);
		break;
	case PROP_TYPE:
		g_value_set_uint (value, qos_item->priv->type);
		break;
	case PROP_VALUE:
		g_value_set_int (value, qos_item->priv->value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_qos_item_class_init:
 * @klass: The UpQosItemClass
 **/
static void
up_qos_item_class_init (UpQosItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_qos_item_finalize;
	object_class->set_property = up_qos_item_set_property;
	object_class->get_property = up_qos_item_get_property;

	/**
	 * UpQosItem:uid:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_UID,
					 g_param_spec_uint ("uid", NULL, NULL,
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));
	/**
	 * UpQosItem:pid:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_PID,
					 g_param_spec_uint ("pid", NULL, NULL,
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));
	/**
	 * UpQosItem:sender:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_SENDER,
					 g_param_spec_string ("sender", NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpQosItem:cmdline:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_CMDLINE,
					 g_param_spec_string ("cmdline", NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpQosItem:cookie:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_COOKIE,
					 g_param_spec_uint ("cookie", NULL, NULL,
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE));
	/**
	 * UpQosItem:timespec:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_TIMESPEC,
					 g_param_spec_uint64 ("timespec", NULL, NULL,
							      0, G_MAXUINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpQosItem:persistent:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_PERSISTENT,
					 g_param_spec_boolean ("persistent", NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpQosItem:type:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_uint ("type", NULL, NULL,
							    0, G_MAXUINT, UP_QOS_KIND_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * UpQosItem:value:
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_VALUE,
					 g_param_spec_int ("value", NULL, NULL,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (UpQosItemPrivate));
}

/**
 * up_qos_item_init:
 * @qos_item: This class instance
 **/
static void
up_qos_item_init (UpQosItem *qos_item)
{
	qos_item->priv = UP_QOS_ITEM_GET_PRIVATE (qos_item);
	qos_item->priv->value = 0.0f;
	qos_item->priv->uid = 0;
	qos_item->priv->pid = 0;
	qos_item->priv->sender = NULL;
	qos_item->priv->cmdline = NULL;
	qos_item->priv->cookie = 0;
	qos_item->priv->timespec = 0;
	qos_item->priv->persistent = FALSE;
	qos_item->priv->type = UP_QOS_KIND_UNKNOWN;
	qos_item->priv->value = 0;
}

/**
 * up_qos_item_finalize:
 * @object: The object to finalize
 **/
static void
up_qos_item_finalize (GObject *object)
{
	UpQosItem *qos_item;

	g_return_if_fail (UP_IS_QOS_ITEM (object));

	qos_item = UP_QOS_ITEM (object);

	g_free (qos_item->priv->sender);
	g_free (qos_item->priv->cmdline);

	G_OBJECT_CLASS (up_qos_item_parent_class)->finalize (object);
}

/**
 * up_qos_item_new:
 *
 * Return value: a new UpQosItem object.
 *
 * Since: 0.9.0
 **/
UpQosItem *
up_qos_item_new (void)
{
	UpQosItem *qos_item;
	qos_item = g_object_new (UP_TYPE_QOS_ITEM, NULL);
	return UP_QOS_ITEM (qos_item);
}

