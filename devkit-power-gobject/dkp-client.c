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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "dkp-client.h"
#include "dkp-device.h"

static void	dkp_client_class_init	(DkpClientClass	*klass);
static void	dkp_client_init		(DkpClient	*client);
static void	dkp_client_finalize	(GObject	*object);

#define DKP_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DKP_TYPE_CLIENT, DkpClientPrivate))

struct DkpClientPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
	DBusGProxy		*prop_proxy;
	GHashTable		*hash;

	gboolean		 have_properties;

	gchar			*daemon_version;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	gboolean		 lid_is_closed;
	gboolean		 on_battery;
	gboolean		 on_low_battery;
	gboolean		 lid_is_present;
};

enum {
	DKP_DEVICE_ADDED,
	DKP_DEVICE_CHANGED,
	DKP_DEVICE_REMOVED,
	DKP_CLIENT_CHANGED,
	DKP_CLIENT_LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_DAEMON_VERSION,
	PROP_CAN_SUSPEND,
	PROP_CAN_HIBERNATE,
	PROP_ON_BATTERY,
	PROP_ON_LOW_BATTERY,
	PROP_LID_IS_CLOSED,
	PROP_LID_IS_PRESENT,
	PROP_LAST
};

static guint signals [DKP_CLIENT_LAST_SIGNAL] = { 0 };
static gpointer dkp_client_object = NULL;

G_DEFINE_TYPE (DkpClient, dkp_client, G_TYPE_OBJECT)

/**
 * dkp_client_get_device:
 **/
static DkpDevice *
dkp_client_get_device (DkpClient *client, const gchar *object_path)
{
	DkpDevice *device;
	device = g_hash_table_lookup (client->priv->hash, object_path);
	return device;
}

/**
 * dkp_client_enumerate_devices:
 *
 * Return a list of devices, which need to be unref'd, and the array needs
 * to be freed
 **/
GPtrArray *
dkp_client_enumerate_devices (DkpClient *client, GError **error)
{
	guint i;
	GPtrArray *array;
	DkpDevice *device;
	GList *list;
	guint len;

	array = g_ptr_array_new ();

	list = g_hash_table_get_values (client->priv->hash);

	len = g_list_length (list);
	for (i=0; i < len; i++) {
		device = g_list_nth_data (list, i);
		g_ptr_array_add (array, g_object_ref (device));
	}
	if (list != NULL)
		g_list_free (list);

	return array;
}

/**
 * dkp_client_enumerate_devices_private:
 **/
static GPtrArray *
dkp_client_enumerate_devices_private (DkpClient *client, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	GPtrArray *devices = NULL;
	GType g_type_array;

	if (!client->priv->proxy)
		return NULL;
	g_type_array = dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH);
	ret = dbus_g_proxy_call (client->priv->proxy, "EnumerateDevices", &error_local,
				 G_TYPE_INVALID,
				 g_type_array, &devices,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("Couldn't enumerate devices: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
		g_error_free (error_local);
	}
	return devices;
}

/**
 * dkp_client_suspend:
 * @client : a #DkpClient instance.
 * @error  : a #GError.
 *
 * Puts the computer into a low power state, but state is not preserved if the
 * power is lost.
 *
 * NOTE: The system is still consuming a small amount of power
 *
 * Return value: TRUE if system suspended okay, FALSE other wise.
 **/
gboolean
dkp_client_suspend (DkpClient *client, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	ret = dbus_g_proxy_call (client->priv->proxy, "Suspend", &error_local,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		/* DBus might time out, which is okay */
		if (g_error_matches (error_local, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			g_debug ("DBUS timed out, but recovering");
			ret = TRUE;
			goto out;
		}

		/* an actual error */
		g_warning ("Couldn't suspend: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
	}
out:
	if (error_local != NULL)
		g_error_free (error_local);
	return ret;
}

/**
 * dkp_client_hibernate:
 * @client : a #DkpClient instance.
 * @error  : a #GError.
 *
 * Puts the computer into a low power state, where state is preserved if the
 * power is lost.
 *
 * Return value: TRUE if system suspended okay, FALSE other wise.
 **/
gboolean
dkp_client_hibernate (DkpClient *client, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	ret = dbus_g_proxy_call (client->priv->proxy, "Hibernate", &error_local,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		/* DBus might time out, which is okay */
		if (g_error_matches (error_local, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			g_debug ("DBUS timed out, but recovering");
			ret = TRUE;
			goto out;
		}

		/* an actual error */
		g_warning ("Couldn't hibernate: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
	}
out:
	if (error_local != NULL)
		g_error_free (error_local);
	return ret;
}

/**
 * dkp_client_ensure_properties:
 **/
static void
dkp_client_ensure_properties (DkpClient *client)
{
	gboolean ret;
	gboolean allowed = FALSE;
	GError *error;
	GHashTable *props;
	GValue *value;

	props = NULL;

	if (client->priv->have_properties)
		goto out;
	if (!client->priv->prop_proxy)
		goto out;

	error = NULL;
	ret = dbus_g_proxy_call (client->priv->prop_proxy, "GetAll", &error,
				 G_TYPE_STRING, "org.freedesktop.UPower",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
				 G_TYPE_INVALID);
	if (!ret) {
		g_warning ("Error invoking GetAll() to get properties: %s", error->message);
		g_error_free (error);
		goto out;
	}

	value = g_hash_table_lookup (props, "DaemonVersion");
	if (value == NULL) {
		g_warning ("No 'DaemonVersion' property");
		goto out;
	}
	client->priv->daemon_version = g_strdup (g_value_get_string (value));

	value = g_hash_table_lookup (props, "CanSuspend");
	if (value == NULL) {
		g_warning ("No 'CanSuspend' property");
		goto out;
	}

	ret = dbus_g_proxy_call (client->priv->proxy, "SuspendAllowed", &error,
	        G_TYPE_INVALID, G_TYPE_BOOLEAN, &allowed, G_TYPE_INVALID);
	if (!ret)
	        goto out;

	ret = g_value_get_boolean (value) && allowed;
	if (ret != client->priv->can_suspend) {
		client->priv->can_suspend = ret;
		g_object_notify (G_OBJECT(client), "can-suspend");
	}

	value = g_hash_table_lookup (props, "CanHibernate");
	if (value == NULL) {
		g_warning ("No 'CanHibernate' property");
		goto out;
	}
	ret = dbus_g_proxy_call (client->priv->proxy, "HibernateAllowed", &error,
	        G_TYPE_INVALID, G_TYPE_BOOLEAN, &allowed, G_TYPE_INVALID);
	if (!ret)
	        goto out;

	ret = g_value_get_boolean (value) && allowed;
	if (ret != client->priv->can_hibernate) {
		client->priv->can_hibernate = ret;
		g_object_notify (G_OBJECT(client), "can-hibernate");
	}

	value = g_hash_table_lookup (props, "LidIsClosed");
	if (value == NULL) {
		g_warning ("No 'LidIsClosed' property");
		goto out;
	}
	ret = g_value_get_boolean (value);
	if (ret != client->priv->lid_is_closed) {
		client->priv->lid_is_closed = ret;
		g_object_notify (G_OBJECT(client), "lid-is-closed");
	}

	value = g_hash_table_lookup (props, "OnBattery");
	if (value == NULL) {
		g_warning ("No 'OnBattery' property");
		goto out;
	}
	ret = g_value_get_boolean (value);
	if (ret != client->priv->on_battery) {
		client->priv->on_battery = ret;
		g_object_notify (G_OBJECT(client), "on-battery");
	}

	value = g_hash_table_lookup (props, "OnLowBattery");
	if (value == NULL) {
		g_warning ("No 'OnLowBattery' property");
		goto out;
	}
	ret = g_value_get_boolean (value);
	if (ret != client->priv->on_low_battery) {
		client->priv->on_low_battery = ret;
		g_object_notify (G_OBJECT(client), "on-low-battery");
	}

	value = g_hash_table_lookup (props, "LidIsPresent");
	if (value == NULL) {
		g_warning ("No 'LidIsPresent' property");
		goto out;
	}
	ret = g_value_get_boolean (value);
	if (ret != client->priv->lid_is_present) {
		client->priv->lid_is_present = ret;
		g_object_notify (G_OBJECT(client), "lid-is-present");
	}

	/* cached */
	client->priv->have_properties = TRUE;

out:
	if (props != NULL)
		g_hash_table_unref (props);
}

#ifndef UP_DISABLE_DEPRECATED
/**
 * dkp_client_get_daemon_version:
 * @client : a #DkpClient instance.
 *
 * Get DeviceKit-power daemon version.
 *
 * Return value: string containing the daemon version, e.g. 008
 **/
const gchar *
dkp_client_get_daemon_version (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), NULL);
	dkp_client_ensure_properties (client);
	return client->priv->daemon_version;
}

/**
 * dkp_client_can_hibernate:
 * @client : a #DkpClient instance.
 *
 * Get whether the system is able to hibernate.
 *
 * Return value: TRUE if system can hibernate, FALSE other wise.
 **/
gboolean
dkp_client_can_hibernate (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	dkp_client_ensure_properties (client);
	return client->priv->can_hibernate;
}

/**
 * dkp_client_lid_is_closed:
 * @client : a #DkpClient instance.
 *
 * Get whether the laptop lid is closed.
 *
 * Return value: %TRUE if lid is closed or %FALSE otherwise.
 */
gboolean
dkp_client_lid_is_closed (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	dkp_client_ensure_properties (client);
	return client->priv->lid_is_closed;
}

/**
 * dkp_client_can_suspend:
 * @client : a #DkpClient instance.
 *
 * Get whether the system is able to suspend.
 *
 * Return value: TRUE if system can suspend, FALSE other wise.
 **/
gboolean
dkp_client_can_suspend (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	dkp_client_ensure_properties (client);
	return client->priv->can_suspend;
}

/**
 * dkp_client_on_battery:
 * @client : a #DkpClient instance.
 *
 * Get whether the system is running on battery power.
 *
 * Return value: TRUE if the system is currently running on battery, FALSE other wise.
 **/
gboolean
dkp_client_on_battery (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	dkp_client_ensure_properties (client);
	return client->priv->on_battery;
}

/**
 * dkp_client_on_low_battery:
 * @client : a #DkpClient instance.
 *
 * Get whether the system is running on low battery power.
 *
 * Return value: TRUE if the system is currently on low battery power, FALSE other wise.
 **/
gboolean
dkp_client_on_low_battery (DkpClient *client)
{
	g_return_val_if_fail (DKP_IS_CLIENT (client), FALSE);
	dkp_client_ensure_properties (client);
	return client->priv->on_low_battery;
}
#endif

/**
 * dkp_client_add:
 **/
static DkpDevice *
dkp_client_add (DkpClient *client, const gchar *object_path)
{
	DkpDevice *device;

	/* create new device */
	device = dkp_device_new ();
	dkp_device_set_object_path (device, object_path, NULL);

	g_hash_table_insert (client->priv->hash, g_strdup (object_path), device);
	return device;
}

/**
 * dkp_client_added_cb:
 **/
static void
dkp_device_added_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpDevice *device;

	/* create new device */
	device = dkp_client_add (client, object_path);
	g_signal_emit (client, signals [DKP_DEVICE_ADDED], 0, device);
}

/**
 * dkp_client_changed_cb:
 **/
static void
dkp_device_changed_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpDevice *device;
	device = dkp_client_get_device (client, object_path);
	if (device != NULL)
		g_signal_emit (client, signals [DKP_DEVICE_CHANGED], 0, device);
}

/**
 * dkp_client_removed_cb:
 **/
static void
dkp_device_removed_cb (DBusGProxy *proxy, const gchar *object_path, DkpClient *client)
{
	DkpDevice *device;
	device = dkp_client_get_device (client, object_path);
	if (device != NULL)
		g_signal_emit (client, signals [DKP_DEVICE_REMOVED], 0, device);
	g_hash_table_remove (client->priv->hash, dkp_device_get_object_path (device));
}

/**
 * dkp_client_changed_cb:
 **/
static void
dkp_client_changed_cb (DBusGProxy *proxy, DkpClient *client)
{
	client->priv->have_properties = FALSE;
	g_signal_emit (client, signals [DKP_CLIENT_CHANGED], 0);
}

static void
dkp_client_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	DkpClient *client;
	client = DKP_CLIENT (object);

	dkp_client_ensure_properties (client);

	switch (prop_id) {
	case PROP_DAEMON_VERSION:
		g_value_set_string (value, client->priv->daemon_version);
		break;
	case PROP_CAN_SUSPEND:
		g_value_set_boolean (value, client->priv->can_suspend);
		break;
	case PROP_CAN_HIBERNATE:
		g_value_set_boolean (value, client->priv->can_hibernate);
		break;
	case PROP_ON_BATTERY:
		g_value_set_boolean (value, client->priv->on_battery);
		break;
	case PROP_ON_LOW_BATTERY:
		g_value_set_boolean (value, client->priv->on_low_battery);
		break;
	case PROP_LID_IS_CLOSED:
		g_value_set_boolean (value, client->priv->lid_is_closed);
		break;
	case PROP_LID_IS_PRESENT:
		g_value_set_boolean (value, client->priv->lid_is_present);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * dkp_client_class_init:
 * @klass: The DkpClientClass
 **/
static void
dkp_client_class_init (DkpClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = dkp_client_get_property;
	object_class->finalize = dkp_client_finalize;

	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_SUSPEND,
					 g_param_spec_boolean ("can-suspend",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_HIBERNATE,
					 g_param_spec_boolean ("can-hibernate",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_BATTERY,
					 g_param_spec_boolean ("on-battery",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_LOW_BATTERY,
					 g_param_spec_boolean ("on-low-battery",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_CLOSED,
					 g_param_spec_boolean ("lid-is-closed",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_PRESENT,
					 g_param_spec_boolean ("lid-is-present",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READABLE));

	signals [DKP_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, device_added),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DKP_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, device_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DKP_DEVICE_CHANGED] =
		g_signal_new ("device-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, device_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals [DKP_CLIENT_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (DkpClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (DkpClientPrivate));
}

/**
 * dkp_client_init:
 * @client: This class instance
 **/
static void
dkp_client_init (DkpClient *client)
{
	GError *error = NULL;
	const gchar *object_path;
	GPtrArray *devices;
	guint i;

	client->priv = DKP_CLIENT_GET_PRIVATE (client);
	client->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, g_object_unref);
	client->priv->have_properties = FALSE;

	/* get on the bus */
	client->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (client->priv->bus == NULL) {
		g_warning ("Couldn't connect to system bus: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* connect to main interface */
	client->priv->proxy = dbus_g_proxy_new_for_name (client->priv->bus,
							 "org.freedesktop.UPower",
							 "/org/freedesktop/UPower",
							 "org.freedesktop.UPower");
	if (client->priv->proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}

	/* connect to properties interface */
	client->priv->prop_proxy = dbus_g_proxy_new_for_name (client->priv->bus,
							      "org.freedesktop.UPower",
							      "/org/freedesktop/UPower",
							      "org.freedesktop.DBus.Properties");
	if (client->priv->prop_proxy == NULL) {
		g_warning ("Couldn't connect to proxy");
		goto out;
	}

	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (client->priv->proxy, "DeviceChanged", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (client->priv->proxy, "Changed", G_TYPE_INVALID);

	/* all callbacks */
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceAdded",
				     G_CALLBACK (dkp_device_added_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceRemoved",
				     G_CALLBACK (dkp_device_removed_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceChanged",
				     G_CALLBACK (dkp_device_changed_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "Changed",
				     G_CALLBACK (dkp_client_changed_cb), client, NULL);

	/* coldplug */
	devices = dkp_client_enumerate_devices_private (client, NULL);
	if (devices == NULL)
		goto out;
	for (i=0; i<devices->len; i++) {
		object_path = (const gchar *) g_ptr_array_index (devices, i);
		dkp_client_add (client, object_path);
	}
out:
	return;
}

/**
 * dkp_client_finalize:
 * @object: The object to finalize
 **/
static void
dkp_client_finalize (GObject *object)
{
	DkpClient *client;

	g_return_if_fail (DKP_IS_CLIENT (object));

	client = DKP_CLIENT (object);

	g_hash_table_destroy (client->priv->hash);

	if (client->priv->bus)
		dbus_g_connection_unref (client->priv->bus);

	if (client->priv->proxy != NULL)
		g_object_unref (client->priv->proxy);

	if (client->priv->prop_proxy != NULL)
		g_object_unref (client->priv->prop_proxy);

	g_free (client->priv->daemon_version);

	G_OBJECT_CLASS (dkp_client_parent_class)->finalize (object);
}

/**
 * dkp_client_new:
 *
 * Return value: a new DkpClient object.
 **/
DkpClient *
dkp_client_new (void)
{
	if (dkp_client_object != NULL) {
		g_object_ref (dkp_client_object);
	} else {
		dkp_client_object = g_object_new (DKP_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (dkp_client_object, &dkp_client_object);
	}
	return DKP_CLIENT (dkp_client_object);
}

