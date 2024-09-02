#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/hdreg.h>
#include <linux/blk-mq.h>

#include "graid.h"

#define VPCIEDISK_MAJOR 400
#define VPCIEDISK_MINORS 1
#define VPCIEDISK_NAME "graiddisk"

#define SECTOR_TO_BYTE(sector) ((sector) << KERNEL_SECTOR_SHIFT)

#define stripe_num(sector_num) ((sector_num) >> SECTORS_IN_STRIPE_SHIFT)
#define stripe_group_num(sector_num, cnt_dev) (stripe_num(sector_num) / cnt_dev)
#define device_num(stripe, cnt_dev) ((stripe) % (cnt_dev))
#define stripe_sta_sector(sector_num) (stripe_num(sector_num) << SECTORS_IN_STRIPE_SHIFT)
#define stripe_end_sector(sector_num) (stripe_sta_sector(sector_num) + SECTORS_IN_STRIPE - 1)
#define sector_whole_to_i(sector_sta, cnt_dev) (((sector_sta) & 0x7) + ((stripe_num(sector_sta) / (cnt_dev)) << SECTORS_IN_STRIPE_SHIFT))
#define i_stripe_sta_sector(sector_num, cnt_dev) sector_whole_to_i(stripe_sta_sector(sector_num), cnt_dev)
#define i_stripe_end_sector(sector_num, cnt_dev) sector_whole_to_i(stripe_end_sector(sector_num), cnt_dev)
// 为方便计算扇区归属的条带，本设备中采用两侧都闭的区间

#define DISK_INFO(string, args...) printk(KERN_INFO "%s: " string, VPCIEDISK_NAME, ##args)
#define DISK_DEBUG(string, args...) printk(KERN_DEBUG "%s: " string, VPCIEDISK_NAME, ##args)
#define DISK_ERROR(string, args...) printk(KERN_ERR "%s: " string, VPCIEDISK_NAME, ##args)

struct blkdev_config {
    unsigned int nr_nvme_disks;
    uint64_t size_nvme_disk;
    unsigned int nvme_major;
    unsigned int nvme_minor_verify;
    unsigned int nvme_minor[32];
};

// tackle signle bio instead of using blk-mq technique, which needs tag_set
struct graidblk_dev {
    struct blkdev_config config;

    uint64_t size;
    unsigned int disk_cnt;
    struct block_device *bdev[32];
    struct block_device *bdev_verify;

    spinlock_t lock;
    struct request_queue *queue;
    struct gendisk *gd;
};

bool pcievdrv_submit_verify(struct bio *bio, unsigned int devi, struct graid_dev *dev);

int vpciedisk_init(struct graid_dev *graid_dev);
void vpciedisk_exit(struct graid_dev *graid_dev);

#endif