/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _MEIZU_EEPROM_COMPAT_H_
#define _MEIZU_EEPROM_COMPAT_H_

struct cam_eeprom_ctrl_t;
struct cam_eeprom_memory_block_t;

int meizu_eeprom_read_cached_memory(struct cam_eeprom_ctrl_t *e_ctrl,
				    struct cam_eeprom_memory_block_t *block);

#endif /* _MEIZU_EEPROM_COMPAT_H_ */
