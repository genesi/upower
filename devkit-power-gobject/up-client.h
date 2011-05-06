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

#if !defined (__UPOWER_H_INSIDE__) && !defined (UP_COMPILATION)
#error "Only <devicekit-power.h> can be included directly."
#endif

#ifndef __DKP_CLIENT_H
#define __DKP_CLIENT_H

#include <glib-object.h>
#include <devkit-power-gobject/dkp-enum.h>
#include <devkit-power-gobject/dkp-device.h>

G_BEGIN_DECLS

#define DKP_TYPE_CLIENT			(dkp_client_get_type ())
#define DKP_CLIENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_CLIENT, DkpClient))
#define DKP_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_CLIENT, DkpClientClass))
#define DKP_IS_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_CLIENT))
#define DKP_IS_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_CLIENT))
#define DKP_CLIENT_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_CLIENT, DkpClientClass))
#define DKP_CLIENT_ERROR		(dkp_client_error_quark ())
#define DKP_CLIENT_TYPE_ERROR		(dkp_client_error_get_type ())

typedef struct DkpClientPrivate DkpClientPrivate;

typedef struct
{
	 GObject		 parent;
	 DkpClientPrivate	*priv;
} DkpClient;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*device_added)		(DkpClient		*client,
							 const DkpDevice	*device);
	void			(*device_changed)      	(DkpClient		*client,
							 const DkpDevice	*device);
	void			(*device_removed)      	(DkpClient		*client,
							 const DkpDevice	*device);
	void			(*changed)              (DkpClient		*client);
	/*< private >*/
	/* Padding for future expansion */
	void (*_dkp_client_reserved1) (void);
	void (*_dkp_client_reserved2) (void);
	void (*_dkp_client_reserved3) (void);
	void (*_dkp_client_reserved4) (void);
	void (*_dkp_client_reserved5) (void);
	void (*_dkp_client_reserved6) (void);
	void (*_dkp_client_reserved7) (void);
	void (*_dkp_client_reserved8) (void);
} DkpClientClass;

GType		 dkp_client_get_type			(void);
DkpClient	*dkp_client_new				(void);
GPtrArray	*dkp_client_enumerate_devices		(DkpClient		*client,
							 GError			**error);
gboolean	 dkp_client_suspend			(DkpClient		*client,
							 GError			**error);
gboolean	 dkp_client_hibernate			(DkpClient		*client,
							 GError			**error);
#ifndef UP_DISABLE_DEPRECATED
const gchar	*dkp_client_get_daemon_version		(DkpClient		*client);
gboolean	 dkp_client_can_hibernate		(DkpClient		*client);
gboolean	 dkp_client_lid_is_closed		(DkpClient		*client);
gboolean	 dkp_client_can_suspend			(DkpClient		*client);
gboolean	 dkp_client_on_battery			(DkpClient		*client);
gboolean	 dkp_client_on_low_battery		(DkpClient		*client);
#endif

G_END_DECLS

#endif /* __DKP_CLIENT_H */

