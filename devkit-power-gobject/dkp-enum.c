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

#include <glib.h>
#include <string.h>

#include "dkp-enum.h"

/**
 * dkp_device_type_to_text:
 **/
const gchar *
dkp_device_type_to_text (DkpDeviceType type_enum)
{
	const gchar *type = NULL;
	switch (type_enum) {
	case DKP_DEVICE_TYPE_LINE_POWER:
		type = "line-power";
		break;
	case DKP_DEVICE_TYPE_BATTERY:
		type = "battery";
		break;
	case DKP_DEVICE_TYPE_UPS:
		type = "ups";
		break;
	case DKP_DEVICE_TYPE_MONITOR:
		type = "monitor";
		break;
	case DKP_DEVICE_TYPE_MOUSE:
		type = "mouse";
		break;
	case DKP_DEVICE_TYPE_KEYBOARD:
		type = "keyboard";
		break;
	case DKP_DEVICE_TYPE_PDA:
		type = "pda";
		break;
	case DKP_DEVICE_TYPE_PHONE:
		type = "phone";
		break;
	default:
		type = "unknown";
		break;
	}
	return type;
}

/**
 * dkp_device_type_from_text:
 **/
DkpDeviceType
dkp_device_type_from_text (const gchar *type)
{
	if (type == NULL)
		return DKP_DEVICE_TYPE_UNKNOWN;
	if (g_strcmp0 (type, "line-power") == 0)
		return DKP_DEVICE_TYPE_LINE_POWER;
	if (g_strcmp0 (type, "battery") == 0)
		return DKP_DEVICE_TYPE_BATTERY;
	if (g_strcmp0 (type, "ups") == 0)
		return DKP_DEVICE_TYPE_UPS;
	if (g_strcmp0 (type, "monitor") == 0)
		return DKP_DEVICE_TYPE_MONITOR;
	if (g_strcmp0 (type, "mouse") == 0)
		return DKP_DEVICE_TYPE_MOUSE;
	if (g_strcmp0 (type, "keyboard") == 0)
		return DKP_DEVICE_TYPE_KEYBOARD;
	if (g_strcmp0 (type, "pda") == 0)
		return DKP_DEVICE_TYPE_PDA;
	if (g_strcmp0 (type, "phone") == 0)
		return DKP_DEVICE_TYPE_PHONE;
	return DKP_DEVICE_TYPE_UNKNOWN;
}

/**
 * dkp_device_state_to_text:
 **/
const gchar *
dkp_device_state_to_text (DkpDeviceState state_enum)
{
	const gchar *state = NULL;
	switch (state_enum) {
	case DKP_DEVICE_STATE_CHARGING:
		state = "charging";
		break;
	case DKP_DEVICE_STATE_DISCHARGING:
		state = "discharging";
		break;
	case DKP_DEVICE_STATE_EMPTY:
		state = "empty";
		break;
	case DKP_DEVICE_STATE_FULLY_CHARGED:
		state = "fully-charged";
		break;
	case DKP_DEVICE_STATE_PENDING_CHARGE:
		state = "pending-charge";
		break;
	case DKP_DEVICE_STATE_PENDING_DISCHARGE:
		state = "pending-discharge";
		break;
	default:
		state = "unknown";
		break;
	}
	return state;
}

/**
 * dkp_device_state_from_text:
 **/
DkpDeviceState
dkp_device_state_from_text (const gchar *state)
{
	if (state == NULL)
		return DKP_DEVICE_STATE_UNKNOWN;
	if (g_strcmp0 (state, "charging") == 0)
		return DKP_DEVICE_STATE_CHARGING;
	if (g_strcmp0 (state, "discharging") == 0)
		return DKP_DEVICE_STATE_DISCHARGING;
	if (g_strcmp0 (state, "empty") == 0)
		return DKP_DEVICE_STATE_EMPTY;
	if (g_strcmp0 (state, "fully-charged") == 0)
		return DKP_DEVICE_STATE_FULLY_CHARGED;
	if (g_strcmp0 (state, "pending-charge") == 0)
		return DKP_DEVICE_STATE_PENDING_CHARGE;
	if (g_strcmp0 (state, "pending-discharge") == 0)
		return DKP_DEVICE_STATE_PENDING_DISCHARGE;
	return DKP_DEVICE_STATE_UNKNOWN;
}

/**
 * dkp_device_technology_to_text:
 **/
const gchar *
dkp_device_technology_to_text (DkpDeviceTechnology technology_enum)
{
	const gchar *technology = NULL;
	switch (technology_enum) {
	case DKP_DEVICE_TECHNOLOGY_LITHIUM_ION:
		technology = "lithium-ion";
		break;
	case DKP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
		technology = "lithium-polymer";
		break;
	case DKP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
		technology = "lithium-iron-phosphate";
		break;
	case DKP_DEVICE_TECHNOLOGY_LEAD_ACID:
		technology = "lead-acid";
		break;
	case DKP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
		technology = "nickel-cadmium";
		break;
	case DKP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
		technology = "nickel-metal-hydride";
		break;
	default:
		technology = "unknown";
		break;
	}
	return technology;
}

/**
 * dkp_device_technology_from_text:
 **/
DkpDeviceTechnology
dkp_device_technology_from_text (const gchar *technology)
{
	if (technology == NULL)
		return DKP_DEVICE_TECHNOLOGY_UNKNOWN;
	if (g_strcmp0 (technology, "lithium-ion") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_ION;
	if (g_strcmp0 (technology, "lithium-polymer") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER;
	if (g_strcmp0 (technology, "lithium-iron-phosphate") == 0)
		return DKP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE;
	if (g_strcmp0 (technology, "lead-acid") == 0)
		return DKP_DEVICE_TECHNOLOGY_LEAD_ACID;
	if (g_strcmp0 (technology, "nickel-cadmium") == 0)
		return DKP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM;
	if (g_strcmp0 (technology, "nickel-metal-hydride") == 0)
		return DKP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE;
	return DKP_DEVICE_TECHNOLOGY_UNKNOWN;
}

/**
 * dkp_qos_type_to_text:
 **/
const gchar *
dkp_qos_type_to_text (DkpQosType type)
{
	if (type == DKP_QOS_TYPE_NETWORK)
		return "network";
	if (type == DKP_QOS_TYPE_CPU_DMA)
		return "cpu_dma";
	return NULL;
}

/**
 * dkp_qos_type_from_text:
 **/
DkpQosType
dkp_qos_type_from_text (const gchar *type)
{
	if (g_strcmp0 (type, "network") == 0)
		return DKP_QOS_TYPE_NETWORK;
	if (g_strcmp0 (type, "cpu_dma") == 0)
		return DKP_QOS_TYPE_CPU_DMA;
	return DKP_QOS_TYPE_UNKNOWN;
}

