/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include "egg-debug.h"

#include "up-backend.h"
#include "up-daemon.h"
#include "up-device.h"
#include "up-device-list.h"
#include "up-history.h"
#include "up-native.h"
#include "up-polkit.h"
#include "up-qos.h"
#include "up-wakeups.h"

static void
up_test_native_func (void)
{
	const gchar *path;

	path = up_native_get_native_path (NULL);
	g_assert_cmpstr (path, ==, "/sys/dummy");
}

static void
up_test_backend_func (void)
{
	UpBackend *backend;

	backend = up_backend_new ();
	g_assert (backend != NULL);

	/* unref */
	g_object_unref (backend);
}

static void
up_test_daemon_func (void)
{
	UpDaemon *daemon;

	daemon = up_daemon_new ();
	g_assert (daemon != NULL);

	/* unref */
	g_object_unref (daemon);
}

static void
up_test_device_func (void)
{
	UpDevice *device;

	device = up_device_new ();
	g_assert (device != NULL);

	/* unref */
	g_object_unref (device);
}

static void
up_test_device_list_func (void)
{
	UpDeviceList *list;
	GObject *native;
	GObject *device;
	GObject *found;
	gboolean ret;

	list = up_device_list_new ();
	g_assert (list != NULL);

	/* add device */
	native = g_object_new (G_TYPE_OBJECT, NULL);
	device = g_object_new (G_TYPE_OBJECT, NULL);
	ret = up_device_list_insert (list, native, device);
	g_assert (ret);

	/* find device */
	found = up_device_list_lookup (list, native);
	g_assert (found != NULL);
	g_object_unref (found);

	/* remove device */
	ret = up_device_list_remove (list, device);
	g_assert (ret);

	/* unref */
	g_object_unref (native);
	g_object_unref (device);
	g_object_unref (list);
}

static void
up_test_history_func (void)
{
	UpHistory *history;

	history = up_history_new ();
	g_assert (history != NULL);

	/* unref */
	g_object_unref (history);
}

static void
up_test_polkit_func (void)
{
	UpPolkit *polkit;

	polkit = up_polkit_new ();
	g_assert (polkit != NULL);

	/* unref */
	g_object_unref (polkit);
}

static void
up_test_qos_func (void)
{
	UpQos *qos;

	qos = up_qos_new ();
	g_assert (qos != NULL);

	/* unref */
	g_object_unref (qos);
}

static void
up_test_wakeups_func (void)
{
	UpWakeups *wakeups;

	wakeups = up_wakeups_new ();
	g_assert (wakeups != NULL);

	/* unref */
	g_object_unref (wakeups);
}

int
main (int argc, char **argv)
{
	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* tests go here */
	g_test_add_func ("/power/backend", up_test_backend_func);
	g_test_add_func ("/power/device", up_test_device_func);
	g_test_add_func ("/power/device_list", up_test_device_list_func);
	g_test_add_func ("/power/history", up_test_history_func);
	g_test_add_func ("/power/native", up_test_native_func);
	g_test_add_func ("/power/polkit", up_test_polkit_func);
	g_test_add_func ("/power/qos", up_test_qos_func);
	g_test_add_func ("/power/wakeups", up_test_wakeups_func);
	g_test_add_func ("/power/daemon", up_test_daemon_func);

	return g_test_run ();
}

