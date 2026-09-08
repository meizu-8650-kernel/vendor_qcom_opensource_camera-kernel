// SPDX-License-Identifier: GPL-2.0-only
/* Meizu 21 Pro actuator compatibility. */

#include <linux/swab.h>

#include "cam_actuator_dev.h"
#include "meizu_actuator_compat.h"

bool meizu_actuator_prepare_settings(const struct cam_actuator_ctrl_t *a_ctrl,
				     const struct i2c_settings_list *input,
				     struct i2c_settings_list *output,
				     struct cam_sensor_i2c_reg_array *reg)
{
	struct cam_sensor_cci_client *client;

	if (!a_ctrl || !input || !output || !reg ||
	    a_ctrl->soc_info.index != 0 ||
	    a_ctrl->io_master_info.master_type != CCI_MASTER)
		return false;

	client = a_ctrl->io_master_info.cci_client;
	if (!client || client->sid != 0x40 ||
	    client->cci_i2c_master != MASTER_0 ||
	    input->op_code != CAM_SENSOR_I2C_WRITE_RANDOM ||
	    input->i2c_settings.addr_type != CAMERA_SENSOR_I2C_TYPE_WORD ||
	    input->i2c_settings.data_type != CAMERA_SENSOR_I2C_TYPE_DWORD ||
	    input->i2c_settings.size != 1 ||
	    !input->i2c_settings.reg_setting ||
	    input->i2c_settings.reg_setting[0].reg_addr != 0x5040)
		return false;

	*output = *input;
	*reg = input->i2c_settings.reg_setting[0];
	reg->reg_data = swab32(reg->reg_data);
	output->i2c_settings.reg_setting = reg;

	return true;
}
