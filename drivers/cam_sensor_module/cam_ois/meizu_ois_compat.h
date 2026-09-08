/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _MEIZU_OIS_COMPAT_H_
#define _MEIZU_OIS_COMPAT_H_

struct cam_ois_ctrl_t;

bool meizu_ois_needs_power_delay(struct cam_ois_ctrl_t *o_ctrl);
bool meizu_ois_needs_byte_swapped_time(struct cam_ois_ctrl_t *o_ctrl);

#endif /* _MEIZU_OIS_COMPAT_H_ */
