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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "egg-debug.h"

#include "up-qos.h"
#include "up-marshal.h"
#include "up-daemon.h"
#include "up-polkit.h"
#include "up-qos-item.h"
#include "up-qos-glue.h"
#include "up-types.h"

static void     up_qos_finalize   (GObject	*object);

#define UP_QOS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UP_TYPE_QOS, UpQosPrivate))

#define UP_QOS_REQUESTS_STRUCT_TYPE (dbus_g_type_get_struct ("GValueArray",	\
							      G_TYPE_UINT,	\
							      G_TYPE_UINT,	\
							      G_TYPE_UINT,	\
							      G_TYPE_STRING,	\
							      G_TYPE_UINT64,	\
							      G_TYPE_BOOLEAN,	\
							      G_TYPE_STRING,	\
							      G_TYPE_INT,	\
							      G_TYPE_INVALID))

struct UpQosPrivate
{
	GPtrArray		*data;
	gint			 fd[UP_QOS_KIND_LAST];
	gint			 last[UP_QOS_KIND_LAST];
	gint			 minimum[UP_QOS_KIND_LAST];
	UpPolkit		*polkit;
	DBusGConnection		*connection;
	DBusGProxy		*proxy;
};

enum {
	LATENCY_CHANGED,
	REQUESTS_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (UpQos, up_qos, G_TYPE_OBJECT)

/**
 * up_qos_find_from_cookie:
 **/
static UpQosItem *
up_qos_find_from_cookie (UpQos *qos, guint32 cookie)
{
	guint i;
	GPtrArray *data;
	UpQosItem *item;

	/* search list */
	data = qos->priv->data;
	for (i=0; i<data->len; i++) {
		item = g_ptr_array_index (data, i);
		if (up_qos_item_get_cookie (item) == cookie)
			return item;
	}

	/* nothing found */
	return NULL;
}

/**
 * up_qos_generate_cookie:
 *
 * Return value: a random cookie not already allocated
 **/
static guint32
up_qos_generate_cookie (UpQos *qos)
{
	guint32 cookie;

	/* iterate until we have a unique cookie */
	do {
		cookie = (guint32) g_random_int_range (1, G_MAXINT32);
	} while (up_qos_find_from_cookie (qos, cookie) != NULL);

	return cookie;
}

/**
 * up_qos_get_lowest:
 **/
static gint
up_qos_get_lowest (UpQos *qos, UpQosKind type)
{
	guint i;
	gint lowest = G_MAXINT;
	GPtrArray *data;
	UpQosItem *item;

	/* find lowest */
	data = qos->priv->data;
	for (i=0; i<data->len; i++) {
		item = g_ptr_array_index (data, i);
		if (up_qos_item_get_kind (item) == type &&
		    up_qos_item_get_value (item) > 0 &&
		    up_qos_item_get_value (item) < lowest)
			lowest = up_qos_item_get_value (item);
	}

	/* over-ride */
	if (lowest < qos->priv->minimum[type]) {
		egg_debug ("minium override from %i to %i", lowest, qos->priv->minimum[type]);
		lowest = qos->priv->minimum[type];
	}

	/* no requests */
	if (lowest == G_MAXINT)
		lowest = -1;

	return lowest;
}

/**
 * up_qos_latency_write:
 **/
static gboolean
up_qos_latency_write (UpQos *qos, UpQosKind type, gint value)
{
	gchar *text = NULL;
	gint retval;
	gint length;
	gboolean ret = TRUE;

	/* write new values to pm-qos */
	if (qos->priv->fd[type] < 0) {
		egg_warning ("cannot write to pm-qos as file not open");
		ret = FALSE;
		goto out;
	}

	/* convert to text */
	text = g_strdup_printf ("%i", value);
	length = strlen (text);

	/* write to device file */
	retval = write (qos->priv->fd[type], text, length);
	if (retval != length) {
		egg_warning ("writing '%s' to device failed", text);
		ret = FALSE;
		goto out;
	}
out:
	g_free (text);
	return ret;
}

/**
 * up_qos_latency_perhaps_changed:
 **/
static gboolean
up_qos_latency_perhaps_changed (UpQos *qos, UpQosKind type)
{
	gint lowest;
	gint *last;

	/* re-find the lowest value */
	lowest = up_qos_get_lowest (qos, type);

	/* find the last value */
	last = &qos->priv->last[type];

	/* same value? */
	if (*last == lowest)
		return FALSE;

	/* write to file */
	up_qos_latency_write (qos, type, lowest);

	/* emit signal */
	g_signal_emit (qos, signals [LATENCY_CHANGED], 0, up_qos_kind_to_string (type), lowest);
	*last = lowest;
	return TRUE;
}

/**
 * up_qos_get_cmdline:
 **/
static gchar *
up_qos_get_cmdline (gint pid)
{
	gboolean ret;
	gchar *filename = NULL;
	gchar *cmdline = NULL;
	GError *error = NULL;

	/* get command line from proc */
	filename = g_strdup_printf ("/proc/%i/cmdline", pid);
	ret = g_file_get_contents (filename, &cmdline, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get cmdline: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (filename);
	return cmdline;
}

/**
 * up_qos_request_latency:
 *
 * Return value: a new random cookie
 **/
void
up_qos_request_latency (UpQos *qos, const gchar *type_text, gint value, gboolean persistent, DBusGMethodInvocation *context)
{
	UpQosItem *item;
	gchar *sender = NULL;
	const gchar *auth;
	gchar *cmdline = NULL;
	GError *error;
	guint uid;
	gint pid;
	PolkitSubject *subject = NULL;
	gboolean retval;
	UpQosKind type;

	/* get correct data */
	type = up_qos_kind_from_string (type_text);
	if (type == UP_QOS_KIND_UNKNOWN) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "type invalid: %s", type_text);
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* as we are async, we can get the sender */
	sender = dbus_g_method_get_sender (context);
	if (sender == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "no DBUS sender");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get the subject */
	subject = up_polkit_get_subject (qos->priv->polkit, context);
	if (subject == NULL)
		goto out;

	/* check auth */
	if (persistent)
		auth = "org.freedesktop.upower.qos.request-latency-persistent";
	else
		auth = "org.freedesktop.upower.qos.request-latency";
	if (!up_polkit_check_auth (qos->priv->polkit, subject, auth, context))
		goto out;

	/* get uid */
	retval = up_polkit_get_uid (qos->priv->polkit, subject, &uid);
	if (!retval) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "cannot get UID");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get pid */
	retval = up_polkit_get_pid (qos->priv->polkit, subject, &pid);
	if (!retval) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "cannot get PID");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get command line */
	cmdline = up_qos_get_cmdline (pid);
	if (cmdline == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "cannot get cmdline");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* seems okay, add to list */
	item = up_qos_item_new ();
	up_qos_item_set_cookie (item, up_qos_generate_cookie (qos));
	up_qos_item_set_sender (item, sender);
	up_qos_item_set_value (item,  value);
	up_qos_item_set_uid (item, uid);
	up_qos_item_set_pid (item, pid);
	up_qos_item_set_cmdline (item, cmdline);
	up_qos_item_set_persistent (item, persistent);
	up_qos_item_set_kind (item, type);
	g_ptr_array_add (qos->priv->data, item);

	egg_debug ("Recieved Qos from '%s' (%i:%i)' saving as #%i",
		   up_qos_item_get_sender (item),
		   up_qos_item_get_value (item),
		   up_qos_item_get_persistent (item),
		   up_qos_item_get_cookie (item));

	/* TODO: if persistent add to datadase */

	/* only emit event on the first one */
	up_qos_latency_perhaps_changed (qos, type);
	dbus_g_method_return (context, up_qos_item_get_cookie (item));
out:
	if (subject != NULL)
		g_object_unref (subject);
	g_free (sender);
	g_free (cmdline);
}

/**
 * up_qos_cancel_request:
 *
 * Removes a cookie and associated data from the UpQosItem struct.
 **/
void
up_qos_cancel_request (UpQos *qos, guint cookie, DBusGMethodInvocation *context)
{
	UpQosItem *item;
	GError *error;
	gchar *sender = NULL;
	PolkitSubject *subject = NULL;

	/* find the correct cookie */
	item = up_qos_find_from_cookie (qos, cookie);
	if (item == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL,
				     "Cannot find request for #%i", cookie);
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* get the sender? */
	sender = dbus_g_method_get_sender (context);
	if (sender == NULL) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "no DBUS sender");
		dbus_g_method_return_error (context, error);
		goto out;
	}

	/* are we not the sender? */
	if (g_strcmp0 (sender, up_qos_item_get_sender (item)) != 0) {
		subject = up_polkit_get_subject (qos->priv->polkit, context);
		if (subject == NULL)
			goto out;
		if (!up_polkit_check_auth (qos->priv->polkit, subject, "org.freedesktop.upower.qos.cancel-request", context))
			goto out;
	}

	egg_debug ("Clear #%i", cookie);

	/* remove object from list */
	g_ptr_array_remove (qos->priv->data, item);
	up_qos_latency_perhaps_changed (qos, up_qos_item_get_kind (item));

	/* TODO: if persistent remove from datadase */

	g_signal_emit (qos, signals [REQUESTS_CHANGED], 0);
out:
	if (subject != NULL)
		g_object_unref (subject);
	g_free (sender);
}

/**
 * up_qos_get_latency:
 *
 * Gets the current latency
 **/
gboolean
up_qos_get_latency (UpQos *qos, const gchar *type_text, gint *value, GError **error)
{
	UpQosKind type;

	/* get correct data */
	type = up_qos_kind_from_string (type_text);
	if (type == UP_QOS_KIND_UNKNOWN) {
		g_set_error (error, UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "type invalid: %s", type_text);
		return FALSE;
	}

	/* get the lowest value for this type */
	*value = up_qos_get_lowest (qos, type);
	return TRUE;
}

/**
 * up_qos_set_minimum_latency:
 **/
void
up_qos_set_minimum_latency (UpQos *qos, const gchar *type_text, gint value, DBusGMethodInvocation *context)
{
	UpQosKind type;
	GError *error;

	/* type valid? */
	type = up_qos_kind_from_string (type_text);
	if (type == UP_QOS_KIND_UNKNOWN) {
		error = g_error_new (UP_DAEMON_ERROR, UP_DAEMON_ERROR_GENERAL, "type invalid: %s", type_text);
		dbus_g_method_return_error (context, error);
		return;
	}

	egg_debug ("setting %s minimum to %i", type_text, value);
	qos->priv->minimum[type] = value;

	/* may have changed */
	up_qos_latency_perhaps_changed (qos, type);
	dbus_g_method_return (context, NULL);
}

/**
 * up_qos_get_latency_requests:
 **/
gboolean
up_qos_get_latency_requests (UpQos *qos, GPtrArray **requests, GError **error)
{
	guint i;
	GPtrArray *data;
	UpQosItem *item;

	*requests = g_ptr_array_new ();
	data = qos->priv->data;
	for (i=0; i<data->len; i++) {
		GValue elem = {0};

		item = g_ptr_array_index (data, i);
		g_value_init (&elem, UP_QOS_REQUESTS_STRUCT_TYPE);
		g_value_take_boxed (&elem, dbus_g_type_specialized_construct (UP_QOS_REQUESTS_STRUCT_TYPE));
		dbus_g_type_struct_set (&elem,
					0, up_qos_item_get_cookie (item),
					1, up_qos_item_get_uid (item),
					2, up_qos_item_get_pid (item),
					3, up_qos_item_get_cmdline (item),
					4, 0, //up_qos_item_get_timespec (item),
					5, up_qos_item_get_persistent (item),
					6, up_qos_kind_to_string (up_qos_item_get_kind (item)),
					7, up_qos_item_get_value (item),
					G_MAXUINT);
		g_ptr_array_add (*requests, g_value_get_boxed (&elem));
	}

//	dbus_g_method_return (context, requests);
//	g_ptr_array_foreach (*requests, (GFunc) g_value_array_free, NULL);
//	g_ptr_array_unref (*requests);

	return TRUE;
}


/**
 * up_qos_remove_dbus:
 **/
static void
up_qos_remove_dbus (UpQos *qos, const gchar *sender)
{
	guint i;
	GPtrArray *data;
	UpQosItem *item;

	/* remove *any* senders that match the sender */
	data = qos->priv->data;
	for (i=0; i<data->len; i++) {
		item = g_ptr_array_index (data, i);
		if (strcmp (up_qos_item_get_sender (item), sender) == 0) {
			egg_debug ("Auto-revoked idle qos on %s", sender);
			g_ptr_array_remove (qos->priv->data, item);
			up_qos_latency_perhaps_changed (qos, up_qos_item_get_kind (item));
		}
	}
}

/**
 * up_qos_name_owner_changed_cb:
 **/
static void
up_qos_name_owner_changed_cb (DBusGProxy *proxy, const gchar *name, const gchar *prev, const gchar *new, UpQos *qos)
{
	if (strlen (new) == 0)
		up_qos_remove_dbus (qos, name);
}

/**
 * up_qos_class_init:
 **/
static void
up_qos_class_init (UpQosClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = up_qos_finalize;

	signals [LATENCY_CHANGED] =
		g_signal_new ("latency-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpQosClass, latency_changed),
			      NULL, NULL, up_marshal_VOID__STRING_INT,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals [REQUESTS_CHANGED] =
		g_signal_new ("requests-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (UpQosClass, requests_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* introspection */
	dbus_g_object_type_install_info (UP_TYPE_QOS, &dbus_glib_up_qos_object_info);

	g_type_class_add_private (klass, sizeof (UpQosPrivate));
}

/**
 * up_qos_init:
 **/
static void
up_qos_init (UpQos *qos)
{
	guint i;
	GError *error = NULL;

	qos->priv = UP_QOS_GET_PRIVATE (qos);
	qos->priv->polkit = up_polkit_new ();
	qos->priv->data = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	/* TODO: need to load persistent values */

	/* setup lowest */
	for (i=0; i<UP_QOS_KIND_LAST; i++)
		qos->priv->last[i] = up_qos_get_lowest (qos, i);

	/* setup minimum */
	for (i=0; i<UP_QOS_KIND_LAST; i++)
		qos->priv->minimum[i] = -1;

	qos->priv->fd[UP_QOS_KIND_CPU_DMA] = open ("/dev/cpu_dma_latency", O_WRONLY);
	if (qos->priv->fd[UP_QOS_KIND_CPU_DMA] < 0)
		egg_warning ("cannot open cpu_dma device file");
	qos->priv->fd[UP_QOS_KIND_NETWORK] = open ("/dev/network_latency", O_WRONLY);
	if (qos->priv->fd[UP_QOS_KIND_NETWORK] < 0)
		egg_warning ("cannot open network device file");

	qos->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		egg_warning ("Cannot connect to bus: %s", error->message);
		g_error_free (error);
		return;
	}

	/* register on the bus */
	dbus_g_connection_register_g_object (qos->priv->connection, "/org/freedesktop/UPower/Policy", G_OBJECT (qos));

	/* watch NOC */
	qos->priv->proxy = dbus_g_proxy_new_for_name_owner (qos->priv->connection, DBUS_SERVICE_DBUS,
						 	    DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, NULL);
	dbus_g_proxy_add_signal (qos->priv->proxy, "NameOwnerChanged",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (qos->priv->proxy, "NameOwnerChanged",
				     G_CALLBACK (up_qos_name_owner_changed_cb), qos, NULL);
}

/**
 * up_qos_finalize:
 **/
static void
up_qos_finalize (GObject *object)
{
	UpQos *qos;
	guint i;

	g_return_if_fail (object != NULL);
	g_return_if_fail (UP_IS_QOS (object));

	qos = UP_QOS (object);
	qos->priv = UP_QOS_GET_PRIVATE (qos);

	/* close files */
	for (i=0; i<UP_QOS_KIND_LAST; i++) {
		if (qos->priv->fd[i] > 0)
			close (qos->priv->fd[i]);
	}
	g_ptr_array_unref (qos->priv->data);
	g_object_unref (qos->priv->proxy);

	g_object_unref (qos->priv->polkit);

	G_OBJECT_CLASS (up_qos_parent_class)->finalize (object);
}

/**
 * up_qos_new:
 **/
UpQos *
up_qos_new (void)
{
	UpQos *qos;
	qos = g_object_new (UP_TYPE_QOS, NULL);
	return UP_QOS (qos);
}

