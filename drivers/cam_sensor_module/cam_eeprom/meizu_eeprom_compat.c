// SPDX-License-Identifier: GPL-2.0-only
/* Meizu 21 Pro EEPROM compatibility. */

#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/string.h>

#include "cam_eeprom_dev.h"
#include "meizu_eeprom_compat.h"

#define MEIZU_EEPROM_CACHE_PARTLABEL        "PARTLABEL=camera"
#define MEIZU_EEPROM_CACHE_SLOT_SIZE        0x6400
#define MEIZU_EEPROM_CACHE_DATA_SLOT_BASE   10
#define MEIZU_EEPROM_CACHE_METADATA_SIZE    131
#define MEIZU_EEPROM_CACHE_RELOAD_OFFSET    128
#define MEIZU_EEPROM_CACHE_LENGTH_OFFSET    129
#define MEIZU_EEPROM_CACHE_ID_OFFSET        29
#define MEIZU_EEPROM_CACHE_ID_SIZE          10
#define MEIZU_EEPROM_CACHE_IDENTITY_SIZE    40

static int meizu_eeprom_cache_read(struct block_device *bdev, u64 offset,
				   void *buffer, size_t length)
{
	struct address_space *mapping;
	loff_t device_size;
	char *destination = buffer;
	int rc = 0;

	if (!bdev || !buffer || !length)
		return -EINVAL;

	device_size = bdev_nr_bytes(bdev);
	if (device_size < 0 || offset > device_size ||
	    length > device_size - offset)
		return -ERANGE;

	mapping = bdev->bd_inode->i_mapping;
	filemap_invalidate_lock_shared(mapping);
	while (length) {
		struct page *page;
		size_t page_offset = offset & ~PAGE_MASK;
		size_t count = min_t(size_t, length,
			PAGE_SIZE - page_offset);

		page = read_cache_page_gfp(mapping, offset >> PAGE_SHIFT,
					   GFP_KERNEL);
		if (IS_ERR(page)) {
			rc = PTR_ERR(page);
			break;
		}

		memcpy_from_page(destination, page, page_offset, count);
		put_page(page);

		destination += count;
		offset += count;
		length -= count;
	}
	filemap_invalidate_unlock_shared(mapping);

	return rc;
}

static int meizu_eeprom_cache_layout(struct cam_eeprom_ctrl_t *e_ctrl,
				     struct cam_eeprom_memory_block_t *block,
				      struct cam_eeprom_memory_map_t **identity_map,
				      u64 *metadata_offset, u64 *data_offset)
{
	int j;

	if (!e_ctrl || !block || !block->map || !block->mapdata ||
	    !block->num_map || !block->num_data)
		return -EINVAL;

	if (e_ctrl->soc_info.index >= MEIZU_EEPROM_CACHE_DATA_SLOT_BASE ||
	    block->num_data > MEIZU_EEPROM_CACHE_SLOT_SIZE)
		return -ERANGE;

	*identity_map = NULL;
	for (j = 0; j < block->num_map; j++) {
		if (!block->map[j].mem.valid_size)
			continue;
		if (*identity_map)
			return -EINVAL;
		*identity_map = &block->map[j];
	}

	if (!*identity_map ||
	    (*identity_map)->mem.valid_size < MEIZU_EEPROM_CACHE_IDENTITY_SIZE ||
	    (*identity_map)->mem.valid_size != block->num_data)
		return -EINVAL;

	*metadata_offset = (u64)e_ctrl->soc_info.index *
		MEIZU_EEPROM_CACHE_SLOT_SIZE;
	*data_offset = (u64)(e_ctrl->soc_info.index +
		MEIZU_EEPROM_CACHE_DATA_SLOT_BASE) *
		MEIZU_EEPROM_CACHE_SLOT_SIZE;

	return 0;
}

static bool meizu_eeprom_cache_identity_valid(const u8 *identity)
{
	return memchr_inv(identity + MEIZU_EEPROM_CACHE_ID_OFFSET, 0,
		MEIZU_EEPROM_CACHE_ID_SIZE) &&
		memchr_inv(identity + MEIZU_EEPROM_CACHE_ID_OFFSET, 0xff,
			   MEIZU_EEPROM_CACHE_ID_SIZE);
}

int meizu_eeprom_read_cached_memory(struct cam_eeprom_ctrl_t *e_ctrl,
				    struct cam_eeprom_memory_block_t *block)
{
	struct cam_eeprom_memory_map_t *identity_map;
	struct cam_eeprom_soc_private *eb_info;
	struct cam_eeprom_i2c_info_t saved_i2c_info;
	struct block_device *bdev;
	u8 metadata[MEIZU_EEPROM_CACHE_METADATA_SIZE];
	u8 identity[MEIZU_EEPROM_CACHE_IDENTITY_SIZE];
	u64 metadata_offset;
	u64 data_offset;
	dev_t dev;
	u16 cached_size;
	bool i2c_info_changed = false;
	int rc;

	rc = meizu_eeprom_cache_layout(e_ctrl, block, &identity_map,
				       &metadata_offset, &data_offset);
	if (rc)
		return rc;

	dev = name_to_dev_t(MEIZU_EEPROM_CACHE_PARTLABEL);
	if (!dev)
		return -ENODEV;

	bdev = blkdev_get_by_dev(dev, FMODE_READ, NULL);
	if (IS_ERR(bdev))
		return PTR_ERR(bdev);

	rc = meizu_eeprom_cache_read(bdev, metadata_offset, metadata,
				     sizeof(metadata));
	if (rc)
		goto put_bdev;

	cached_size = metadata[MEIZU_EEPROM_CACHE_LENGTH_OFFSET] |
		(metadata[MEIZU_EEPROM_CACHE_LENGTH_OFFSET + 1] << 8);
	if (metadata[MEIZU_EEPROM_CACHE_RELOAD_OFFSET] != 0 ||
	    cached_size != block->num_data) {
		rc = -ENODATA;
		goto put_bdev;
	}

	eb_info = e_ctrl->soc_info.soc_private;
	if (!eb_info) {
		rc = -EINVAL;
		goto put_bdev;
	}

	if (identity_map->saddr) {
		saved_i2c_info = eb_info->i2c_info;
		i2c_info_changed = true;
		eb_info->i2c_info.slave_addr = identity_map->saddr;
		rc = cam_eeprom_update_i2c_info(e_ctrl, &eb_info->i2c_info);
		if (rc)
			goto put_bdev;
	}

	rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				    identity_map->mem.addr, identity,
				    identity_map->mem.addr_type,
				    identity_map->mem.data_type,
				    sizeof(identity));
	if (rc < 0)
		goto put_bdev;

	if (!meizu_eeprom_cache_identity_valid(identity) ||
	    metadata[1] != identity[1] ||
	    memcmp(metadata + MEIZU_EEPROM_CACHE_ID_OFFSET,
		   identity + MEIZU_EEPROM_CACHE_ID_OFFSET,
		   MEIZU_EEPROM_CACHE_ID_SIZE)) {
		rc = -ENODATA;
		goto put_bdev;
	}

	rc = meizu_eeprom_cache_read(bdev, data_offset, block->mapdata,
				     block->num_data);
	if (!rc && (block->mapdata[1] != identity[1] ||
		    memcmp(block->mapdata + MEIZU_EEPROM_CACHE_ID_OFFSET,
			   identity + MEIZU_EEPROM_CACHE_ID_OFFSET,
		   MEIZU_EEPROM_CACHE_ID_SIZE)))
		rc = -EBADMSG;

put_bdev:
	if (i2c_info_changed) {
		int restore_rc;

		eb_info->i2c_info = saved_i2c_info;
		restore_rc = cam_eeprom_update_i2c_info(e_ctrl,
							&eb_info->i2c_info);
		if (!rc)
			rc = restore_rc;
	}
	blkdev_put(bdev, FMODE_READ);
	return rc;
}
