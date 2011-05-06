/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#include <glib.h>
#include <glib-object.h>
#include <string.h>
#include <stdlib.h>

#include "dkp-enum.h"
#include "dkp-qos-obj.h"

/**
 * dkp_qos_obj_clear_internal:
 **/
static void
dkp_qos_obj_clear_internal (DkpQosObj *obj)
{
	obj->uid = 0;
	obj->pid = 0;
	obj->sender = NULL; /* only used in the daemon */
	obj->cmdline = NULL;
	obj->cookie = 0;
	obj->timespec = 0;
	obj->persistent = FALSE;
	obj->type = DKP_QOS_TYPE_UNKNOWN;
	obj->value = 0;
}

/**
 * dkp_qos_obj_copy:
 **/
DkpQosObj *
dkp_qos_obj_copy (const DkpQosObj *cobj)
{
	DkpQosObj *obj;
	obj = g_new0 (DkpQosObj, 1);
	obj->cookie = cobj->cookie;
	return obj;
}

/**
 * dkp_qos_obj_equal:
 **/
gboolean
dkp_qos_obj_equal (const DkpQosObj *obj1, const DkpQosObj *obj2)
{
	if (obj1->cookie == obj2->cookie)
		return TRUE;
	return FALSE;
}

/**
 * dkp_qos_obj_print:
 **/
gboolean
dkp_qos_obj_print (const DkpQosObj *obj)
{
	g_print ("cookie:%i\n", obj->cookie);
	return TRUE;
}

/**
 * dkp_qos_obj_new:
 **/
DkpQosObj *
dkp_qos_obj_new (void)
{
	DkpQosObj *obj;
	obj = g_new0 (DkpQosObj, 1);
	dkp_qos_obj_clear_internal (obj);
	return obj;
}

/**
 * dkp_qos_obj_free:
 **/
void
dkp_qos_obj_free (DkpQosObj *obj)
{
	if (obj == NULL)
		return;
	g_free (obj->cmdline);
	g_free (obj->sender);
	g_free (obj);
	return;
}


