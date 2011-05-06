/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 David Zeuthen <davidz@redhat.com>
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "egg-debug.h"

#include "up-native.h"
#include "up-device.h"
#include "up-history.h"
#include "up-history-item.h"
#include "up-stats-item.h"
#include "up-marshal.h"
#include "up-device-glue.h"

struct UpDevicePrivate
{
	gchar			*object_path;
	DBusGConnection		*system_bus_connection;
	DBusGProxy		*system_bus_proxy;
	UpDaemon		*daemon;
	UpHistory		*history;
	GObject			*native;
	gboolean		 has_ever_refresh;
	gboolean		 during_coldplug;

	/* properties */
	guint64			 update_time;
	gchar			*vendor;
	gchar			*model;
	gchar			*serial;
	gchar			*native_path;
	gboolean		 power_supply;
	gboolean		 online;
	gboolean		 is_present;
	gboolean		 is_rechargeable;
	gboolean		 has_history;
	gboolean		 has_statistics;
	UpDeviceKind		 type;
	UpDeviceState		 state;
	UpDeviceTechnology	 technology;
	gdouble			 capacity;		/* percent */
	gdouble			 energy;		/* Watt Hours */
	gdouble			 energy_empty;		/* Watt Hours */
	gdouble			 energy_full;		/* Watt Hours */
	gdouble			 energy_full_design;	/* Watt Hours */
	gdouble			 energy_rate;		/* Watts */
	gdouble			 voltage;		/* Volts */
	gint64			 time_to_empty;		/* seconds */
	gint64			 time_to_full;		/* seconds */
	gdouble			 percentage;		/* percent */
	gboolean		 recall_notice;
	gchar			*recall_vendor;
	gchar			*recall_url;
};

static gboolean	up_device_register_device	(UpDevice *device);

enum {
	PROP_0,
	PROP_NATIVE_PATH,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_UPDATE_TIME,
	PROP_TYPE,
	PROP_ONLINE,
	PROP_POWER_SUPPLY,
	PROP_CAPACITY,
	PROP_IS_PRESENT,
	PROP_IS_RECHARGEABLE,
	PROP_HAS_HISTORY,
	PROP_HAS_STATISTICS,
	PROP_STATE,
	PROP_ENERGY,
	PROP_ENERGY_EMPTY,
	PROP_ENERGY_FULL,
	PROP_ENERGY_FULL_DESIGN,
	PROP_ENERGY_RATE,
	PROP_VOLTAGE,
	PROP_TIME_TO_EMPTY,
	PROP_TIME_TO_FULL,
	PROP_PERCENTAGE,
	PROP_TECHNOLOGY,
	PROP_RECALL_NOTICE,
	PROP_RECALL_VENDOR,
	PROP_RECALL_URL,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST,
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (UpDevice, up_device, G_TYPE_OBJECT)
#define UP_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_DEVICE, UpDevicePrivate))
#define UP_DBUS_STRUCT_UINT_DOUBLE_UINT (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_UINT, G_TYPE_DOUBLE, G_TYPE_UINT, G_TYPE_INVALID))
#define UP_DBUS_STRUCT_DOUBLE_DOUBLE (dbus_g_type_get_struct ("GValueArray", \
	G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_INVALID))

/**
 * up_device_error_quark:
 **/
GQuark
up_device_error_quark (void)
{
	static GQuark ret = 0;

	if (ret == 0) {
		ret = g_quark_from_static_string ("up_device_error");
	}

	return ret;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

/**
 * up_device_error_get_type:
 **/
GType
up_device_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] =
			{
				ENUM_ENTRY (UP_DEVICE_ERROR_GENERAL, "GeneralError"),
				{ 0, 0, 0 }
			};
		g_assert (UP_DEVICE_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
		etype = g_enum_register_static ("UpDeviceError", values);
	}
	return etype;
}

/**
 * up_device_get_property:
 **/
static void
up_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	UpDevice *device = UP_DEVICE (object);
	switch (prop_id) {
	case PROP_NATIVE_PATH:
		g_value_set_string (value, device->priv->native_path);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, device->priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, device->priv->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, device->priv->serial);
		break;
	case PROP_UPDATE_TIME:
		g_value_set_uint64 (value, device->priv->update_time);
		break;
	case PROP_TYPE:
		g_value_set_uint (value, device->priv->type);
		break;
	case PROP_POWER_SUPPLY:
		g_value_set_boolean (value, device->priv->power_supply);
		break;
	case PROP_ONLINE:
		g_value_set_boolean (value, device->priv->online);
		break;
	case PROP_IS_PRESENT:
		g_value_set_boolean (value, device->priv->is_present);
		break;
	case PROP_IS_RECHARGEABLE:
		g_value_set_boolean (value, device->priv->is_rechargeable);
		break;
	case PROP_HAS_HISTORY:
		g_value_set_boolean (value, device->priv->has_history);
		break;
	case PROP_HAS_STATISTICS:
		g_value_set_boolean (value, device->priv->has_statistics);
		break;
	case PROP_STATE:
		g_value_set_uint (value, device->priv->state);
		break;
	case PROP_CAPACITY:
		g_value_set_double (value, device->priv->capacity);
		break;
	case PROP_ENERGY:
		g_value_set_double (value, device->priv->energy);
		break;
	case PROP_ENERGY_EMPTY:
		g_value_set_double (value, device->priv->energy_empty);
		break;
	case PROP_ENERGY_FULL:
		g_value_set_double (value, device->priv->energy_full);
		break;
	case PROP_ENERGY_FULL_DESIGN:
		g_value_set_double (value, device->priv->energy_full_design);
		break;
	case PROP_ENERGY_RATE:
		g_value_set_double (value, device->priv->energy_rate);
		break;
	case PROP_VOLTAGE:
		g_value_set_double (value, device->priv->voltage);
		break;
	case PROP_TIME_TO_EMPTY:
		g_value_set_int64 (value, device->priv->time_to_empty);
		break;
	case PROP_TIME_TO_FULL:
		g_value_set_int64 (value, device->priv->time_to_full);
		break;
	case PROP_PERCENTAGE:
		g_value_set_double (value, device->priv->percentage);
		break;
	case PROP_TECHNOLOGY:
		g_value_set_uint (value, device->priv->technology);
		break;
	case PROP_RECALL_NOTICE:
		g_value_set_boolean (value, device->priv->recall_notice);
		break;
	case PROP_RECALL_VENDOR:
		g_value_set_string (value, device->priv->recall_vendor);
		break;
	case PROP_RECALL_URL:
		g_value_set_string (value, device->priv->recall_url);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_device_set_property:
 **/
static void
up_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	UpDevice *device = UP_DEVICE (object);

	switch (prop_id) {
	case PROP_NATIVE_PATH:
		g_free (device->priv->native_path);
		device->priv->native_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_VENDOR:
		g_free (device->priv->vendor);
		device->priv->vendor = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (device->priv->model);
		device->priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_SERIAL:
		g_free (device->priv->serial);
		device->priv->serial = g_strdup (g_value_get_string (value));
		break;
	case PROP_UPDATE_TIME:
		device->priv->update_time = g_value_get_uint64 (value);
		break;
	case PROP_TYPE:
		device->priv->type = g_value_get_uint (value);
		break;
	case PROP_POWER_SUPPLY:
		device->priv->power_supply = g_value_get_boolean (value);
		break;
	case PROP_ONLINE:
		device->priv->online = g_value_get_boolean (value);
		break;
	case PROP_IS_PRESENT:
		device->priv->is_present = g_value_get_boolean (value);
		break;
	case PROP_IS_RECHARGEABLE:
		device->priv->is_rechargeable = g_value_get_boolean (value);
		break;
	case PROP_HAS_HISTORY:
		device->priv->has_history = g_value_get_boolean (value);
		break;
	case PROP_HAS_STATISTICS:
		device->priv->has_statistics = g_value_get_boolean (value);
		break;
	case PROP_STATE:
		device->priv->state = g_value_get_uint (value);
		break;
	case PROP_CAPACITY:
		device->priv->capacity = g_value_get_double (value);
		break;
	case PROP_ENERGY:
		device->priv->energy = g_value_get_double (value);
		break;
	case PROP_ENERGY_EMPTY:
		device->priv->energy_empty = g_value_get_double (value);
		break;
	case PROP_ENERGY_FULL:
		device->priv->energy_full = g_value_get_double (value);
		break;
	case PROP_ENERGY_FULL_DESIGN:
		device->priv->energy_full_design = g_value_get_double (value);
		break;
	case PROP_ENERGY_RATE:
		device->priv->energy_rate = g_value_get_double (value);
		break;
	case PROP_VOLTAGE:
		device->priv->voltage = g_value_get_double (value);
		break;
	case PROP_TIME_TO_EMPTY:
		device->priv->time_to_empty = g_value_get_int64 (value);
		break;
	case PROP_TIME_TO_FULL:
		device->priv->time_to_full = g_value_get_int64 (value);
		break;
	case PROP_PERCENTAGE:
		device->priv->percentage = g_value_get_double (value);
		break;
	case PROP_TECHNOLOGY:
		device->priv->technology = g_value_get_uint (value);
		break;
	case PROP_RECALL_NOTICE:
		device->priv->recall_notice = g_value_get_boolean (value);
		break;
	case PROP_RECALL_VENDOR:
		g_free (device->priv->recall_vendor);
		device->priv->recall_vendor = g_strdup (g_value_get_string (value));
		break;
	case PROP_RECALL_URL:
		g_free (device->priv->recall_url);
		device->priv->recall_url = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * up_device_get_on_battery:
 *
 * Note: Only implement for system devices, i.e. ones supplying the system
 **/
gboolean
up_device_get_on_battery (UpDevice *device, gboolean *on_battery)
{
	UpDeviceClass *klass = UP_DEVICE_GET_CLASS (device);

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	/* no support */
	if (klass->get_on_battery == NULL)
		return FALSE;

	return klass->get_on_battery (device, on_battery);
}

/**
 * up_device_get_low_battery:
 *
 * Note: Only implement for system devices, i.e. ones supplying the system
 **/
gboolean
up_device_get_low_battery (UpDevice *device, gboolean *low_battery)
{
	UpDeviceClass *klass = UP_DEVICE_GET_CLASS (device);

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	/* no support */
	if (klass->get_low_battery == NULL)
		return FALSE;

	return klass->get_low_battery (device, low_battery);
}

/**
 * up_device_get_online:
 *
 * Note: Only implement for system devices, i.e. devices supplying the system
 **/
gboolean
up_device_get_online (UpDevice *device, gboolean *online)
{
	UpDeviceClass *klass = UP_DEVICE_GET_CLASS (device);

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	/* no support */
	if (klass->get_online == NULL)
		return FALSE;

	return klass->get_online (device, online);
}

/**
 * up_device_get_id:
 **/
static gchar *
up_device_get_id (UpDevice *device)
{
	GString *string;
	gchar *id = NULL;

	/* line power */
	if (device->priv->type == UP_DEVICE_KIND_LINE_POWER) {
		goto out;

	/* batteries */
	} else if (device->priv->type == UP_DEVICE_KIND_BATTERY) {
		/* we don't have an ID if we are not present */
		if (!device->priv->is_present)
			goto out;

		string = g_string_new ("");

		/* in an ideal world, model-capacity-serial */
		if (device->priv->model != NULL && strlen (device->priv->model) > 2) {
			g_string_append (string, device->priv->model);
			g_string_append_c (string, '-');
		}
		if (device->priv->energy_full_design > 0) {
			/* FIXME: this may not be stable if we are using voltage_now */
			g_string_append_printf (string, "%i", (guint) device->priv->energy_full_design);
			g_string_append_c (string, '-');
		}
		if (device->priv->serial != NULL && strlen (device->priv->serial) > 2) {
			g_string_append (string, device->priv->serial);
			g_string_append_c (string, '-');
		}

		/* make sure we are sane */
		if (string->len == 0) {
			/* just use something generic */
			g_string_append (string, "generic_id");
		} else {
			/* remove trailing '-' */
			g_string_set_size (string, string->len - 1);
		}

		/* the id may have invalid chars that need to be replaced */
		id = g_string_free (string, FALSE);

	} else {
		/* generic fallback, get what data we can */
		string = g_string_new ("");
		if (device->priv->vendor != NULL) {
			g_string_append (string, device->priv->vendor);
			g_string_append_c (string, '-');
		}
		if (device->priv->model != NULL) {
			g_string_append (string, device->priv->model);
			g_string_append_c (string, '-');
		}
		if (device->priv->serial != NULL) {
			g_string_append (string, device->priv->serial);
			g_string_append_c (string, '-');
		}

		/* make sure we are sane */
		if (string->len == 0) {
			/* just use something generic */
			g_string_append (string, "generic_id");
		} else {
			/* remove trailing '-' */
			g_string_set_size (string, string->len - 1);
		}

		/* the id may have invalid chars that need to be replaced */
		id = g_string_free (string, FALSE);
	}

	g_strdelimit (id, "\\\t\"?' /,.", '_');

out:
	return id;
}

/**
 * up_device_get_daemon:
 *
 * Returns a refcounted #UpDaemon instance, or %NULL
 **/
UpDaemon *
up_device_get_daemon (UpDevice *device)
{
	if (device->priv->daemon == NULL)
		return NULL;
	return g_object_ref (device->priv->daemon);
}

/**
 * up_device_coldplug:
 *
 * Return %TRUE on success, %FALSE if we failed to get data and should be removed
 **/
gboolean
up_device_coldplug (UpDevice *device, UpDaemon *daemon, GObject *native)
{
	gboolean ret;
	const gchar *native_path;
	UpDeviceClass *klass = UP_DEVICE_GET_CLASS (device);
	gchar *id = NULL;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	/* save */
	device->priv->native = g_object_ref (native);
	device->priv->daemon = g_object_ref (daemon);

	native_path = up_native_get_native_path (native);
	device->priv->native_path = g_strdup (native_path);

	/* stop signals and callbacks */
	g_object_freeze_notify (G_OBJECT(device));
	device->priv->during_coldplug = TRUE;

	/* coldplug source */
	if (klass->coldplug != NULL) {
		ret = klass->coldplug (device);
		if (!ret) {
			egg_debug ("failed to coldplug %s", device->priv->native_path);
			goto out;
		}
	}

	/* only put on the bus if we succeeded */
	ret = up_device_register_device (device);
	if (!ret) {
		egg_warning ("failed to register device %s", device->priv->native_path);
		goto out;
	}

	/* force a refresh, although failure isn't fatal */
	ret = up_device_refresh_internal (device);
	if (!ret) {
		egg_debug ("failed to refresh %s", device->priv->native_path);

		/* TODO: refresh should really have seporate
		 *       success _and_ changed parameters */
		ret = TRUE;
		goto out;
	}

	/* get the id so we can load the old history */
	id = up_device_get_id (device);
	if (id != NULL)
		up_history_set_id (device->priv->history, id);

out:
	/* start signals and callbacks */
	g_object_thaw_notify (G_OBJECT(device));
	device->priv->during_coldplug = FALSE;
	g_free (id);
	return ret;
}

/**
 * up_device_get_statistics:
 **/
gboolean
up_device_get_statistics (UpDevice *device, const gchar *type, DBusGMethodInvocation *context)
{
	GError *error;
	GPtrArray *array = NULL;
	GPtrArray *complex;
	UpStatsItem *item;
	GValue *value;
	guint i;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	/* doesn't even try to support this */
	if (!device->priv->has_statistics) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "device does not support getting stats");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get the correct data */
	if (g_strcmp0 (type, "charging") == 0)
		array = up_history_get_profile_data (device->priv->history, TRUE);
	else if (g_strcmp0 (type, "discharging") == 0)
		array = up_history_get_profile_data (device->priv->history, FALSE);

	/* maybe the device doesn't support histories */
	if (array == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "device has no statistics");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* always 101 items of data */
	if (array->len != 101) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "statistics invalid as have %i items", array->len);
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* copy data to dbus struct */
	complex = g_ptr_array_sized_new (array->len);
	for (i=0; i<array->len; i++) {
		item = (UpStatsItem *) g_ptr_array_index (array, i);
		value = g_new0 (GValue, 1);
		g_value_init (value, UP_DBUS_STRUCT_DOUBLE_DOUBLE);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (UP_DBUS_STRUCT_DOUBLE_DOUBLE));
		dbus_g_type_struct_set (value,
					0, up_stats_item_get_value (item),
					1, up_stats_item_get_accuracy (item), -1);
		g_ptr_array_add (complex, g_value_get_boxed (value));
		g_free (value);
	}

	dbus_g_method_return (context, complex);
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return TRUE;
}

/**
 * up_device_get_history:
 **/
gboolean
up_device_get_history (UpDevice *device, const gchar *type_string, guint timespan, guint resolution, DBusGMethodInvocation *context)
{
	GError *error;
	GPtrArray *array = NULL;
	GPtrArray *complex;
	UpHistoryItem *item;
	GValue *value;
	guint i;
	UpHistoryType type = UP_HISTORY_TYPE_UNKNOWN;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (type_string != NULL, FALSE);

	/* doesn't even try to support this */
	if (!device->priv->has_history) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "device does not support getting history");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get the correct data */
	if (g_strcmp0 (type_string, "rate") == 0)
		type = UP_HISTORY_TYPE_RATE;
	else if (g_strcmp0 (type_string, "charge") == 0)
		type = UP_HISTORY_TYPE_CHARGE;
	else if (g_strcmp0 (type_string, "time-full") == 0)
		type = UP_HISTORY_TYPE_TIME_FULL;
	else if (g_strcmp0 (type_string, "time-empty") == 0)
		type = UP_HISTORY_TYPE_TIME_EMPTY;

	/* something recognised */
	if (type != UP_HISTORY_TYPE_UNKNOWN)
		array = up_history_get_data (device->priv->history, type, timespan, resolution);

	/* maybe the device doesn't have any history */
	if (array == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "device has no history");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* copy data to dbus struct */
	complex = g_ptr_array_sized_new (array->len);
	for (i=0; i<array->len; i++) {
		item = (UpHistoryItem *) g_ptr_array_index (array, i);
		value = g_new0 (GValue, 1);
		g_value_init (value, UP_DBUS_STRUCT_UINT_DOUBLE_UINT);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (UP_DBUS_STRUCT_UINT_DOUBLE_UINT));
		dbus_g_type_struct_set (value,
					0, up_history_item_get_time (item),
					1, up_history_item_get_value (item),
					2, up_history_item_get_state (item), -1);
		g_ptr_array_add (complex, g_value_get_boxed (value));
		g_free (value);
	}

	dbus_g_method_return (context, complex);
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return TRUE;
}

/**
 * up_device_refresh_internal:
 *
 * NOTE: if you're calling this function you have to ensure you're doing the
 * the changed signals on the right interfaces, although by monitoring
 * notify::update-time this should be mostly done.
 **/
gboolean
up_device_refresh_internal (UpDevice *device)
{
	gboolean ret = FALSE;
	UpDeviceClass *klass = UP_DEVICE_GET_CLASS (device);

	/* not implemented */
	if (klass->refresh == NULL)
		goto out;

	/* do the refresh */
	ret = klass->refresh (device);
	if (!ret) {
		egg_debug ("no changes");
		goto out;
	}

	/* the first time, print all properties */
	if (!device->priv->has_ever_refresh) {
		egg_debug ("added native-path: %s\n", device->priv->native_path);
		device->priv->has_ever_refresh = TRUE;
		goto out;
	}
out:
	return ret;
}

/**
 * up_device_refresh:
 *
 * Return %TRUE on success, %FALSE if we failed to refresh or no data
 **/
gboolean
up_device_refresh (UpDevice *device, DBusGMethodInvocation *context)
{
	gboolean ret;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	ret = up_device_refresh_internal (device);
	dbus_g_method_return (context);
	return ret;
}

/**
 * up_device_get_object_path:
 **/
const gchar *
up_device_get_object_path (UpDevice *device)
{
	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);
	return device->priv->object_path;
}

GObject *
up_device_get_native (UpDevice *device)
{
	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);
	return device->priv->native;
}

/**
 * up_device_compute_object_path:
 **/
static gchar *
up_device_compute_object_path (UpDevice *device)
{
	gchar *basename;
	gchar *id;
	gchar *object_path;
	const gchar *native_path;
	const gchar *type;
	guint i;

	type = up_device_kind_to_string (device->priv->type);
	native_path = device->priv->native_path;
	basename = g_path_get_basename (native_path);
	id = g_strjoin ("_", type, basename, NULL);

	/* make DBUS valid path */
	for (i=0; id[i] != '\0'; i++) {
		if (id[i] == '-')
			id[i] = '_';
		if (id[i] == '.')
			id[i] = 'x';
		if (id[i] == ':')
			id[i] = 'o';
	}
	object_path = g_build_filename ("/org/freedesktop/UPower/devices", id, NULL);

	g_free (basename);
	g_free (id);

	return object_path;
}

/**
 * up_device_register_device:
 **/
static gboolean
up_device_register_device (UpDevice *device)
{
	gboolean ret = TRUE;

	device->priv->object_path = up_device_compute_object_path (device);
	egg_debug ("object path = %s", device->priv->object_path);
	dbus_g_connection_register_g_object (device->priv->system_bus_connection,
					     device->priv->object_path, G_OBJECT (device));
	device->priv->system_bus_proxy = dbus_g_proxy_new_for_name (device->priv->system_bus_connection,
								    DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (device->priv->system_bus_proxy == NULL) {
		egg_warning ("proxy invalid");
		ret = FALSE;
	}
	return ret;
}

/**
 * up_device_perhaps_changed_cb:
 **/
static void
up_device_perhaps_changed_cb (GObject *object, GParamSpec *pspec, UpDevice *device)
{
	g_return_if_fail (UP_IS_DEVICE (device));

	/* don't proxy during coldplug */
	if (device->priv->during_coldplug)
		return;

	/* save new history */
	up_history_set_state (device->priv->history, device->priv->state);
	up_history_set_charge_data (device->priv->history, device->priv->percentage);
	up_history_set_rate_data (device->priv->history, device->priv->energy_rate);
	up_history_set_time_full_data (device->priv->history, device->priv->time_to_full);
	up_history_set_time_empty_data (device->priv->history, device->priv->time_to_empty);

	/*  The order here matters; we want Device::Changed() before
	 *  the DeviceChanged() signal on the main object */
	egg_debug ("emitting changed on %s", device->priv->native_path);
	g_signal_emit (device, signals[SIGNAL_CHANGED], 0);
}

/**
 * up_device_init:
 **/
static void
up_device_init (UpDevice *device)
{
	GError *error = NULL;

	device->priv = UP_DEVICE_GET_PRIVATE (device);
	device->priv->object_path = NULL;
	device->priv->system_bus_connection = NULL;
	device->priv->system_bus_proxy = NULL;
	device->priv->daemon = NULL;
	device->priv->native = NULL;
	device->priv->has_ever_refresh = FALSE;
	device->priv->during_coldplug = FALSE;
	device->priv->history = up_history_new ();

	device->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (device->priv->system_bus_connection == NULL) {
		egg_error ("error getting system bus: %s", error->message);
		g_error_free (error);
	}
	g_signal_connect (device, "notify::update-time", G_CALLBACK (up_device_perhaps_changed_cb), device);
}

/**
 * up_device_finalize:
 **/
static void
up_device_finalize (GObject *object)
{
	UpDevice *device;

	g_return_if_fail (object != NULL);
	g_return_if_fail (UP_IS_DEVICE (object));

	device = UP_DEVICE (object);
	g_return_if_fail (device->priv != NULL);
	if (device->priv->native != NULL)
		g_object_unref (device->priv->native);
	if (device->priv->daemon != NULL)
		g_object_unref (device->priv->daemon);
	g_object_unref (device->priv->history);
	g_free (device->priv->object_path);
	g_free (device->priv->vendor);
	g_free (device->priv->model);
	g_free (device->priv->serial);
	g_free (device->priv->native_path);
	g_free (device->priv->recall_vendor);
	g_free (device->priv->recall_url);

	G_OBJECT_CLASS (up_device_parent_class)->finalize (object);
}

/**
 * up_device_class_init:
 **/
static void
up_device_class_init (UpDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = up_device_get_property;
	object_class->set_property = up_device_set_property;
	object_class->finalize = up_device_finalize;

	g_type_class_add_private (klass, sizeof (UpDevicePrivate));

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	dbus_g_object_type_install_info (UP_TYPE_DEVICE, &dbus_glib_up_device_object_info);

	/**
	 * UpDevice:update-time:
	 */
	g_object_class_install_property (object_class,
					 PROP_UPDATE_TIME,
					 g_param_spec_uint64 ("update-time",
							      NULL, NULL,
							      0, G_MAXUINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:vendor:
	 */
	g_object_class_install_property (object_class,
					 PROP_VENDOR,
					 g_param_spec_string ("vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:model:
	 */
	g_object_class_install_property (object_class,
					 PROP_MODEL,
					 g_param_spec_string ("model",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:serial:
	 */
	g_object_class_install_property (object_class,
					 PROP_SERIAL,
					 g_param_spec_string ("serial",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:native-path:
	 */
	g_object_class_install_property (object_class,
					 PROP_NATIVE_PATH,
					 g_param_spec_string ("native-path",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:power-supply:
	 */
	g_object_class_install_property (object_class,
					 PROP_POWER_SUPPLY,
					 g_param_spec_boolean ("power-supply",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:online:
	 */
	g_object_class_install_property (object_class,
					 PROP_ONLINE,
					 g_param_spec_boolean ("online",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:is-present:
	 */
	g_object_class_install_property (object_class,
					 PROP_IS_PRESENT,
					 g_param_spec_boolean ("is-present",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:is-rechargeable:
	 */
	g_object_class_install_property (object_class,
					 PROP_IS_RECHARGEABLE,
					 g_param_spec_boolean ("is-rechargeable",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:has-history:
	 */
	g_object_class_install_property (object_class,
					 PROP_HAS_HISTORY,
					 g_param_spec_boolean ("has-history",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:has-statistics:
	 */
	g_object_class_install_property (object_class,
					 PROP_HAS_STATISTICS,
					 g_param_spec_boolean ("has-statistics",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:type:
	 */
	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_uint ("type",
							    NULL, NULL,
							    UP_DEVICE_KIND_UNKNOWN,
							    UP_DEVICE_KIND_LAST,
							    UP_DEVICE_KIND_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * UpDevice:state:
	 */
	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_uint ("state",
							    NULL, NULL,
							    UP_DEVICE_STATE_UNKNOWN,
							    UP_DEVICE_STATE_LAST,
							    UP_DEVICE_STATE_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * UpDevice:technology:
	 */
	g_object_class_install_property (object_class,
					 PROP_TECHNOLOGY,
					 g_param_spec_uint ("technology",
							    NULL, NULL,
							    UP_DEVICE_TECHNOLOGY_UNKNOWN,
							    UP_DEVICE_TECHNOLOGY_LAST,
							    UP_DEVICE_TECHNOLOGY_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * UpDevice:capacity:
	 */
	g_object_class_install_property (object_class,
					 PROP_CAPACITY,
					 g_param_spec_double ("capacity", NULL, NULL,
							      0.0, 100.f, 100.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy:
	 */
	g_object_class_install_property (object_class,
					 PROP_ENERGY,
					 g_param_spec_double ("energy", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-empty:
	 */
	g_object_class_install_property (object_class,
					 PROP_ENERGY_EMPTY,
					 g_param_spec_double ("energy-empty", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-full:
	 */
	g_object_class_install_property (object_class,
					 PROP_ENERGY_FULL,
					 g_param_spec_double ("energy-full", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-full-design:
	 */
	g_object_class_install_property (object_class,
					 PROP_ENERGY_FULL_DESIGN,
					 g_param_spec_double ("energy-full-design", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-rate:
	 */
	g_object_class_install_property (object_class,
					 PROP_ENERGY_RATE,
					 g_param_spec_double ("energy-rate", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:voltage:
	 */
	g_object_class_install_property (object_class,
					 PROP_VOLTAGE,
					 g_param_spec_double ("voltage", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:time-to-empty:
	 */
	g_object_class_install_property (object_class,
					 PROP_TIME_TO_EMPTY,
					 g_param_spec_int64 ("time-to-empty", NULL, NULL,
							      0, G_MAXINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:time-to-full:
	 */
	g_object_class_install_property (object_class,
					 PROP_TIME_TO_FULL,
					 g_param_spec_int64 ("time-to-full", NULL, NULL,
							      0, G_MAXINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:percentage:
	 */
	g_object_class_install_property (object_class,
					 PROP_PERCENTAGE,
					 g_param_spec_double ("percentage", NULL, NULL,
							      0.0, 100.f, 100.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-notice:
	 */
	g_object_class_install_property (object_class,
					 PROP_RECALL_NOTICE,
					 g_param_spec_boolean ("recall-notice",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-vendor:
	 */
	g_object_class_install_property (object_class,
					 PROP_RECALL_VENDOR,
					 g_param_spec_string ("recall-vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-url:
	 */
	g_object_class_install_property (object_class,
					 PROP_RECALL_URL,
					 g_param_spec_string ("recall-url",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));

	dbus_g_error_domain_register (UP_DEVICE_ERROR, NULL, UP_DEVICE_TYPE_ERROR);
}

/**
 * up_device_new:
 **/
UpDevice *
up_device_new (void)
{
	UpDevice *device;
	device = UP_DEVICE (g_object_new (UP_TYPE_DEVICE, NULL));
	return device;
}

