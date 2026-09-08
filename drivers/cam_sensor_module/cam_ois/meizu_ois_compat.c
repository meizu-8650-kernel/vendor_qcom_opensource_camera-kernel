// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meizu 21 Pro OIS compatibility.
 */

#include <linux/of.h>
#include <linux/string.h>

#include "cam_ois_dev.h"
#include "meizu_ois_compat.h"

static bool meizu_ois_dt_name_is(struct cam_ois_ctrl_t *o_ctrl,
				 const char *property, const char *expected)
{
	const char *name;
	struct device_node *node;

	if (!o_ctrl->soc_info.dev)
		return false;

	node = o_ctrl->soc_info.dev->of_node;
	return node && !of_property_read_string(node, property, &name) &&
		!strcmp(name, expected);
}

static bool meizu_ois_cci_is(struct cam_ois_ctrl_t *o_ctrl, u32 index,
			     u16 sid, enum cci_i2c_master_t master)
{
	struct cam_sensor_cci_client *client;

	if (o_ctrl->soc_info.index != index ||
	    o_ctrl->io_master_info.master_type != CCI_MASTER)
		return false;

	client = o_ctrl->io_master_info.cci_client;
	return client && client->sid == sid &&
		client->cci_i2c_master == master;
}

bool meizu_ois_needs_power_delay(struct cam_ois_ctrl_t *o_ctrl)
{
	if (!meizu_ois_cci_is(o_ctrl, 0, 0x40, MASTER_0))
		return false;

	return !strcmp(o_ctrl->ois_name, "M2481_rumba_ov50h40") ||
		meizu_ois_dt_name_is(o_ctrl, "cam-name", "OV50H40-ois");
}

bool meizu_ois_needs_byte_swapped_time(struct cam_ois_ctrl_t *o_ctrl)
{
	if (!meizu_ois_cci_is(o_ctrl, 2, 0x72, MASTER_1))
		return false;

	return !strcmp(o_ctrl->ois_name, "M2481_dw9784_imx754") ||
		meizu_ois_dt_name_is(o_ctrl, "ois-name", "754-2ois");
}
