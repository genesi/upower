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

#ifndef __DKP_DEVICE_H
#define __DKP_DEVICE_H

#include <glib-object.h>
#include <devkit-power-gobject/dkp-enum.h>

G_BEGIN_DECLS

#define DKP_TYPE_DEVICE		(dkp_device_get_type ())
#define DKP_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DKP_TYPE_DEVICE, DkpDevice))
#define DKP_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DKP_TYPE_DEVICE, DkpDeviceClass))
#define DKP_IS_DEVICE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DKP_TYPE_DEVICE))
#define DKP_IS_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DKP_TYPE_DEVICE))
#define DKP_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DKP_TYPE_DEVICE, DkpDeviceClass))
#define DKP_DEVICE_ERROR	(dkp_device_error_quark ())
#define DKP_DEVICE_TYPE_ERROR	(dkp_device_error_get_type ())

typedef struct DkpDevicePrivate DkpDevicePrivate;

typedef struct
{
	 GObject		 parent;
	 DkpDevicePrivate	*priv;
} DkpDevice;

typedef struct
{
	GObjectClass		 parent_class;
	void			(*changed)		(DkpDevice		*device,
							 gpointer		*obj);
	/*< private >*/
	/* Padding for future expansion */
	void (*_dkp_device_reserved1) (void);
	void (*_dkp_device_reserved2) (void);
	void (*_dkp_device_reserved3) (void);
	void (*_dkp_device_reserved4) (void);
	void (*_dkp_device_reserved5) (void);
	void (*_dkp_device_reserved6) (void);
	void (*_dkp_device_reserved7) (void);
	void (*_dkp_device_reserved8) (void);
} DkpDeviceClass;

GType		 dkp_device_get_type			(void);
DkpDevice	*dkp_device_new				(void);

const gchar	*dkp_device_get_object_path		(const DkpDevice	*device);
gboolean	 dkp_device_set_object_path		(DkpDevice		*device,
							 const gchar		*object_path,
							 GError			**error);
gboolean	 dkp_device_print			(const DkpDevice	*device);
gboolean	 dkp_device_refresh			(DkpDevice		*device,
							 GError			**error);
GPtrArray	*dkp_device_get_history			(const DkpDevice	*device,
							 const gchar		*type,
							 guint			 timespec,
							 guint			 resolution,
							 GError			**error);
GPtrArray	*dkp_device_get_statistics		(const DkpDevice	*device,
							 const gchar		*type,
							 GError			**error);

G_END_DECLS

#endif /* __DKP_DEVICE_H */

