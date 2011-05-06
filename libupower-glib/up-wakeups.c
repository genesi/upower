/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "up-wakeups.h"

static void	up_wakeups_class_init	(UpWakeupsClass	*klass);
static void	up_wakeups_init		(UpWakeups	*wakeups);
static void	up_wakeups_finalize	(GObject	*object);

#define UP_WAKEUPS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_WAKEUPS, UpWakeupsPrivate))

struct UpWakeupsPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
	DBusGProxy		*prop_proxy;
	gboolean		 has_capability;
	gboolean		 have_properties;
};

enum {
	UP_WAKEUPS_DATA_CHANGED,
	UP_WAKEUPS_TOTAL_CHANGED,
	UP_WAKEUPS_LAST_SIGNAL
};

static guint signals [UP_WAKEUPS_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (UpWakeups, up_wakeups, G_TYPE_OBJECT)

/**
 * up_wakeups_get_total_sync:
 * @wakeups: a #UpWakeups instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the the total number of wakeups per second from the daemon.
 *
 * Return value: number of wakeups per second.
 *
 * Since: 0.9.1
 **/
guint
up_wakeups_get_total_sync (UpWakeups *wakeups, GCancellable *cancellable, GError **error)
{
	guint total = 0;
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (UP_IS_WAKEUPS (wakeups), FALSE);
	g_return_val_if_fail (wakeups->priv->proxy != NULL, FALSE);

	ret = dbus_g_proxy_call (wakeups->priv->proxy, "GetTotal", &error_local,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &total,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("Couldn't get total: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return total;
}

/**
 * up_wakeups_get_data_sync:
 * @wakeups: a #UpWakeups instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the wakeups data from the daemon.
 *
 * Return value: an array of %UpWakeupItem's
 *
 * Since: 0.9.1
 **/
GPtrArray *
up_wakeups_get_data_sync (UpWakeups *wakeups, GCancellable *cancellable, GError **error)
{
	GError *error_local = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	UpWakeupItem *item;
	GPtrArray *array = NULL;
	gboolean ret;

	g_return_val_if_fail (UP_IS_WAKEUPS (wakeups), NULL);
	g_return_val_if_fail (wakeups->priv->proxy != NULL, NULL);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_BOOLEAN,
						G_TYPE_UINT,
						G_TYPE_DOUBLE,
						G_TYPE_STRING,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (wakeups->priv->proxy, "GetData", &error_local,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("GetData on failed: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0)
		goto out;

	/* convert */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		item = up_wakeup_item_new ();

		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		up_wakeup_item_set_is_userspace (item, g_value_get_boolean (gv));
		g_value_unset (gv);

		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		up_wakeup_item_set_id (item, g_value_get_uint (gv));
		g_value_unset (gv);

		/* 2 */
		gv = g_value_array_get_nth (gva, 2);
		up_wakeup_item_set_value (item, g_value_get_double (gv));
		g_value_unset (gv);

		/* 3 */
		gv = g_value_array_get_nth (gva, 3);
		up_wakeup_item_set_cmdline (item, g_value_get_string (gv));
		g_value_unset (gv);

		/* 4 */
		gv = g_value_array_get_nth (gva, 4);
		up_wakeup_item_set_details (item, g_value_get_string (gv));
		g_value_unset (gv);

		/* add */
		g_ptr_array_add (array, item);
		g_value_array_free (gva);
	}
out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_unref (gvalue_ptr_array);
	return array;
}

/**
 * up_wakeups_ensure_properties:
 **/
static void
up_wakeups_ensure_properties (UpWakeups *wakeups)
{
	gboolean ret;
	GError *error;
	GHashTable *props;
	GValue *value;

	props = NULL;

	if (wakeups->priv->have_properties)
		goto out;

	error = NULL;
	ret = dbus_g_proxy_call (wakeups->priv->prop_proxy, "GetAll", &error,
				 G_TYPE_STRING, "org.freedesktop.UPower.Wakeups",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("Error invoking GetAll() to get properties: %s", error->message);
		g_error_free (error);
		goto out;
	}

	value = g_hash_table_lookup (props, "HasCapability");
	if (value == NULL) {
		g_warning ("No 'HasCapability' property");
		goto out;
	}
	wakeups->priv->has_capability = g_value_get_boolean (value);

	/* cached */
	wakeups->priv->have_properties = TRUE;

out:
	if (props != NULL)
		g_hash_table_unref (props);
}

/**
 * up_wakeups_get_properties_sync:
 * @wakeups: a #UpWakeups instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets properties from the daemon about wakeup data.
 *
 * Return value: %TRUE if supported
 *
 * Since: 0.9.1
 **/
gboolean
up_wakeups_get_properties_sync (UpWakeups *wakeups, GCancellable *cancellable, GError **error)
{
	g_return_val_if_fail (UP_IS_WAKEUPS (wakeups), FALSE);
	up_wakeups_ensure_properties (wakeups);
	return TRUE;
}

/**
 * up_wakeups_get_has_capability:
 * @wakeups: a #UpWakeups instance.
 *
 * Returns if the daemon supports getting the wakeup data.
 *
 * Return value: %TRUE if supported
 *
 * Since: 0.9.1
 **/
gboolean
up_wakeups_get_has_capability (UpWakeups *wakeups)
{
	g_return_val_if_fail (UP_IS_WAKEUPS (wakeups), FALSE);
	up_wakeups_ensure_properties (wakeups);
	return wakeups->priv->has_capability;
}

/**
 * up_wakeups_total_changed_cb:
 **/
static void
up_wakeups_total_changed_cb (DBusGProxy *proxy, guint value, UpWakeups *wakeups)
{
	g_signal_emit (wakeups, signals [UP_WAKEUPS_TOTAL_CHANGED], 0, value);
}

/**
 * up_wakeups_data_changed_cb:
 **/
static void
up_wakeups_data_changed_cb (DBusGProxy *proxy, UpWakeups *wakeups)
{
	g_signal_emit (wakeups, signals [UP_WAKEUPS_DATA_CHANGED], 0);
}

/**
 * up_wakeups_class_init:
 **/
static void
up_wakeups_class_init (UpWakeupsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_wakeups_finalize;

	signals [UP_WAKEUPS_DATA_CHANGED] =
		g_signal_new ("data-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpWakeupsClass, data_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals [UP_WAKEUPS_TOTAL_CHANGED] =
		g_signal_new ("total-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpWakeupsClass, data_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (UpWakeupsPrivate));
}

/**
 * up_wakeups_init:
 **/
static void
up_wakeups_init (UpWakeups *wakeups)
{
	GError *error = NULL;

	wakeups->priv = UP_WAKEUPS_GET_PRIVATE (wakeups);
	wakeups->priv->has_capability = FALSE;
	wakeups->priv->have_properties = FALSE;

	/* get on the bus */
	wakeups->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (wakeups->priv->bus == NULL) {
		g_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to properties interface */
	wakeups->priv->prop_proxy = dbus_g_proxy_new_for_name (wakeups->priv->bus,
							      "org.freedesktop.UPower",
							      "/org/freedesktop/UPower/Wakeups",
							      "org.freedesktop.DBus.Properties");
	if (wakeups->priv->prop_proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}

	/* connect to main interface */
	wakeups->priv->proxy = dbus_g_proxy_new_for_name (wakeups->priv->bus,
							 "org.freedesktop.UPower",
							 "/org/freedesktop/UPower/Wakeups",
							 "org.freedesktop.UPower.Wakeups");
	if (wakeups->priv->proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}
	dbus_g_proxy_add_signal (wakeups->priv->proxy, "TotalChanged", G_TYPE_UINT, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (wakeups->priv->proxy, "DataChanged", G_TYPE_INVALID);

	/* all callbacks */
	dbus_g_proxy_connect_signal (wakeups->priv->proxy, "TotalChanged",
				     G_CALLBACK (up_wakeups_total_changed_cb), wakeups, NULL);
	dbus_g_proxy_connect_signal (wakeups->priv->proxy, "DataChanged",
				     G_CALLBACK (up_wakeups_data_changed_cb), wakeups, NULL);
out:
	return;
}

/**
 * up_wakeups_finalize:
 **/
static void
up_wakeups_finalize (GObject *object)
{
	UpWakeups *wakeups;

	g_return_if_fail (UP_IS_WAKEUPS (object));

	wakeups = UP_WAKEUPS (object);
	if (wakeups->priv->proxy != NULL)
		g_object_unref (wakeups->priv->proxy);
	if (wakeups->priv->prop_proxy != NULL)
		g_object_unref (wakeups->priv->prop_proxy);

	G_OBJECT_CLASS (up_wakeups_parent_class)->finalize (object);
}

/**
 * up_wakeups_new:
 *
 * Gets a new object to allow querying the wakeups data from the server.
 *
 * Return value: the a new @UpWakeups object.
 *
 * Since: 0.9.1
 **/
UpWakeups *
up_wakeups_new (void)
{
	UpWakeups *wakeups;
	wakeups = g_object_new (UP_TYPE_WAKEUPS, NULL);
	return UP_WAKEUPS (wakeups);
}

