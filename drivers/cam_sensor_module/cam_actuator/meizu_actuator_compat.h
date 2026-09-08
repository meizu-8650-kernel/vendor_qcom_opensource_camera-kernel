/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _MEIZU_ACTUATOR_COMPAT_H_
#define _MEIZU_ACTUATOR_COMPAT_H_

#include <linux/types.h>

struct cam_actuator_ctrl_t;
struct cam_sensor_i2c_reg_array;
struct i2c_settings_list;

bool meizu_actuator_prepare_settings(const struct cam_actuator_ctrl_t *a_ctrl,
				     const struct i2c_settings_list *input,
				     struct i2c_settings_list *output,
				     struct cam_sensor_i2c_reg_array *reg);

#endif /* _MEIZU_ACTUATOR_COMPAT_H_ */
