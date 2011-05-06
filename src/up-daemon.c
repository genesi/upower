/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <davidz@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "egg-debug.h"

#include "up-polkit.h"
#include "up-device-list.h"
#include "up-device.h"
#include "up-backend.h"
#include "up-daemon.h"

#include "up-daemon-glue.h"
#include "up-marshal.h"

enum
{
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

enum
{
	SIGNAL_DEVICE_ADDED,
	SIGNAL_DEVICE_REMOVED,
	SIGNAL_DEVICE_CHANGED,
	SIGNAL_CHANGED,
	SIGNAL_SLEEPING,
	SIGNAL_RESUMING,
	SIGNAL_LAST,
};

static guint signals[SIGNAL_LAST] = { 0 };

struct UpDaemonPrivate
{
	DBusGConnection		*connection;
	DBusGProxy		*proxy;
	UpPolkit		*polkit;
	UpBackend		*backend;
	UpDeviceList		*power_devices;
	gboolean		 on_battery;
	gboolean		 on_low_battery;
	gboolean		 lid_is_closed;
	gboolean		 lid_is_present;
	gboolean		 kernel_can_suspend;
	gboolean		 kernel_can_hibernate;
	gboolean		 hibernate_has_encrypted_swap;
	gboolean		 during_coldplug;
	gboolean		 sent_sleeping_signal;
	guint			 battery_poll_id;
	guint			 battery_poll_count;
	GTimer			*about_to_sleep_timer;
	guint			 about_to_sleep_id;
	guint			 conf_sleep_timeout;
	gboolean		 conf_allow_hibernate_encrypted_swap;
};

static void	up_daemon_finalize		(GObject	*object);
static gboolean	up_daemon_get_on_battery_local	(UpDaemon	*daemon);
static gboolean	up_daemon_get_on_low_battery_local (UpDaemon	*daemon);
static gboolean	up_daemon_get_on_ac_local 	(UpDaemon	*daemon);

G_DEFINE_TYPE (UpDaemon, up_daemon, G_TYPE_OBJECT)

#define UP_DAEMON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_DAEMON, UpDaemonPrivate))

/* if using more memory compared to usable swap, disable hibernate */
/* Native Linux suspend-to-disk does not use compression, and needs 2 KB of
 * page meta information for each MB of active memory. Add some error margin
 * here, though. */
#define UP_DAEMON_SWAP_WATERLINE 			98.f /* % */

/* refresh all the devices after this much time when on-battery has changed */
#define UP_DAEMON_ON_BATTERY_REFRESH_DEVICES_DELAY	1 /* seconds */
#define UP_DAEMON_POLL_BATTERY_NUMBER_TIMES		5

/**
 * up_daemon_get_on_battery_local:
 *
 * As soon as _any_ battery goes discharging, this is true
 **/
static gboolean
up_daemon_get_on_battery_local (UpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = FALSE;
	gboolean on_battery;
	UpDevice *device;
	GPtrArray *array;

	/* ask each device */
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		ret = up_device_get_on_battery (device, &on_battery);
		if (ret && on_battery) {
			result = TRUE;
			break;
		}
	}
	g_ptr_array_unref (array);
	return result;
}

/**
 * up_daemon_get_number_devices_of_type:
 **/
guint
up_daemon_get_number_devices_of_type (UpDaemon *daemon, UpDeviceKind type)
{
	guint i;
	UpDevice *device;
	GPtrArray *array;
	UpDeviceKind type_tmp;
	guint count = 0;

	/* ask each device */
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		g_object_get (device,
			      "type", &type_tmp,
			      NULL);
		if (type == type_tmp)
			count++;
	}
	g_ptr_array_unref (array);
	return count;
}

/**
 * up_daemon_get_on_low_battery_local:
 *
 * As soon as _all_ batteries are low, this is true
 **/
static gboolean
up_daemon_get_on_low_battery_local (UpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = TRUE;
	gboolean on_low_battery;
	UpDevice *device;
	GPtrArray *array;

	/* ask each device */
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		ret = up_device_get_low_battery (device, &on_low_battery);
		if (ret && !on_low_battery) {
			result = FALSE;
			break;
		}
	}
	g_ptr_array_unref (array);
	return result;
}

/**
 * up_daemon_get_on_ac_local:
 *
 * As soon as _any_ ac supply goes online, this is true
 **/
static gboolean
up_daemon_get_on_ac_local (UpDaemon *daemon)
{
	guint i;
	gboolean ret;
	gboolean result = FALSE;
	gboolean online;
	UpDevice *device;
	GPtrArray *array;

	/* ask each device */
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		ret = up_device_get_online (device, &online);
		if (ret && online) {
			result = TRUE;
			break;
		}
	}
	g_ptr_array_unref (array);
	return result;
}

/**
 * up_daemon_set_powersave:
 **/
static gboolean
up_daemon_set_powersave (UpDaemon *daemon, gboolean powersave)
{
	gboolean ret = FALSE;
	const gchar *command;
	GError *error = NULL;

	/* run script */
	command = up_backend_get_powersave_command (daemon->priv->backend, powersave);
	if (command == NULL) {
		egg_warning ("no powersave command set");
		goto out;
	}
	egg_debug ("excuting command: %s", command);
	ret = g_spawn_command_line_async (command, &error);
	if (!ret) {
		egg_warning ("failed to run script: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * up_daemon_refresh_battery_devices:
 **/
static gboolean
up_daemon_refresh_battery_devices (UpDaemon *daemon)
{
	guint i;
	GPtrArray *array;
	UpDevice *device;
	UpDeviceKind type;

	/* refresh all devices in array */
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		/* only refresh battery devices */
		g_object_get (device,
			      "type", &type,
			      NULL);
		if (type == UP_DEVICE_KIND_BATTERY)
			up_device_refresh_internal (device);
	}
	g_ptr_array_unref (array);

	return TRUE;
}

/**
 * up_daemon_enumerate_devices:
 **/
gboolean
up_daemon_enumerate_devices (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	guint i;
	GPtrArray *array;
	GPtrArray *object_paths;
	UpDevice *device;

	/* build a pointer array of the object paths */
	object_paths = g_ptr_array_new_with_free_func (g_free);
	array = up_device_list_get_array (daemon->priv->power_devices);
	for (i=0; i<array->len; i++) {
		device = (UpDevice *) g_ptr_array_index (array, i);
		g_ptr_array_add (object_paths, g_strdup (up_device_get_object_path (device)));
	}
	g_ptr_array_unref (array);

	/* return it on the bus */
	dbus_g_method_return (context, object_paths);

	/* free */
	g_ptr_array_unref (object_paths);
	return TRUE;
}

/**
 * up_daemon_about_to_sleep:
 **/
gboolean
up_daemon_about_to_sleep (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	PolkitSubject *subject = NULL;
	GError *error;
	UpDaemonPrivate *priv = daemon->priv;

	/* already requested */
	if (priv->about_to_sleep_id != 0) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Sleep has already been requested and is pending");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	subject = up_polkit_get_subject (priv->polkit, context);
	if (subject == NULL)
		goto out;

	/* TODO: use another PolicyKit context? */
	if (!up_polkit_check_auth (priv->polkit, subject, "org.freedesktop.upower.suspend", context))
		goto out;

	/* we've told the clients we're going down */
	egg_debug ("emitting sleeping");
	g_signal_emit (daemon, signals[SIGNAL_SLEEPING], 0);
	g_timer_start (priv->about_to_sleep_timer);
	daemon->priv->sent_sleeping_signal = TRUE;

	dbus_g_method_return (context, NULL);
out:
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/* temp object for deferred callback */
typedef struct {
	UpDaemon		*daemon;
	DBusGMethodInvocation	*context;
	gchar			*command;
} UpDaemonDeferredSleep;

/**
 * up_daemon_deferred_sleep_cb:
 **/
static gboolean
up_daemon_deferred_sleep_cb (UpDaemonDeferredSleep *sleep)
{
	GError *error;
	GError *error_local = NULL;
	gchar *stdout = NULL;
	gchar *stderr = NULL;
	gboolean ret;
	UpDaemon *daemon = sleep->daemon;
	UpDaemonPrivate *priv = daemon->priv;

	/* run the command */
	ret = g_spawn_command_line_sync (sleep->command, &stdout, &stderr, NULL, &error_local);
	if (!ret) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Failed to spawn: %s, stdout:%s, stderr:%s", error_local->message, stdout, stderr);
		g_error_free (error_local);
		dbus_g_method_return_error (sleep->context, error);
		goto out;
	}

	/* emit signal for session components */
	egg_debug ("emitting resuming");
	g_signal_emit (daemon, signals[SIGNAL_RESUMING], 0);

	/* reset the about-to-sleep logic */
	g_timer_reset (priv->about_to_sleep_timer);
	g_timer_stop (priv->about_to_sleep_timer);

	/* actually return from the DBus call now */
	dbus_g_method_return (sleep->context, NULL);

out:
	/* clear timer */
	priv->about_to_sleep_id = 0;
	priv->sent_sleeping_signal = FALSE;

	g_free (stdout);
	g_free (stderr);

	/* delete temp object */
	g_object_unref (sleep->daemon);
	g_free (sleep->command);
	g_free (sleep);

	return FALSE;
}

/**
 * up_daemon_deferred_sleep:
 **/
static void
up_daemon_deferred_sleep (UpDaemon *daemon, const gchar *command, DBusGMethodInvocation *context)
{
	UpDaemonDeferredSleep *sleep;
	UpDaemonPrivate *priv = daemon->priv;
	gfloat elapsed;

	/* create callback object */
	sleep = g_new0 (UpDaemonDeferredSleep, 1);
	sleep->daemon = g_object_ref (daemon);
	sleep->context = context;
	sleep->command = g_strdup (command);

	/* we didn't use AboutToSleep() so send the signal for clients now */
	if (!priv->sent_sleeping_signal) {
		egg_debug ("no AboutToSleep(), so emitting ::Sleeping()");
		g_signal_emit (daemon, signals[SIGNAL_SLEEPING], 0);
		priv->about_to_sleep_id = g_timeout_add (priv->conf_sleep_timeout,
							 (GSourceFunc) up_daemon_deferred_sleep_cb, sleep);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (priv->about_to_sleep_id, "[UpDaemon] about-to-sleep no signal");
#endif
		return;
	}

	/* about to sleep */
	elapsed = 1000.0f * g_timer_elapsed (priv->about_to_sleep_timer, NULL);
	egg_debug ("between AboutToSleep() and %s was %fms", sleep->command, elapsed);
	if (elapsed < priv->conf_sleep_timeout) {
		/* we have to wait for the difference in time */
		priv->about_to_sleep_id = g_timeout_add (priv->conf_sleep_timeout - elapsed,
							 (GSourceFunc) up_daemon_deferred_sleep_cb, sleep);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (priv->about_to_sleep_id, "[UpDaemon] about-to-sleep less");
#endif
	} else {
		/* we can do this straight away */
		priv->about_to_sleep_id = g_idle_add ((GSourceFunc) up_daemon_deferred_sleep_cb, sleep);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (priv->about_to_sleep_id, "[UpDaemon] about-to-sleep more");
#endif
	}
}

/**
 * up_daemon_suspend:
 **/
gboolean
up_daemon_suspend (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	GError *error;
	PolkitSubject *subject = NULL;
	const gchar *command;
	UpDaemonPrivate *priv = daemon->priv;

	/* no kernel support */
	if (!priv->kernel_can_suspend) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "No kernel support");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	subject = up_polkit_get_subject (priv->polkit, context);
	if (subject == NULL)
		goto out;

	if (!up_polkit_check_auth (priv->polkit, subject, "org.freedesktop.upower.suspend", context))
		goto out;

	/* already requested */
	if (priv->about_to_sleep_id != 0) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Sleep has already been requested and is pending");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* do this deferred action */
	command = up_backend_get_suspend_command (priv->backend);
	up_daemon_deferred_sleep (daemon, command, context);
out:
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/**
 * up_daemon_suspend_allowed:
 **/
gboolean
up_daemon_suspend_allowed (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	gboolean ret;
	PolkitSubject *subject = NULL;
	UpDaemonPrivate *priv = daemon->priv;

	subject = up_polkit_get_subject (priv->polkit, context);
	if (subject == NULL)
		goto out;

	ret = up_polkit_is_allowed (priv->polkit, subject, "org.freedesktop.upower.suspend", context);
	dbus_g_method_return (context, ret);

out:
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/** 
 * up_daemon_check_hibernate_swap:
 *
 * Check current memory usage whether we have enough swap space for
 * hibernate.
 **/
static gboolean
up_daemon_check_hibernate_swap (UpDaemon *daemon)
{
	gfloat waterline;

	if (daemon->priv->kernel_can_hibernate) {
		waterline = up_backend_get_used_swap (daemon->priv->backend);
		if (waterline < UP_DAEMON_SWAP_WATERLINE) {
			egg_debug ("enough swap to for hibernate");
			return TRUE;
		} else {
			egg_debug ("not enough swap to hibernate");
			return FALSE;
		}
	}

	return FALSE;
}

/**
 * up_daemon_hibernate:
 **/
gboolean
up_daemon_hibernate (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	GError *error;
	PolkitSubject *subject = NULL;
	const gchar *command;
	UpDaemonPrivate *priv = daemon->priv;

	/* no kernel support */
	if (!priv->kernel_can_hibernate) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "No kernel support");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* enough swap? */
	if (!up_daemon_check_hibernate_swap (daemon)) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Not enough swap space");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* encrypted swap and no override? */
	if (priv->hibernate_has_encrypted_swap &&
	    !priv->conf_allow_hibernate_encrypted_swap) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Swap space is encrypted, use AllowHibernateEncryptedSwap to override");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	subject = up_polkit_get_subject (priv->polkit, context);
	if (subject == NULL)
		goto out;

	if (!up_polkit_check_auth (priv->polkit, subject, "org.freedesktop.upower.hibernate", context))
		goto out;

	/* already requested */
	if (priv->about_to_sleep_id != 0) {
		error = g_error_new (UP_DAEMON_ERROR,
				     UP_DAEMON_ERROR_GENERAL,
				     "Sleep has already been requested and is pending");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* do this deferred action */
	command = up_backend_get_hibernate_command (priv->backend);
	up_daemon_deferred_sleep (daemon, command, context);
out:
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/**
 * up_daemon_hibernate_allowed:
 **/
gboolean
up_daemon_hibernate_allowed (UpDaemon *daemon, DBusGMethodInvocation *context)
{
	gboolean ret;
	PolkitSubject *subject = NULL;
	UpDaemonPrivate *priv = daemon->priv;

	subject = up_polkit_get_subject (priv->polkit, context);
	if (subject == NULL)
		goto out;

	ret = up_polkit_is_allowed (priv->polkit, subject, "org.freedesktop.upower.hibernate", context);
	dbus_g_method_return (context, ret);

out:
	if (subject != NULL)
		g_object_unref (subject);
	return TRUE;
}

/**
 * up_daemon_register_power_daemon:
 **/
static gboolean
up_daemon_register_power_daemon (UpDaemon *daemon)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	UpDaemonPrivate *priv = daemon->priv;

	priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (priv->connection == NULL) {
		if (error != NULL) {
			g_critical ("error getting system bus: %s", error->message);
			g_error_free (error);
		}
		goto out;
	}

	/* connect to DBUS */
	priv->proxy = dbus_g_proxy_new_for_name (priv->connection,
						 DBUS_SERVICE_DBUS,
						 DBUS_PATH_DBUS,
						 DBUS_INTERFACE_DBUS);

	/* register GObject */
	dbus_g_connection_register_g_object (priv->connection,
					     "/org/freedesktop/UPower",
					     G_OBJECT (daemon));

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * up_daemon_startup:
 **/
gboolean
up_daemon_startup (UpDaemon *daemon)
{
	gboolean ret;
	gboolean on_battery;
	gboolean on_low_battery;
	UpDaemonPrivate *priv = daemon->priv;

	/* register on bus */
	ret = up_daemon_register_power_daemon (daemon);
	if (!ret) {
		egg_warning ("failed to register");
		goto out;
	}

	/* stop signals and callbacks */
	egg_debug ("daemon now coldplug");
	g_object_freeze_notify (G_OBJECT(daemon));
	priv->during_coldplug = TRUE;

	/* coldplug backend backend */
	ret = up_backend_coldplug (priv->backend, daemon);
	if (!ret) {
		egg_warning ("failed to coldplug backend");
		goto out;
	}

	/* get battery state */
	on_battery = (up_daemon_get_on_battery_local (daemon) &&
		      !up_daemon_get_on_ac_local (daemon));
	on_low_battery = up_daemon_get_on_low_battery_local (daemon);
	up_daemon_set_on_battery (daemon, on_battery);
	up_daemon_set_on_low_battery (daemon, on_low_battery);

	/* start signals and callbacks */
	g_object_thaw_notify (G_OBJECT(daemon));
	priv->during_coldplug = FALSE;
	egg_debug ("daemon now not coldplug");

	/* set power policy */
	up_daemon_set_powersave (daemon, priv->on_battery);
out:
	return ret;
}

/**
 * up_daemon_get_device_list:
 **/
UpDeviceList *
up_daemon_get_device_list (UpDaemon *daemon)
{
	return g_object_ref (daemon->priv->power_devices);
}

/**
 * up_daemon_set_lid_is_closed:
 **/
void
up_daemon_set_lid_is_closed (UpDaemon *daemon, gboolean lid_is_closed)
{
	UpDaemonPrivate *priv = daemon->priv;
	egg_debug ("lid_is_closed = %s", lid_is_closed ? "yes" : "no");
	priv->lid_is_closed = lid_is_closed;
	g_object_notify (G_OBJECT (daemon), "lid-is-closed");
}

/**
 * up_daemon_set_lid_is_present:
 **/
void
up_daemon_set_lid_is_present (UpDaemon *daemon, gboolean lid_is_present)
{
	UpDaemonPrivate *priv = daemon->priv;
	egg_debug ("lid_is_present = %s", lid_is_present ? "yes" : "no");
	priv->lid_is_present = lid_is_present;
	g_object_notify (G_OBJECT (daemon), "lid-is-present");
}

/**
 * up_daemon_set_on_battery:
 **/
void
up_daemon_set_on_battery (UpDaemon *daemon, gboolean on_battery)
{
	UpDaemonPrivate *priv = daemon->priv;
	egg_debug ("on_battery = %s", on_battery ? "yes" : "no");
	priv->on_battery = on_battery;
	g_object_notify (G_OBJECT (daemon), "on-battery");
}

/**
 * up_daemon_set_on_low_battery:
 **/
void
up_daemon_set_on_low_battery (UpDaemon *daemon, gboolean on_low_battery)
{
	UpDaemonPrivate *priv = daemon->priv;
	egg_debug ("on_low_battery = %s", on_low_battery ? "yes" : "no");
	priv->on_low_battery = on_low_battery;
	g_object_notify (G_OBJECT (daemon), "on-low-battery");
}

/**
 * up_daemon_refresh_battery_devices_cb:
 **/
static gboolean
up_daemon_refresh_battery_devices_cb (UpDaemon *daemon)
{
	UpDaemonPrivate *priv = daemon->priv;

	/* no more left to do? */
	if (priv->battery_poll_count-- == 0) {
		priv->battery_poll_id = 0;
		return FALSE;
	}

	egg_debug ("doing the delayed refresh (%i)", priv->battery_poll_count);
	up_daemon_refresh_battery_devices (daemon);

	/* keep going until none left to do */
	return TRUE;
}

/**
 * up_daemon_poll_battery_devices_for_a_little_bit:
 **/
static void
up_daemon_poll_battery_devices_for_a_little_bit (UpDaemon *daemon)
{
	UpDaemonPrivate *priv = daemon->priv;

	priv->battery_poll_count = UP_DAEMON_POLL_BATTERY_NUMBER_TIMES;

	/* already polling */
	if (priv->battery_poll_id != 0)
		return;
	priv->battery_poll_id =
		g_timeout_add_seconds (UP_DAEMON_ON_BATTERY_REFRESH_DEVICES_DELAY,
				       (GSourceFunc) up_daemon_refresh_battery_devices_cb, daemon);
#if GLIB_CHECK_VERSION(2,25,8)
	g_source_set_name_by_id (priv->battery_poll_id, "[UpDaemon] poll batteries for AC event");
#endif
}

/**
 * up_daemon_device_changed_cb:
 **/
static void
up_daemon_device_changed_cb (UpDevice *device, UpDaemon *daemon)
{
	const gchar *object_path;
	UpDeviceKind type;
	gboolean ret;
	UpDaemonPrivate *priv = daemon->priv;

	g_return_if_fail (UP_IS_DAEMON (daemon));
	g_return_if_fail (UP_IS_DEVICE (device));

	/* refresh battery devices when AC state changes */
	g_object_get (device,
		      "type", &type,
		      NULL);
	if (type == UP_DEVICE_KIND_LINE_POWER) {
		/* refresh now, and again in a little while */
		up_daemon_refresh_battery_devices (daemon);
		up_daemon_poll_battery_devices_for_a_little_bit (daemon);
	}

	/* second, check if the on_battery and on_low_battery state has changed */
	ret = (up_daemon_get_on_battery_local (daemon) && !up_daemon_get_on_ac_local (daemon));
	if (ret != priv->on_battery) {
		up_daemon_set_on_battery (daemon, ret);

		/* set power policy */
		up_daemon_set_powersave (daemon, ret);
	}
	ret = up_daemon_get_on_low_battery_local (daemon);
	if (ret != priv->on_low_battery)
		up_daemon_set_on_low_battery (daemon, ret);

	/* emit */
	if (!priv->during_coldplug) {
		object_path = up_device_get_object_path (device);
		egg_debug ("emitting device-changed: %s", object_path);

		/* don't crash the session */
		if (object_path == NULL) {
			egg_warning ("INTERNAL STATE CORRUPT: not sending NULL, device:%p", device);
			return;
		}
		g_signal_emit (daemon, signals[SIGNAL_DEVICE_CHANGED], 0, object_path);
	}
}

/**
 * up_daemon_device_added_cb:
 **/
static void
up_daemon_device_added_cb (UpBackend *backend, GObject *native, UpDevice *device, UpDaemon *daemon)
{
	UpDeviceKind type;
	const gchar *object_path;
	UpDaemonPrivate *priv = daemon->priv;

	g_return_if_fail (UP_IS_DAEMON (daemon));
	g_return_if_fail (UP_IS_DEVICE (device));
	g_return_if_fail (G_IS_OBJECT (native));

	/* add to device list */
	up_device_list_insert (priv->power_devices, native, G_OBJECT (device));

	/* connect, so we get changes */
	g_signal_connect (device, "changed",
			  G_CALLBACK (up_daemon_device_changed_cb), daemon);

	/* refresh after a short delay */
	g_object_get (device,
		      "type", &type,
		      NULL);
	if (type == UP_DEVICE_KIND_BATTERY)
		up_daemon_poll_battery_devices_for_a_little_bit (daemon);

	/* emit */
	if (!priv->during_coldplug) {
		object_path = up_device_get_object_path (device);
		egg_debug ("emitting added: %s (during coldplug %i)", object_path, priv->during_coldplug);

		/* don't crash the session */
		if (object_path == NULL) {
			egg_warning ("INTERNAL STATE CORRUPT: not sending NULL, native:%p, device:%p", native, device);
			return;
		}
		g_signal_emit (daemon, signals[SIGNAL_DEVICE_ADDED], 0, object_path);
	}
}

/**
 * up_daemon_device_removed_cb:
 **/
static void
up_daemon_device_removed_cb (UpBackend *backend, GObject *native, UpDevice *device, UpDaemon *daemon)
{
	UpDeviceKind type;
	const gchar *object_path;
	UpDaemonPrivate *priv = daemon->priv;

	g_return_if_fail (UP_IS_DAEMON (daemon));
	g_return_if_fail (UP_IS_DEVICE (device));
	g_return_if_fail (G_IS_OBJECT (native));

	/* remove from list */
	up_device_list_remove (priv->power_devices, G_OBJECT(device));

	/* refresh after a short delay */
	g_object_get (device,
		      "type", &type,
		      NULL);
	if (type == UP_DEVICE_KIND_BATTERY)
		up_daemon_poll_battery_devices_for_a_little_bit (daemon);

	/* emit */
	if (!priv->during_coldplug) {
		object_path = up_device_get_object_path (device);
		egg_debug ("emitting device-removed: %s", object_path);

		/* don't crash the session */
		if (object_path == NULL) {
			egg_warning ("INTERNAL STATE CORRUPT: not sending NULL, native:%p, device:%p", native, device);
			return;
		}
		g_signal_emit (daemon, signals[SIGNAL_DEVICE_REMOVED], 0, object_path);
	}

	/* finalise the object */
	g_object_unref (device);
}

/**
 * up_daemon_properties_changed_cb:
 **/
static void
up_daemon_properties_changed_cb (GObject *object, GParamSpec *pspec, UpDaemon *daemon)
{
	g_return_if_fail (UP_IS_DAEMON (daemon));

	/* emit */
	if (!daemon->priv->during_coldplug) {
		egg_debug ("emitting changed");
		g_signal_emit (daemon, signals[SIGNAL_CHANGED], 0);
	}
}

/**
 * up_daemon_init:
 **/
static void
up_daemon_init (UpDaemon *daemon)
{
	gboolean ret;
	GError *error = NULL;
	GKeyFile *file;

	daemon->priv = UP_DAEMON_GET_PRIVATE (daemon);
	daemon->priv->polkit = up_polkit_new ();
	daemon->priv->lid_is_present = FALSE;
	daemon->priv->lid_is_closed = FALSE;
	daemon->priv->kernel_can_suspend = FALSE;
	daemon->priv->kernel_can_hibernate = FALSE;
	daemon->priv->hibernate_has_encrypted_swap = FALSE;
	daemon->priv->power_devices = up_device_list_new ();
	daemon->priv->on_battery = FALSE;
	daemon->priv->on_low_battery = FALSE;
	daemon->priv->during_coldplug = FALSE;
	daemon->priv->sent_sleeping_signal = FALSE;
	daemon->priv->battery_poll_id = 0;
	daemon->priv->battery_poll_count = 0;
	daemon->priv->about_to_sleep_id = 0;
	daemon->priv->conf_sleep_timeout = 1000;
	daemon->priv->conf_allow_hibernate_encrypted_swap = FALSE;

	/* load some values from the config file */
	file = g_key_file_new ();
	ret = g_key_file_load_from_file (file, PACKAGE_SYSCONF_DIR "/UPower/UPower.conf", G_KEY_FILE_NONE, &error);
	if (ret) {
		daemon->priv->conf_sleep_timeout =
			g_key_file_get_integer (file, "UPower", "SleepTimeout", NULL);
		daemon->priv->conf_allow_hibernate_encrypted_swap =
			g_key_file_get_boolean (file, "UPower", "AllowHibernateEncryptedSwap", NULL);
	} else {
		egg_warning ("failed to load config file: %s", error->message);
		g_error_free (error);
	}
	g_key_file_free (file);

	daemon->priv->backend = up_backend_new ();
	g_signal_connect (daemon->priv->backend, "device-added",
			  G_CALLBACK (up_daemon_device_added_cb), daemon);
	g_signal_connect (daemon->priv->backend, "device-removed",
			  G_CALLBACK (up_daemon_device_removed_cb), daemon);

	/* use a timer for the about-to-sleep logic */
	daemon->priv->about_to_sleep_timer = g_timer_new ();
	g_timer_stop (daemon->priv->about_to_sleep_timer);

	/* watch when these properties change */
	g_signal_connect (daemon, "notify::lid-is-present",
			  G_CALLBACK (up_daemon_properties_changed_cb), daemon);
	g_signal_connect (daemon, "notify::lid-is-closed",
			  G_CALLBACK (up_daemon_properties_changed_cb), daemon);
	g_signal_connect (daemon, "notify::on-battery",
			  G_CALLBACK (up_daemon_properties_changed_cb), daemon);
	g_signal_connect (daemon, "notify::on-low-battery",
			  G_CALLBACK (up_daemon_properties_changed_cb), daemon);

	/* check if we have support */
	daemon->priv->kernel_can_suspend = up_backend_kernel_can_suspend (daemon->priv->backend);
	daemon->priv->kernel_can_hibernate = up_backend_kernel_can_hibernate (daemon->priv->backend);

	/* is the swap usable? */
	if (daemon->priv->kernel_can_hibernate)
		daemon->priv->hibernate_has_encrypted_swap = up_backend_has_encrypted_swap (daemon->priv->backend);
}

/**
 * up_daemon_error_quark:
 **/
GQuark
up_daemon_error_quark (void)
{
	static GQuark ret = 0;
	if (ret == 0)
		ret = g_quark_from_static_string ("up_daemon_error");
	return ret;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
/**
 * up_daemon_error_get_type:
 **/
GType
up_daemon_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY (UP_DAEMON_ERROR_GENERAL, "GeneralError"),
			ENUM_ENTRY (UP_DAEMON_ERROR_NOT_SUPPORTED, "NotSupported"),
			ENUM_ENTRY (UP_DAEMON_ERROR_NO_SUCH_DEVICE, "NoSuchDevice"),
			{ 0, 0, 0 }
		};
		g_assert (UP_DAEMON_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
		etype = g_enum_register_static ("UpDaemonError", values);
	}
	return etype;
}

/**
 * up_daemon_get_property:
 **/
static void
up_daemon_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	UpDaemon *daemon = UP_DAEMON (object);
	UpDaemonPrivate *priv = daemon->priv;

	switch (prop_id) {
	case PROP_DAEMON_VERSION:
		g_value_set_string (value, PACKAGE_VERSION);
		break;
	case PROP_CAN_SUSPEND:
		g_value_set_boolean (value, priv->kernel_can_suspend);
		break;
	case PROP_CAN_HIBERNATE:
		g_value_set_boolean (value, (priv->kernel_can_hibernate &&
					     up_daemon_check_hibernate_swap (daemon) &&
					     (!priv->hibernate_has_encrypted_swap ||
					      priv->conf_allow_hibernate_encrypted_swap)));
		break;
	case PROP_ON_BATTERY:
		g_value_set_boolean (value, priv->on_battery);
		break;
	case PROP_ON_LOW_BATTERY:
		g_value_set_boolean (value, priv->on_battery && priv->on_low_battery);
		break;
	case PROP_LID_IS_CLOSED:
		g_value_set_boolean (value, priv->lid_is_closed);
		break;
	case PROP_LID_IS_PRESENT:
		g_value_set_boolean (value, priv->lid_is_present);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_daemon_set_property:
 **/
static void
up_daemon_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_daemon_class_init:
 **/
static void
up_daemon_class_init (UpDaemonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_daemon_finalize;
	object_class->get_property = up_daemon_get_property;
	object_class->set_property = up_daemon_set_property;

	g_type_class_add_private (klass, sizeof (UpDaemonPrivate));

	signals[SIGNAL_DEVICE_ADDED] =
		g_signal_new ("device-added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[SIGNAL_DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[SIGNAL_DEVICE_CHANGED] =
		g_signal_new ("device-changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[SIGNAL_SLEEPING] =
		g_signal_new ("sleeping",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[SIGNAL_RESUMING] =
		g_signal_new ("resuming",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_object_class_install_property (object_class,
					 PROP_DAEMON_VERSION,
					 g_param_spec_string ("daemon-version",
							      "Daemon Version",
							      "The version of the running daemon",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_PRESENT,
					 g_param_spec_boolean ("lid-is-present",
							       "Is a laptop",
							       "If this computer is probably a laptop",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_SUSPEND,
					 g_param_spec_boolean ("can-suspend",
							       "Can Suspend",
							       "Whether the system can suspend",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_HIBERNATE,
					 g_param_spec_boolean ("can-hibernate",
							       "Can Hibernate",
							       "Whether the system can hibernate",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_BATTERY,
					 g_param_spec_boolean ("on-battery",
							       "On Battery",
							       "Whether the system is running on battery",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ON_LOW_BATTERY,
					 g_param_spec_boolean ("on-low-battery",
							       "On Low Battery",
							       "Whether the system is running on battery and if the battery is critically low",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LID_IS_CLOSED,
					 g_param_spec_boolean ("lid-is-closed",
							       "Laptop lid is closed",
							       "If the laptop lid is closed",
							       FALSE,
							       G_PARAM_READABLE));

	dbus_g_object_type_install_info (UP_TYPE_DAEMON, &dbus_glib_up_daemon_object_info);

	dbus_g_error_domain_register (UP_DAEMON_ERROR, NULL, UP_DAEMON_TYPE_ERROR);
}

/**
 * up_daemon_finalize:
 **/
static void
up_daemon_finalize (GObject *object)
{
	UpDaemon *daemon = UP_DAEMON (object);
	UpDaemonPrivate *priv = daemon->priv;

	if (priv->battery_poll_id != 0)
		g_source_remove (priv->battery_poll_id);

	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);
	if (priv->connection != NULL)
		dbus_g_connection_unref (priv->connection);
	g_object_unref (priv->power_devices);
	g_object_unref (priv->polkit);
	g_object_unref (priv->backend);
	g_timer_destroy (priv->about_to_sleep_timer);

	G_OBJECT_CLASS (up_daemon_parent_class)->finalize (object);
}

/**
 * up_daemon_new:
 **/
UpDaemon *
up_daemon_new (void)
{
	UpDaemon *daemon;
	daemon = UP_DAEMON (g_object_new (UP_TYPE_DAEMON, NULL));
	return daemon;
}

