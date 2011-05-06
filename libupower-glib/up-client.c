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
 * SECTION:up-client
 * @short_description: Main client object for accessing the UPower daemon
 *
 * A helper GObject to use for accessing UPower information, and to be notified
 * when it is changed.
 *
 * See also: #UpDevice
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "up-client.h"
#include "up-device.h"

static void	up_client_class_init	(UpClientClass	*klass);
static void	up_client_init		(UpClient	*client);
static void	up_client_finalize	(GObject	*object);

#define UP_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_CLIENT, UpClientPrivate))

/**
 * UpClientPrivate:
 *
 * Private #UpClient data
 **/
struct _UpClientPrivate
{
	DBusGConnection		*bus;
	DBusGProxy		*proxy;
	DBusGProxy		*prop_proxy;
	GPtrArray		*array;
	gboolean		 have_properties;
	gchar			*daemon_version;
	gboolean		 can_suspend;
	gboolean		 can_hibernate;
	gboolean		 lid_is_closed;
	gboolean		 on_battery;
	gboolean		 on_low_battery;
	gboolean		 lid_is_present;
	gboolean		 done_enumerate;
};

enum {
	UP_CLIENT_DEVICE_ADDED,
	UP_CLIENT_DEVICE_CHANGED,
	UP_CLIENT_DEVICE_REMOVED,
	UP_CLIENT_CHANGED,
	UP_CLIENT_LAST_SIGNAL
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

static guint signals [UP_CLIENT_LAST_SIGNAL] = { 0 };
static gpointer up_client_object = NULL;

G_DEFINE_TYPE (UpClient, up_client, G_TYPE_OBJECT)

/*
 * up_client_get_device:
 */
static UpDevice *
up_client_get_device (UpClient *client, const gchar *object_path)
{
	guint i;
	const gchar *object_path_tmp;
	UpDevice *device;
	UpClientPrivate *priv = client->priv;

	for (i=0; i<priv->array->len; i++) {
		device = g_ptr_array_index (priv->array, i);
		object_path_tmp = up_device_get_object_path (device);
		if (g_strcmp0 (object_path_tmp, object_path) == 0)
			return device;
	}
	return NULL;
}

/**
 * up_client_get_devices:
 * @client: a #UpClient instance.
 *
 * Get a copy of the device objects.
 * You must have called up_client_enumerate_devices_sync() before calling this
 * function.
 *
 * Return value: an array of #UpDevice objects, free with g_ptr_array_unref()
 *
 * Since: 0.9.0
 **/
GPtrArray *
up_client_get_devices (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), NULL);
	g_return_val_if_fail (client->priv->done_enumerate, NULL);
	return g_ptr_array_ref (client->priv->array);
}

/*
 * up_client_get_devices_private:
 */
static GPtrArray *
up_client_get_devices_private (UpClient *client, GError **error)
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
 * up_client_suspend_sync:
 * @client: a #UpClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Puts the computer into a low power state, but state is not preserved if the
 * power is lost.
 *
 * NOTE: The system is still consuming a small amount of power
 *
 * Return value: TRUE if system suspended okay, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_suspend_sync (UpClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
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
 * up_client_hibernate_sync:
 * @client: a #UpClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError.
 *
 * Puts the computer into a low power state, where state is preserved if the
 * power is lost.
 *
 * Return value: TRUE if system suspended okay, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_hibernate_sync (UpClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
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
 * up_client_about_to_sleep_sync:
 * @client: a #UpClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Tells UPower that we are soon to reqest either Suspend() or Hibernate()
 * and that session and system components should be notified of this.
 *
 * Return value: TRUE if system suspended okay, FALSE other wise.
 *
 * Since: 0.9.1
 **/
gboolean
up_client_about_to_sleep_sync (UpClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;

	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (client->priv->proxy != NULL, FALSE);

	ret = dbus_g_proxy_call (client->priv->proxy, "AboutToSleep", &error_local,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		/* DBus might time out, which is okay */
		if (g_error_matches (error_local, DBUS_GERROR, DBUS_GERROR_NO_REPLY)) {
			g_debug ("DBUS timed out, but recovering");
			ret = TRUE;
			goto out;
		}

		/* an actual error */
		g_warning ("Couldn't sent that we were about to sleep: %s", error_local->message);
		g_set_error (error, 1, 0, "%s", error_local->message);
	}
out:
	if (error_local != NULL)
		g_error_free (error_local);
	return ret;
}

/**
 * up_client_get_properties_sync:
 * @client: a #UpClient instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Get all the properties from UPower daemon.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_get_properties_sync (UpClient *client, GCancellable *cancellable, GError **error)
{
	gboolean ret = TRUE;
	gboolean allowed = FALSE;
	GHashTable *props;
	GValue *value;

	props = NULL;

	if (client->priv->have_properties)
		goto out;
	if (!client->priv->prop_proxy)
		goto out;

	error = NULL;
	ret = dbus_g_proxy_call (client->priv->prop_proxy, "GetAll", error,
				 G_TYPE_STRING, "org.freedesktop.UPower",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props,
				 G_TYPE_INVALID);
	if (!ret)
		goto out;

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
	
	ret = dbus_g_proxy_call (client->priv->proxy, "SuspendAllowed", error,
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
	ret = dbus_g_proxy_call (client->priv->proxy, "HibernateAllowed", error,
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
	return ret;
}

/**
 * up_client_get_daemon_version:
 * @client: a #UpClient instance.
 *
 * Get UPower daemon version.
 *
 * Return value: string containing the daemon version, e.g. 008
 *
 * Since: 0.9.0
 **/
const gchar *
up_client_get_daemon_version (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), NULL);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->daemon_version;
}

/**
 * up_client_get_can_hibernate:
 * @client: a #UpClient instance.
 *
 * Get whether the system is able to hibernate.
 *
 * Return value: TRUE if system can hibernate, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_get_can_hibernate (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->can_hibernate;
}

/**
 * up_client_get_lid_is_closed:
 * @client: a #UpClient instance.
 *
 * Get whether the laptop lid is closed.
 *
 * Return value: %TRUE if lid is closed or %FALSE otherwise.
 *
 * Since: 0.9.0
 */
gboolean
up_client_get_lid_is_closed (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->lid_is_closed;
}

/**
 * up_client_get_lid_is_present:
 * @client: a #UpClient instance.
 *
 * Get whether a laptop lid is present on this machine.
 *
 * Return value: %TRUE if the machine has a laptop lid
 *
 * Since: 0.9.2
 */
gboolean
up_client_get_lid_is_present (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->lid_is_present;
}

/**
 * up_client_get_can_suspend:
 * @client: a #UpClient instance.
 *
 * Get whether the system is able to suspend.
 *
 * Return value: TRUE if system can suspend, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_get_can_suspend (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->can_suspend;
}

/**
 * up_client_get_on_battery:
 * @client: a #UpClient instance.
 *
 * Get whether the system is running on battery power.
 *
 * Return value: TRUE if the system is currently running on battery, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_get_on_battery (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->on_battery;
}

/**
 * up_client_get_on_low_battery:
 * @client: a #UpClient instance.
 *
 * Get whether the system is running on low battery power.
 *
 * Return value: TRUE if the system is currently on low battery power, FALSE other wise.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_get_on_low_battery (UpClient *client)
{
	g_return_val_if_fail (UP_IS_CLIENT (client), FALSE);
	up_client_get_properties_sync (client, NULL, NULL);
	return client->priv->on_low_battery;
}

/*
 * up_client_add:
 */
static void
up_client_add (UpClient *client, const gchar *object_path)
{
	UpDevice *device = NULL;
	UpDevice *device_tmp;
	gboolean ret;

	/* check existing list for this object path */
	device_tmp = up_client_get_device (client, object_path);
	if (device_tmp != NULL) {
		g_warning ("already added: %s", object_path);
		goto out;
	}

	/* create new device */
	device = up_device_new ();
	ret = up_device_set_object_path_sync (device, object_path, NULL, NULL);
	if (!ret)
		goto out;

	/* add to array */
	g_ptr_array_add (client->priv->array, g_object_ref (device));
	g_signal_emit (client, signals [UP_CLIENT_DEVICE_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
}

/*
 * up_client_added_cb:
 */
static void
up_device_added_cb (DBusGProxy *proxy, const gchar *object_path, UpClient *client)
{
	up_client_add (client, object_path);
}

/*
 * up_client_changed_cb:
 */
static void
up_device_changed_cb (DBusGProxy *proxy, const gchar *object_path, UpClient *client)
{
	UpDevice *device;
	device = up_client_get_device (client, object_path);
	if (device != NULL)
		g_signal_emit (client, signals [UP_CLIENT_DEVICE_CHANGED], 0, device);
}

/*
 * up_client_removed_cb:
 */
static void
up_device_removed_cb (DBusGProxy *proxy, const gchar *object_path, UpClient *client)
{
	UpDevice *device;
	device = up_client_get_device (client, object_path);
	if (device != NULL) {
		g_signal_emit (client, signals [UP_CLIENT_DEVICE_REMOVED], 0, device);
		g_ptr_array_remove (client->priv->array, device);
	}
}

/*
 * up_client_changed_cb:
 */
static void
up_client_changed_cb (DBusGProxy *proxy, UpClient *client)
{
	client->priv->have_properties = FALSE;
	g_signal_emit (client, signals [UP_CLIENT_CHANGED], 0);
}

static void
up_client_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	UpClient *client;
	client = UP_CLIENT (object);

	up_client_get_properties_sync (client, NULL, NULL);

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

/*
 * up_client_class_init:
 * @klass: The UpClientClass
 */
static void
up_client_class_init (UpClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = up_client_get_property;
	object_class->finalize = up_client_finalize;

	/**
	 * UpClient:daemon-version:
	 *
	 * The daemon version.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      "Daemon version",
							      NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * UpClient:can-suspend:
	 *
	 * If the computer can suspend.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_CAN_SUSPEND,
					 g_param_spec_boolean ("can-suspend",
							       "If the computer can suspend",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));
	/**
	 * UpClient:can-hibernate:
	 *
	 * If the computer can hibernate.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_CAN_HIBERNATE,
					 g_param_spec_boolean ("can-hibernate",
							       "If the computer can hibernate",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));
	/**
	 * UpClient:on-battery:
	 *
	 * If the computer is on battery power.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_ON_BATTERY,
					 g_param_spec_boolean ("on-battery",
							       "If the computer is on battery power",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));
	/**
	 * UpClient:on-low-battery:
	 *
	 * If the computer is on low battery power.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_ON_LOW_BATTERY,
					 g_param_spec_boolean ("on-low-battery",
							       "If the computer is on low battery power",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));
	/**
	 * UpClient:lid-is-closed:
	 *
	 * If the laptop lid is closed.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_LID_IS_CLOSED,
					 g_param_spec_boolean ("lid-is-closed",
							       "If the laptop lid is closed",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));
	/**
	 * UpClient:lid-is-present:
	 *
	 * If a laptop lid is present.
	 *
	 * Since: 0.9.0
	 */
	g_object_class_install_property (object_class,
					 PROP_LID_IS_PRESENT,
					 g_param_spec_boolean ("lid-is-present",
							       "If a laptop lid is present",
							       NULL,
							       FALSE,
							       G_PARAM_READABLE));

	/**
	 * UpClient::device-added:
	 * @client: the #UpClient instance that emitted the signal
	 * @device: the #UpDevice that was added.
	 *
	 * The ::device-added signal is emitted when a power device is added.
	 *
	 * Since: 0.9.0
	 **/
	signals [UP_CLIENT_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpClientClass, device_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, UP_TYPE_DEVICE);

	/**
	 * UpClient::device-removed:
	 * @client: the #UpClient instance that emitted the signal
	 * @device: the #UpDevice that was removed.
	 *
	 * The ::device-added signal is emitted when a power device is removed.
	 *
	 * Since: 0.9.0
	 **/
	signals [UP_CLIENT_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpClientClass, device_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, UP_TYPE_DEVICE);

	/**
	 * UpClient::device-changed:
	 * @client: the #UpClient instance that emitted the signal
	 * @device: the #UpDevice that was changed.
	 *
	 * The ::device-changed signal is emitted when a power device is changed.
	 *
	 * Since: 0.9.0
	 **/
	signals [UP_CLIENT_DEVICE_CHANGED] =
		g_signal_new ("device-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpClientClass, device_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, UP_TYPE_DEVICE);

	/**
	 * UpClient::changed:
	 * @client: the #UpDevice instance that emitted the signal
	 *
	 * The ::changed signal is emitted when properties may have changed.
	 *
	 * Since: 0.9.0
	 **/
	signals [UP_CLIENT_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (UpClientPrivate));
}

/**
 * up_client_enumerate_devices_sync:
 * @client: a #UpClient instance.
 * @error: a #GError, or %NULL.
 *
 * Enumerates all the devices from the daemon.
 *
 * Return value: %TRUE for success, else %FALSE.
 *
 * Since: 0.9.0
 **/
gboolean
up_client_enumerate_devices_sync (UpClient *client, GCancellable *cancellable, GError **error)
{
	const gchar *object_path;
	GPtrArray *devices;
	guint i;
	gboolean ret = TRUE;

	/* already done */
	if (client->priv->done_enumerate)
		goto out;

	/* coldplug */
	devices = up_client_get_devices_private (client, error);
	if (devices == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i=0; i<devices->len; i++) {
		object_path = (const gchar *) g_ptr_array_index (devices, i);
		up_client_add (client, object_path);
	}

	/* only do this once per instance */
	client->priv->done_enumerate = TRUE;
out:
	return ret;
}

/*
 * up_client_init:
 * @client: This class instance
 */
static void
up_client_init (UpClient *client)
{
	GError *error = NULL;

	client->priv = UP_CLIENT_GET_PRIVATE (client);
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->have_properties = FALSE;
	client->priv->done_enumerate = FALSE;

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
				     G_CALLBACK (up_device_added_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceRemoved",
				     G_CALLBACK (up_device_removed_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "DeviceChanged",
				     G_CALLBACK (up_device_changed_cb), client, NULL);
	dbus_g_proxy_connect_signal (client->priv->proxy, "Changed",
				     G_CALLBACK (up_client_changed_cb), client, NULL);
out:
	return;
}

/*
 * up_client_finalize:
 */
static void
up_client_finalize (GObject *object)
{
	UpClient *client;

	g_return_if_fail (UP_IS_CLIENT (object));

	client = UP_CLIENT (object);

	g_ptr_array_unref (client->priv->array);

	if (client->priv->bus)
		dbus_g_connection_unref (client->priv->bus);

	if (client->priv->proxy != NULL)
		g_object_unref (client->priv->proxy);

	if (client->priv->prop_proxy != NULL)
		g_object_unref (client->priv->prop_proxy);

	g_free (client->priv->daemon_version);

	G_OBJECT_CLASS (up_client_parent_class)->finalize (object);
}

/**
 * up_client_new:
 *
 * Creates a new #UpClient object.
 *
 * Return value: a new UpClient object.
 *
 * Since: 0.9.0
 **/
UpClient *
up_client_new (void)
{
	if (up_client_object != NULL) {
		g_object_ref (up_client_object);
	} else {
		up_client_object = g_object_new (UP_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (up_client_object, &up_client_object);
	}
	return UP_CLIENT (up_client_object);
}

