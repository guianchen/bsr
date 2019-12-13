#include "../../../bsr/bsr_int.h"
#ifdef _LIN_FAST_SYNC
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include "ext_fs.h"

#define EXT_MAGIC		0xEF53		/* EXT2_SUPER_MAGIC, EXT3_SUPER_MAGIC, EXT4_SUPER_MAGIC*/
#define XFS_MAGIC		0x58465342	/* XFS_SUPER_MAGIC, XFS_SB_MAGIC */


// for ext-fs debugging
int ext_used_blocks(unsigned int group, char * bitmap,
			unsigned int nbytes, unsigned int offset, unsigned int count)
{
	int p = 0;
	int used = 0;
	unsigned int i;
	unsigned int j;
	drbd_info(NO_OBJECT, "used_blocks : ");
	offset += group * nbytes;
	for (i = 0; i < count; i++) {
		if (test_bit_le(i, bitmap)) {
			used++;
			if (p)
				printk(", ");
			printk("%u", i + offset);
			for (j = ++i; j < count && test_bit_le(j, bitmap); j++)
				used++;
			if (--j != i) {
				printk("-");
				printk("%u", j + offset);
				i = j;
			}
			p = 1;
		}
	}
	
	printk("\n");
	return used;
}

// for ext-fs debugging
int ext_free_blocks(unsigned int group, char * bitmap,
			unsigned int nbytes, unsigned int offset, unsigned int count)
{
	int p = 0;
	unsigned int i;
	unsigned int j;
	int free = 0;
	drbd_info(NO_OBJECT, "free_blocks : ");
	offset += group * nbytes;
	for (i = 0; i < count; i++) {
		if (!test_bit_le(i, bitmap)) {
			free++;
			if (p)
				printk(", ");
			printk("%u", i + offset);
			for (j = ++i; j < count && !test_bit_le(j, bitmap); j++)
				free++;
			if (--j != i) {
				printk("-");
				printk("%u", j + offset);
				i = j;
			}
			p = 1;
		}
	}
	
	printk("\n");
	return free;
}



static char * read_superblock(struct file *fd)
{
	ssize_t ret;
	static char super_block[EXT_SUPER_BLOCK_OFFSET + EXT_SUPER_BLOCK_SIZE];
	
	/* read 2048 bytes.
	 *   ext superblock is starting at the 1024 bytes, size is 1024 bytes
	 *   xfs superblock is starting at the 0 byte, size is 512 bytes	
	*/
	ret = fd->f_op->read(fd, super_block, sizeof(super_block), &fd->f_pos); // TODO : change to __bread? vfs_read? kernel_read?
	
	if (ret < 0 || ret != sizeof(super_block)) {
		drbd_err(NO_OBJECT, "failed to read super_block (err=%ld)\n", ret);
		return NULL;
	}

	return super_block;
}

PVOLUME_BITMAP_BUFFER read_ext_bitmap(struct file *fd, struct ext_super_block *ext_sb)
{
	unsigned long group_count = 0;
	unsigned int group_no;
	unsigned int read_size;
	long long int bitmap_size;
	unsigned short desc_size;
	PVOLUME_BITMAP_BUFFER bitmap_buf;
	ULONGLONG total_block;
	ssize_t ret;
	loff_t offset, group_desc_offset;
	unsigned long free_blocks_co = 0;	
	unsigned long bytes_per_block;
	unsigned long first_data_block = le32_to_cpu(ext_sb->s_first_data_block);
	unsigned long blocks_per_group = le32_to_cpu(ext_sb->s_blocks_per_group);

	
	if (ext_sb->s_feature_incompat & cpu_to_le32(EXT_FEATURE_INCOMPAT_META_BG)) {
		drbd_info(NO_OBJECT, "EXT_FEATURE_INCOMPAT_META_BG is set. fastsync not support \n");
		// TODO : support MEAT_BG
		return NULL;
	}

	total_block = ((ULONGLONG)le32_to_cpu(ext_sb->s_blocks_count_hi) << 32) | le32_to_cpu(ext_sb->s_blocks_count_lo);
	bytes_per_block = EXT_DEFAULT_BLOCK_SIZE << le32_to_cpu(ext_sb->s_log_block_size);
	group_count = (total_block - first_data_block + blocks_per_group - 1) / blocks_per_group;

	bitmap_size = (total_block / BITS_PER_BYTE) + 1;
	bitmap_buf = (PVOLUME_BITMAP_BUFFER)kmalloc(sizeof(VOLUME_BITMAP_BUFFER) + bitmap_size, GFP_ATOMIC|__GFP_NOWARN, '');

	if (bitmap_buf == NULL) {
		drbd_err(NO_OBJECT, "bitmap_buf allocation failed\n");
		return NULL;
	}


	bitmap_buf->BitmapSize = bitmap_size;
	memset(bitmap_buf->Buffer, 0, bitmap_buf->BitmapSize);


	if ((ext_sb->s_feature_incompat & cpu_to_le32(EXT_FEATURE_INCOMPAT_64BIT))) {
		if (!ext_sb->s_desc_size) {
			drbd_err(NO_OBJECT, "wrong s_desc_size\n");
			goto fail_and_free;
		}

		desc_size = le16_to_cpu(ext_sb->s_desc_size);
	}
	else {
		desc_size = EXT_DEFAULT_DESC_SIZE;
	}

	if (debug_fast_sync) {
		drbd_info(NO_OBJECT, "=============================\n");
		drbd_info(NO_OBJECT, "first_data_block : %lu \n", first_data_block);
		drbd_info(NO_OBJECT, "total block count : %llu \n", total_block);	
		drbd_info(NO_OBJECT, "blocks_per_group : %lu \n", blocks_per_group);
		drbd_info(NO_OBJECT, "group descriptor size : %u \n", desc_size);
		drbd_info(NO_OBJECT, "block size : %lu \n", bytes_per_block);
		drbd_info(NO_OBJECT, "bitmap size : %lld \n", bitmap_size);
		drbd_info(NO_OBJECT, "group count : %lu \n", group_count);
		drbd_info(NO_OBJECT, "=============================\n");
	}

	group_desc_offset = bytes_per_block * (first_data_block + 1);
	read_size = bytes_per_block;

	for (group_no = 0; group_no < group_count; group_no++) {
		struct ext_group_desc group_desc= {0,};
		unsigned int used = 0;
		unsigned int free = 0;
		unsigned int first_block = 0;
		unsigned int last_block = 0;
		bool block_uninit = false;
		unsigned long long bg_block_bitmap;
		
		
		if (debug_fast_sync) {
			first_block = group_no * blocks_per_group + first_data_block;
			last_block = first_block + (blocks_per_group - 1);
			if (last_block > total_block - 1) {
				last_block = total_block - 1;
			}
		}

		offset = fd->f_op->llseek(fd, group_desc_offset + group_no * desc_size, SEEK_SET);
		if (offset < 0) {
			drbd_err(NO_OBJECT, "failed to lseek group_descriptor (err=%lld)\n", offset);
			goto fail_and_free;
		}

		// read group descriptor
		ret = fd->f_op->read(fd, (char *)&group_desc, desc_size, &fd->f_pos);
		if (ret < 0 || ret != desc_size) {
			drbd_err(NO_OBJECT, "failed to read group_descriptor (err=%ld)\n", ret);
			goto fail_and_free;
		}	
		
		block_uninit = group_desc.bg_flags & cpu_to_le16(EXT_BG_BLOCK_UNINIT);
			
		if (!le32_to_cpu(group_desc.bg_block_bitmap_lo)) {
			drbd_err(NO_OBJECT, "failed to read bg_block_bitmap_lo\n");
			goto fail_and_free;
		}

		bg_block_bitmap = le32_to_cpu(group_desc.bg_block_bitmap_lo) |
					(desc_size >= EXT_MIN_DESC_SIZE_64BIT ?
					(ULONGLONG)le32_to_cpu(group_desc.bg_block_bitmap_hi) << 32 : 0);
		
		if (debug_fast_sync) {
			drbd_info(NO_OBJECT, "Group %u (Blocks %u ~ %u) \n", group_no, first_block, last_block);
			drbd_info(NO_OBJECT, "block bitmap : %llu\n", bg_block_bitmap);
			drbd_info(NO_OBJECT, "block bitmap offset : %llu\n", bg_block_bitmap * bytes_per_block);
		}


		if (block_uninit) {
			if (debug_fast_sync) {
				drbd_info(NO_OBJECT, "skip BLOCK_UNINIT group\n");
				drbd_info(NO_OBJECT, "=============================\n");
				free_blocks_co += bytes_per_block * BITS_PER_BYTE;
			}
			continue;
		}

		if (bytes_per_block * (group_no + 1) > bitmap_size)
				read_size = bitmap_size - (group_no * bytes_per_block);

		// Move position to bitmap block
		offset = fd->f_op->llseek(fd, bg_block_bitmap * bytes_per_block, SEEK_SET);
		if (offset < 0) {
			drbd_err(NO_OBJECT, "failed to lseek bitmap_block (err=%lld)\n", offset);
			goto fail_and_free;
		}


		// read bitmap block
		ret = fd->f_op->read(fd, &bitmap_buf->Buffer[bytes_per_block * group_no], read_size, &fd->f_pos);
		
		
		drbd_err(NO_OBJECT, "read bitmap_block (%ld)\n", ret);
		if (ret < 0 || ret != read_size) {
			drbd_err(NO_OBJECT, "failed to read bitmap_block (err=%ld)\n", ret);
			goto fail_and_free;
		}

		if (debug_fast_sync) {

			used = ext_used_blocks(group_no, &bitmap_buf->Buffer[bytes_per_block * group_no],
							blocks_per_group,
							first_data_block,
							last_block - first_block + 1);
			drbd_info(NO_OBJECT, "used block count : %d\n", used);

			free = ext_free_blocks(group_no, &bitmap_buf->Buffer[bytes_per_block * group_no],
							blocks_per_group,
							first_data_block, 
							last_block - first_block + 1);
			drbd_info(NO_OBJECT, "free block count : %d\n", free);
			drbd_info(NO_OBJECT, "=============================\n");
			free_blocks_co += free;
		}

	}
	if (debug_fast_sync) {
		drbd_info(NO_OBJECT, "free_blocks : %lu\n", free_blocks_co);
	}

	return bitmap_buf;

fail_and_free:
	if (bitmap_buf != NULL) {
		kfree(bitmap_buf);
		bitmap_buf = NULL;
	}
	
	return NULL;
}


bool is_ext_fs(struct ext_super_block *ext_sb)
{
	if (le16_to_cpu(ext_sb->s_magic) == EXT_MAGIC && 
		le32_to_cpu(ext_sb->s_blocks_count_lo) > 0 && 
		le32_to_cpu(ext_sb->s_blocks_per_group) > 0 && 
		le32_to_cpu(ext_sb->s_inodes_per_group) > 0 &&
		EXT_DEFAULT_BLOCK_SIZE << le32_to_cpu(ext_sb->s_log_block_size) > 0 &&
		le16_to_cpu(ext_sb->s_inode_size) > 0 ) {
		return true;
	}

	return false;
}


PVOID GetVolumeBitmap(struct drbd_device *device, ULONGLONG * ptotal_block, ULONG * pbytes_per_block)
{
	struct file *fd;
	PVOLUME_BITMAP_BUFFER bitmap_buf = NULL;
	char * super_block = NULL;

	char disk_name[512] = {0};
	
    mm_segment_t old_fs = get_fs();
	
	sprintf(disk_name, "/dev/bsr%d", device->minor);
    set_fs(KERNEL_DS);
	


	fd = filp_open(disk_name, O_RDONLY, 0);

	if (fd == NULL || IS_ERR(fd)) {
		drbd_err(device, "%s open failed\n", disk_name);
		goto fail;
	}

	super_block = read_superblock(fd);
	if (super_block == NULL) {		
		goto fail_and_close;
	}

	if (is_ext_fs((struct ext_super_block *)(super_block + EXT_SUPER_BLOCK_OFFSET))) {
		// for ext-filesystem
		struct ext_super_block *ext_sb = (struct ext_super_block *)(super_block + EXT_SUPER_BLOCK_OFFSET);

		*ptotal_block = le32_to_cpu(ext_sb->s_blocks_count_lo);
		*pbytes_per_block = EXT_DEFAULT_BLOCK_SIZE << le32_to_cpu(ext_sb->s_log_block_size);

		bitmap_buf = read_ext_bitmap(fd, ext_sb);
	}
	else /* if (is_xfs_fs((struct xfs_super_block *)super_block))*/ {
		// TODO : for xfs filesystem
	}


fail_and_close:
	filp_close(fd, NULL);
	set_fs(old_fs);
fail:
	if (bitmap_buf)
		return (PVOLUME_BITMAP_BUFFER)bitmap_buf;
	else 
		return NULL;
}
#else
PVOID GetVolumeBitmap(struct drbd_device *device, ULONGLONG * ptotal_block, ULONG * pbytes_per_block)
{
	return NULL;
}
#endif