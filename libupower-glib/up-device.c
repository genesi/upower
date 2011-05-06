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

/**
 * SECTION:up-device
 * @short_description: Client object for accessing information about UPower devices
 *
 * A helper GObject to use for accessing UPower devices, and to be notified
 * when it is changed.
 *
 * See also: #UpClient
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <string.h>

#include "up-device.h"
#include "up-stats-item.h"
#include "up-history-item.h"

static void	up_device_class_init	(UpDeviceClass	*klass);
static void	up_device_init		(UpDevice	*device);
static void	up_device_finalize	(GObject		*object);

#define UP_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_DEVICE, UpDevicePrivate))

/**
 * UpDevicePrivate:
 *
 * Private #PkDevice data
 **/
struct _UpDevicePrivate
{
	gchar			*object_path;
	DBusGConnection		*bus;
	DBusGProxy		*proxy_device;
	DBusGProxy		*proxy_props;

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
	UpDeviceKind		 kind;
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

enum {
	PROP_0,
	PROP_UPDATE_TIME,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_NATIVE_PATH,
	PROP_POWER_SUPPLY,
	PROP_ONLINE,
	PROP_IS_PRESENT,
	PROP_IS_RECHARGEABLE,
	PROP_HAS_HISTORY,
	PROP_HAS_STATISTICS,
	PROP_KIND,
	PROP_STATE,
	PROP_TECHNOLOGY,
	PROP_CAPACITY,
	PROP_ENERGY,
	PROP_ENERGY_EMPTY,
	PROP_ENERGY_FULL,
	PROP_ENERGY_FULL_DESIGN,
	PROP_ENERGY_RATE,
	PROP_VOLTAGE,
	PROP_TIME_TO_EMPTY,
	PROP_TIME_TO_FULL,
	PROP_PERCENTAGE,
	PROP_RECALL_NOTICE,
	PROP_RECALL_VENDOR,
	PROP_RECALL_URL,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (UpDevice, up_device, G_TYPE_OBJECT)

/*
 * up_device_get_device_properties:
 */
static GHashTable *
up_device_get_device_properties (UpDevice *device, GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	GHashTable *hash_table = NULL;

	ret = dbus_g_proxy_call (device->priv->proxy_props, "GetAll", &error_local,
				 G_TYPE_STRING, "org.freedesktop.UPower.Device",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				 &hash_table,
				 G_TYPE_INVALID);
	if (!ret) {
		g_set_error (error, 1, 0, "Couldn't call GetAll() to get properties for %s: %s", device->priv->object_path, error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	return hash_table;
}

/*
 * up_device_collect_props_cb:
 */
static void
up_device_collect_props_cb (const char *key, const GValue *value, UpDevice *device)
{
	if (g_strcmp0 (key, "NativePath") == 0) {
		g_free (device->priv->native_path);
		device->priv->native_path = g_strdup (g_value_get_string (value));
	} else if (g_strcmp0 (key, "Vendor") == 0) {
		g_free (device->priv->vendor);
		device->priv->vendor = g_strdup (g_value_get_string (value));
	} else if (g_strcmp0 (key, "Model") == 0) {
		g_free (device->priv->model);
		device->priv->model = g_strdup (g_value_get_string (value));
	} else if (g_strcmp0 (key, "Serial") == 0) {
		g_free (device->priv->serial);
		device->priv->serial = g_strdup (g_value_get_string (value));
	} else if (g_strcmp0 (key, "UpdateTime") == 0) {
		device->priv->update_time = g_value_get_uint64 (value);
	} else if (g_strcmp0 (key, "Type") == 0) {
		device->priv->kind = g_value_get_uint (value);
	} else if (g_strcmp0 (key, "Online") == 0) {
		device->priv->online = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "HasHistory") == 0) {
		device->priv->has_history = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "HasStatistics") == 0) {
		device->priv->has_statistics = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "Energy") == 0) {
		device->priv->energy = g_value_get_double (value);
	} else if (g_strcmp0 (key, "EnergyEmpty") == 0) {
		device->priv->energy_empty = g_value_get_double (value);
	} else if (g_strcmp0 (key, "EnergyFull") == 0) {
		device->priv->energy_full = g_value_get_double (value);
	} else if (g_strcmp0 (key, "EnergyFullDesign") == 0) {
		device->priv->energy_full_design = g_value_get_double (value);
	} else if (g_strcmp0 (key, "EnergyRate") == 0) {
		device->priv->energy_rate = g_value_get_double (value);
	} else if (g_strcmp0 (key, "Voltage") == 0) {
		device->priv->voltage = g_value_get_double (value);
	} else if (g_strcmp0 (key, "TimeToFull") == 0) {
		device->priv->time_to_full = g_value_get_int64 (value);
	} else if (g_strcmp0 (key, "TimeToEmpty") == 0) {
		device->priv->time_to_empty = g_value_get_int64 (value);
	} else if (g_strcmp0 (key, "Percentage") == 0) {
		device->priv->percentage = g_value_get_double (value);
	} else if (g_strcmp0 (key, "Technology") == 0) {
		device->priv->technology = g_value_get_uint (value);
	} else if (g_strcmp0 (key, "IsPresent") == 0) {
		device->priv->is_present = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "IsRechargeable") == 0) {
		device->priv->is_rechargeable = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "PowerSupply") == 0) {
		device->priv->power_supply = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "Capacity") == 0) {
		device->priv->capacity = g_value_get_double (value);
	} else if (g_strcmp0 (key, "State") == 0) {
		device->priv->state = g_value_get_uint (value);
	} else if (g_strcmp0 (key, "RecallNotice") == 0) {
		device->priv->recall_notice = g_value_get_boolean (value);
	} else if (g_strcmp0 (key, "RecallVendor") == 0) {
		g_free (device->priv->recall_vendor);
		device->priv->recall_vendor = g_strdup (g_value_get_string (value));
	} else if (g_strcmp0 (key, "RecallUrl") == 0) {
		g_free (device->priv->recall_url);
		device->priv->recall_url = g_strdup (g_value_get_string (value));
	} else {
		g_warning ("unhandled property '%s'", key);
	}
}

/*
 * up_device_refresh_internal:
 */
static gboolean
up_device_refresh_internal (UpDevice *device, GError **error)
{
	GHashTable *hash;
	GError *error_local = NULL;

	/* get all the properties */
	hash = up_device_get_device_properties (device, &error_local);
	if (hash == NULL) {
		g_set_error (error, 1, 0, "Cannot get device properties for %s: %s", device->priv->object_path, error_local->message);
		g_error_free (error_local);
		return FALSE;
	}
	g_hash_table_foreach (hash, (GHFunc) up_device_collect_props_cb, device);
	g_hash_table_unref (hash);
	return TRUE;
}

/*
 * up_device_changed_cb:
 */
static void
up_device_changed_cb (DBusGProxy *proxy, UpDevice *device)
{
	g_return_if_fail (UP_IS_DEVICE (device));
	up_device_refresh_internal (device, NULL);
	g_signal_emit (device, signals [SIGNAL_CHANGED], 0);
}

/**
 * up_device_set_object_path_sync:
 * @device: a #UpDevice instance.
 * @object_path: The UPower object path.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Sets the object path of the object and fills up initial properties.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.9.0
 **/
gboolean
up_device_set_object_path_sync (UpDevice *device, const gchar *object_path, GCancellable *cancellable, GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;
	DBusGProxy *proxy_device;
	DBusGProxy *proxy_props;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);

	if (device->priv->object_path != NULL)
		return FALSE;
	if (object_path == NULL)
		return FALSE;

	/* connect to the bus */
	device->priv->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error_local);
	if (device->priv->bus == NULL) {
		g_set_error (error, 1, 0, "Couldn't connect to system bus: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* connect to the correct path for properties */
	proxy_props = dbus_g_proxy_new_for_name (device->priv->bus, "org.freedesktop.UPower",
						 object_path, "org.freedesktop.DBus.Properties");
	if (proxy_props == NULL) {
		g_set_error_literal (error, 1, 0, "Couldn't connect to proxy");
		goto out;
	}

	/* connect to the correct path for all the other methods */
	proxy_device = dbus_g_proxy_new_for_name (device->priv->bus, "org.freedesktop.UPower",
						  object_path, "org.freedesktop.UPower.Device");
	if (proxy_device == NULL) {
		g_set_error_literal (error, 1, 0, "Couldn't connect to proxy");
		goto out;
	}

	/* listen to Changed */
	dbus_g_proxy_add_signal (proxy_device, "Changed", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy_device, "Changed",
				     G_CALLBACK (up_device_changed_cb), device, NULL);

	/* yay */
	device->priv->proxy_device = proxy_device;
	device->priv->proxy_props = proxy_props;
	device->priv->object_path = g_strdup (object_path);

	/* coldplug */
	ret = up_device_refresh_internal (device, &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "cannot refresh: %s", error_local->message);
		g_error_free (error_local);
	}
out:
	return ret;
}

/**
 * up_device_get_object_path:
 * @device: a #UpDevice instance.
 *
 * Gets the object path for the device.
 *
 * Return value: the object path, or %NULL
 *
 * Since: 0.9.0
 **/
const gchar *
up_device_get_object_path (UpDevice *device)
{
	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);
	return device->priv->object_path;
}

/*
 * up_device_to_text_history:
 */
static void
up_device_to_text_history (UpDevice *device, GString *string, const gchar *type)
{
	guint i;
	GPtrArray *array;
	UpHistoryItem *item;

	/* get a fair chunk of data */
	array = up_device_get_history_sync (device, type, 120, 10, NULL, NULL);
	if (array == NULL)
		return;

	/* pretty print */
	g_string_append_printf (string, "  History (%s):\n", type);
	for (i=0; i<array->len; i++) {
		item = (UpHistoryItem *) g_ptr_array_index (array, i);
		g_string_append_printf (string, "    %i\t%.3f\t%s\n",
				 up_history_item_get_time (item),
				 up_history_item_get_value (item),
				 up_device_state_to_string (up_history_item_get_state (item)));
	}
	g_ptr_array_unref (array);
}

/*
 * up_device_bool_to_string:
 */
static const gchar *
up_device_bool_to_string (gboolean ret)
{
	return ret ? "yes" : "no";
}

/*
 * up_device_to_text_time_to_string:
 */
static gchar *
up_device_to_text_time_to_string (gint seconds)
{
	gfloat value = seconds;

	if (value < 0)
		return g_strdup ("unknown");
	if (value < 60)
		return g_strdup_printf ("%.0f seconds", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f minutes", value);
	value /= 60.0;
	if (value < 60)
		return g_strdup_printf ("%.1f hours", value);
	value /= 24.0;
	return g_strdup_printf ("%.1f days", value);
}

/**
 * up_device_to_text:
 * @device: a #UpDevice instance.
 *
 * Converts the device to a string description.
 *
 * Return value: text representation of #UpDevice
 *
 * Since: 0.9.0
 **/
gchar *
up_device_to_text (UpDevice *device)
{
	struct tm *time_tm;
	time_t t;
	gchar time_buf[256];
	gchar *time_str;
	GString *string;

	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);

	/* get a human readable time */
	t = (time_t) device->priv->update_time;
	time_tm = localtime (&t);
	strftime (time_buf, sizeof time_buf, "%c", time_tm);

	string = g_string_new ("");
	g_string_append_printf (string, "  native-path:          %s\n", device->priv->native_path);
	if (device->priv->vendor != NULL && device->priv->vendor[0] != '\0')
		g_string_append_printf (string, "  vendor:               %s\n", device->priv->vendor);
	if (device->priv->model != NULL && device->priv->model[0] != '\0')
		g_string_append_printf (string, "  model:                %s\n", device->priv->model);
	if (device->priv->serial != NULL && device->priv->serial[0] != '\0')
		g_string_append_printf (string, "  serial:               %s\n", device->priv->serial);
	g_string_append_printf (string, "  power supply:         %s\n", up_device_bool_to_string (device->priv->power_supply));
	g_string_append_printf (string, "  updated:              %s (%d seconds ago)\n", time_buf, (int) (time (NULL) - device->priv->update_time));
	g_string_append_printf (string, "  has history:          %s\n", up_device_bool_to_string (device->priv->has_history));
	g_string_append_printf (string, "  has statistics:       %s\n", up_device_bool_to_string (device->priv->has_statistics));
	g_string_append_printf (string, "  %s\n", up_device_kind_to_string (device->priv->kind));

	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MOUSE ||
	    device->priv->kind == UP_DEVICE_KIND_KEYBOARD ||
	    device->priv->kind == UP_DEVICE_KIND_UPS)
		g_string_append_printf (string, "    present:             %s\n", up_device_bool_to_string (device->priv->is_present));
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MOUSE ||
	    device->priv->kind == UP_DEVICE_KIND_KEYBOARD)
		g_string_append_printf (string, "    rechargeable:        %s\n", up_device_bool_to_string (device->priv->is_rechargeable));
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MOUSE ||
	    device->priv->kind == UP_DEVICE_KIND_KEYBOARD ||
	    device->priv->kind == UP_DEVICE_KIND_UPS)
		g_string_append_printf (string, "    state:               %s\n", up_device_state_to_string (device->priv->state));
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY) {
		g_string_append_printf (string, "    energy:              %g Wh\n", device->priv->energy);
		g_string_append_printf (string, "    energy-empty:        %g Wh\n", device->priv->energy_empty);
		g_string_append_printf (string, "    energy-full:         %g Wh\n", device->priv->energy_full);
		g_string_append_printf (string, "    energy-full-design:  %g Wh\n", device->priv->energy_full_design);
	}
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MONITOR)
		g_string_append_printf (string, "    energy-rate:         %g W\n", device->priv->energy_rate);
	if (device->priv->kind == UP_DEVICE_KIND_UPS ||
	    device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MONITOR) {
		if (device->priv->voltage > 0)
			g_string_append_printf (string, "    voltage:             %g V\n", device->priv->voltage);
	}
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_UPS) {
		if (device->priv->time_to_full > 0) {
			time_str = up_device_to_text_time_to_string (device->priv->time_to_full);
			g_string_append_printf (string, "    time to full:        %s\n", time_str);
			g_free (time_str);
		}
		if (device->priv->time_to_empty > 0) {
			time_str = up_device_to_text_time_to_string (device->priv->time_to_empty);
			g_string_append_printf (string, "    time to empty:       %s\n", time_str);
			g_free (time_str);
		}
	}
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY ||
	    device->priv->kind == UP_DEVICE_KIND_MOUSE ||
	    device->priv->kind == UP_DEVICE_KIND_KEYBOARD ||
	    device->priv->kind == UP_DEVICE_KIND_UPS)
		g_string_append_printf (string, "    percentage:          %g%%\n", device->priv->percentage);
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY) {
		if (device->priv->capacity > 0)
			g_string_append_printf (string, "    capacity:            %g%%\n", device->priv->capacity);
	}
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY) {
		if (device->priv->technology != UP_DEVICE_TECHNOLOGY_UNKNOWN)
			g_string_append_printf (string, "    technology:          %s\n", up_device_technology_to_string (device->priv->technology));
	}
	if (device->priv->kind == UP_DEVICE_KIND_LINE_POWER)
		g_string_append_printf (string, "    online:             %s\n", up_device_bool_to_string (device->priv->online));
	if (device->priv->kind == UP_DEVICE_KIND_BATTERY) {
		if (device->priv->recall_notice) {
			g_string_append_printf (string, "    recall vendor:       %s\n", device->priv->recall_vendor);
			g_string_append_printf (string, "    recall url:          %s\n", device->priv->recall_url);
		}
	}

	/* if we can, get history */
	if (device->priv->has_history) {
		up_device_to_text_history (device, string, "charge");
		up_device_to_text_history (device, string, "rate");
	}

	return g_string_free (string, FALSE);
}

/**
 * up_device_refresh_sync:
 * @device: a #UpDevice instance.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Refreshes properties on the device.
 * This function is normally not required.
 *
 * Return value: #TRUE for success, else #FALSE and @error is used
 *
 * Since: 0.9.0
 **/
gboolean
up_device_refresh_sync (UpDevice *device, GCancellable *cancellable, GError **error)
{
	GError *error_local = NULL;
	gboolean ret;

	g_return_val_if_fail (UP_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->proxy_device != NULL, FALSE);

	/* just refresh the device */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "Refresh", &error_local,
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		g_set_error (error, 1, 0, "Refresh() on %s failed: %s", device->priv->object_path, error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	return ret;
}

/**
 * up_device_get_history_sync:
 * @device: a #UpDevice instance.
 * @type: The type of history, known values are "rate" and "charge".
 * @timespec: the amount of time to look back into time.
 * @resolution: the resolution of data.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the device history.
 *
 * Return value: an array of #UpHistoryItem's, else #NULL and @error is used
 *
 * Since: 0.9.0
 **/
GPtrArray *
up_device_get_history_sync (UpDevice *device, const gchar *type, guint timespec, guint resolution, GCancellable *cancellable, GError **error)
{
	GError *error_local = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	UpHistoryItem *item;
	GPtrArray *array = NULL;
	gboolean ret;

	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);
	g_return_val_if_fail (device->priv->proxy_device != NULL, NULL);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_UINT,
						G_TYPE_DOUBLE,
						G_TYPE_UINT,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "GetHistory", &error_local,
				 G_TYPE_STRING, type,
				 G_TYPE_UINT, timespec,
				 G_TYPE_UINT, resolution,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		g_set_error (error, 1, 0, "GetHistory(%s,%i) on %s failed: %s", type, timespec,
			   device->priv->object_path, error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0) {
		g_set_error_literal (error, 1, 0, "no data");
		goto out;
	}

	/* convert */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		item = up_history_item_new ();
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		up_history_item_set_time (item, g_value_get_uint (gv));
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		up_history_item_set_value (item, g_value_get_double (gv));
		g_value_unset (gv);
		/* 2 */
		gv = g_value_array_get_nth (gva, 2);
		up_history_item_set_state (item, g_value_get_uint (gv));
		g_value_unset (gv);
		g_ptr_array_add (array, item);
		g_value_array_free (gva);
	}

out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	return array;
}

/**
 * up_device_get_statistics_sync:
 * @device: a #UpDevice instance.
 * @type: the type of statistics.
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError, or %NULL.
 *
 * Gets the device current statistics.
 *
 * Return value: an array of #UpStatsItem's, else #NULL and @error is used
 *
 * Since: 0.9.0
 **/
GPtrArray *
up_device_get_statistics_sync (UpDevice *device, const gchar *type, GCancellable *cancellable, GError **error)
{
	GError *error_local = NULL;
	GType g_type_gvalue_array;
	GPtrArray *gvalue_ptr_array = NULL;
	GValueArray *gva;
	GValue *gv;
	guint i;
	UpStatsItem *item;
	GPtrArray *array = NULL;
	gboolean ret;

	g_return_val_if_fail (UP_IS_DEVICE (device), NULL);
	g_return_val_if_fail (device->priv->proxy_device != NULL, NULL);

	g_type_gvalue_array = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_DOUBLE,
						G_TYPE_DOUBLE,
						G_TYPE_INVALID));

	/* get compound data */
	ret = dbus_g_proxy_call (device->priv->proxy_device, "GetStatistics", &error_local,
				 G_TYPE_STRING, type,
				 G_TYPE_INVALID,
				 g_type_gvalue_array, &gvalue_ptr_array,
				 G_TYPE_INVALID);
	if (!ret) {
		g_set_error (error, 1, 0, "GetStatistics(%s) on %s failed: %s", type,
				      device->priv->object_path, error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* no data */
	if (gvalue_ptr_array->len == 0) {
		g_set_error_literal (error, 1, 0, "no data");
		goto out;
	}

	/* convert */
	array = g_ptr_array_new ();

	for (i=0; i<gvalue_ptr_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (gvalue_ptr_array, i);
		item = up_stats_item_new ();
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		up_stats_item_set_value (item, g_value_get_double (gv));
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		up_stats_item_set_accuracy (item, g_value_get_double (gv));
		g_value_unset (gv);
		/* 2 */
		g_ptr_array_add (array, item);
		g_value_array_free (gva);
	}
out:
	if (gvalue_ptr_array != NULL)
		g_ptr_array_free (gvalue_ptr_array, TRUE);
	return array;
}

/*
 * up_device_set_property:
 */
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
	case PROP_KIND:
		device->priv->kind = g_value_get_uint (value);
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

/*
 * up_device_get_property:
 */
static void
up_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	UpDevice *device = UP_DEVICE (object);

	switch (prop_id) {
	case PROP_UPDATE_TIME:
		g_value_set_uint64 (value, device->priv->update_time);
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
	case PROP_NATIVE_PATH:
		g_value_set_string (value, device->priv->native_path);
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
	case PROP_KIND:
		g_value_set_uint (value, device->priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, device->priv->state);
		break;
	case PROP_TECHNOLOGY:
		g_value_set_uint (value, device->priv->technology);
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

/*
 * up_device_class_init:
 */
static void
up_device_class_init (UpDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_device_finalize;
	object_class->set_property = up_device_set_property;
	object_class->get_property = up_device_get_property;

	/**
	 * UpDevice::changed:
	 * @device: the #UpDevice instance that emitted the signal
	 *
	 * The ::changed signal is emitted when the device data has changed.
	 *
	 * Since: 0.9.0
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpDeviceClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * UpDevice:update-time:
	 *
	 * The last time the device was updated.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_UPDATE_TIME,
					 g_param_spec_uint64 ("update-time",
							      NULL, NULL,
							      0, G_MAXUINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:vendor:
	 *
	 * The vendor of the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_VENDOR,
					 g_param_spec_string ("vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:model:
	 *
	 * The model of the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_MODEL,
					 g_param_spec_string ("model",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:serial:
	 *
	 * The serial number of the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_SERIAL,
					 g_param_spec_string ("serial",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:native-path:
	 *
	 * The native path of the device, useful for direct device access.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_NATIVE_PATH,
					 g_param_spec_string ("native-path",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:power-supply:
	 *
	 * If the device is powering the system.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_POWER_SUPPLY,
					 g_param_spec_boolean ("power-supply",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:online:
	 *
	 * If the device is online, i.e. connected.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ONLINE,
					 g_param_spec_boolean ("online",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:is-present:
	 *
	 * If the device is present, as some devices like laptop batteries
	 * can be removed, leaving an empty bay that is still technically a
	 * device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_IS_PRESENT,
					 g_param_spec_boolean ("is-present",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:is-rechargeable:
	 *
	 * If the device has a rechargable battery.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_IS_RECHARGEABLE,
					 g_param_spec_boolean ("is-rechargeable",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:has-history:
	 *
	 * If the device has history data that might be useful.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_HAS_HISTORY,
					 g_param_spec_boolean ("has-history",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:has-statistics:
	 *
	 * If the device has statistics data that might be useful.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_HAS_STATISTICS,
					 g_param_spec_boolean ("has-statistics",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:kind:
	 *
	 * The device kind, e.g. %UP_DEVICE_KIND_KEYBOARD.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_KIND,
					 g_param_spec_uint ("kind",
							    NULL, NULL,
							    UP_DEVICE_KIND_UNKNOWN,
							    UP_DEVICE_KIND_LAST,
							    UP_DEVICE_KIND_UNKNOWN,
							    G_PARAM_READWRITE));
	/**
	 * UpDevice:state:
	 *
	 * The state the device is in at this time, e.g. %UP_DEVICE_STATE_EMPTY.
	 *
	 * Since: 0.9.0
	 **/
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
	 *
	 * The battery technology e.g. %UP_DEVICE_TECHNOLOGY_LITHIUM_ION.
	 *
	 * Since: 0.9.0
	 **/
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
	 *
	 * The percentage capacity of the device where 100% means the device has
	 * the same charge potential as when it was manufactured.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_CAPACITY,
					 g_param_spec_double ("capacity", NULL, NULL,
							      0.0, 100.f, 100.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy:
	 *
	 * The energy left in the device. Measured in mWh.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENERGY,
					 g_param_spec_double ("energy", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-empty:
	 *
	 * The energy the device will have when it is empty. This is usually zero.
	 * Measured in mWh.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENERGY_EMPTY,
					 g_param_spec_double ("energy-empty", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-full:
	 *
	 * The amount of energy when the device is fully charged. Measured in mWh.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENERGY_FULL,
					 g_param_spec_double ("energy-full", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-full-design:
	 *
	 * The amount of energy when the device was brand new. Measured in mWh.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENERGY_FULL_DESIGN,
					 g_param_spec_double ("energy-full-design", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:energy-rate:
	 *
	 * The rate of discharge or charge. Measured in mW.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_ENERGY_RATE,
					 g_param_spec_double ("energy-rate", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:voltage:
	 *
	 * The current voltage of the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_VOLTAGE,
					 g_param_spec_double ("voltage", NULL, NULL,
							      0.0, G_MAXDOUBLE, 0.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:time-to-empty:
	 *
	 * The amount of time until the device is empty.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_TIME_TO_EMPTY,
					 g_param_spec_int64 ("time-to-empty", NULL, NULL,
							      0, G_MAXINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:time-to-full:
	 *
	 * The amount of time until the device is fully charged.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_TIME_TO_FULL,
					 g_param_spec_int64 ("time-to-full", NULL, NULL,
							      0, G_MAXINT64, 0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:percentage:
	 *
	 * The percentage charge of the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_PERCENTAGE,
					 g_param_spec_double ("percentage", NULL, NULL,
							      0.0, 100.f, 100.0,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-notice:
	 *
	 * If the device may be recalled due to defect. NOTE: This property does
	 * not mean the battery is broken, and just that the user should
	 * _check_ with the vendor.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_RECALL_NOTICE,
					 g_param_spec_boolean ("recall-notice",
							       NULL, NULL,
							       FALSE,
							       G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-vendor:
	 *
	 * The vendor that is recalling the device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_RECALL_VENDOR,
					 g_param_spec_string ("recall-vendor",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * UpDevice:recall-url:
	 *
	 * The vendors internet link for the recalled device.
	 *
	 * Since: 0.9.0
	 **/
	g_object_class_install_property (object_class,
					 PROP_RECALL_URL,
					 g_param_spec_string ("recall-url",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (UpDevicePrivate));
}

/*
 * up_device_init:
 */
static void
up_device_init (UpDevice *device)
{
	device->priv = UP_DEVICE_GET_PRIVATE (device);
	device->priv->object_path = NULL;
	device->priv->proxy_device = NULL;
	device->priv->proxy_props = NULL;
}

/*
 * up_device_finalize:
 */
static void
up_device_finalize (GObject *object)
{
	UpDevice *device;

	g_return_if_fail (UP_IS_DEVICE (object));

	device = UP_DEVICE (object);

	g_free (device->priv->object_path);
	g_free (device->priv->vendor);
	g_free (device->priv->model);
	g_free (device->priv->serial);
	g_free (device->priv->native_path);
	g_free (device->priv->recall_vendor);
	g_free (device->priv->recall_url);
	if (device->priv->proxy_device != NULL)
		g_object_unref (device->priv->proxy_device);
	if (device->priv->proxy_props != NULL)
		g_object_unref (device->priv->proxy_props);

	G_OBJECT_CLASS (up_device_parent_class)->finalize (object);
}

/**
 * up_device_new:
 *
 * Creates a new #UpDevice object.
 *
 * Return value: a new UpDevice object.
 *
 * Since: 0.9.0
 **/
UpDevice *
up_device_new (void)
{
	UpDevice *device;
	device = g_object_new (UP_TYPE_DEVICE, NULL);
	return UP_DEVICE (device);
}

