#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <linux/ns_common.h>
#include <linux/mnt_namespace.h>
#include <linux/nsproxy.h>
#include <linux/fs_struct.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/uh_inform.h>
#include "../fs/mount.h"


struct feature_uh {
	int feature_id;
	int feature_type; //1.feature 2.dmv type 3.partition(mounted device)
	int feature_state;
	char *feature_id_str;
	char *feature_state_str;
};

static char* feature_state_char[UH_INFORM_SIZE_STATE] = {
	/*UH_INFORM_INACTIVE*/
	": inactive",
	/*UH_INFORM_ACTIVE*/
	": active",
	/*UH_INFORM_DMV_ALTA*/
	": alta",
	/*UH_INFORM_DMV_LITE*/
	": lite",
	/*UH_INFORM_DMV_FULL*/
	": full",
	/*DMV_MOUNTED*/
	": dm-verity",
	/*DMV_UNMOUNTED*/
	": normal",
	/*UH_INFORM_INFORM_ERROR*/
	": type error"
};

static struct feature_uh feature_uh_s[UH_INFORM_SIZE_FEATURE];

static int feature_init(int feature_id, int feature_type, int feature_state,
		char* feature_id_str ) {
	if(feature_id >= UH_INFORM_SIZE_FEATURE ||
		feature_type >= UH_INFORM_SIZE_TYPE ||
		feature_state >= UH_INFORM_SIZE_STATE ||
		feature_id_str == NULL )
		return UH_INFORM_FALSE;
	feature_uh_s[feature_id].feature_id = feature_id;
	feature_uh_s[feature_id].feature_type = feature_type;
	feature_uh_s[feature_id].feature_id_str = feature_id_str;
	feature_uh_s[feature_id].feature_state = feature_state;
	feature_uh_s[feature_id].feature_state_str = feature_state_char[feature_state];
	return UH_INFORM_TRUE;
}

static int feature_get_config(void) {
	int ret = UH_INFORM_TRUE;
#ifdef CONFIG_RKP_CFP_JOPP
	ret |= feature_set_state(FEATURE_JOPP, UH_INFORM_ACTIVE);
#endif
#ifdef CONFIG_RKP_CFP_ROPP
	ret |= feature_set_state(FEATURE_ROPP, UH_INFORM_ACTIVE);
#endif
#ifdef CONFIG_UH_RKP
	ret |= feature_set_state(FEATURE_RKP, UH_INFORM_ACTIVE);
#endif
#ifdef CONFIG_UH_RKP_KDP
	ret |= feature_set_state(FEATURE_KDP, UH_INFORM_ACTIVE);
#endif
#ifdef CONFIG_DM_VERITY
	ret |= feature_set_state(FEATURE_DMV, UH_INFORM_DMV_FULL);
#endif
#ifdef DMV_ALTA
	ret |= feature_set_state(FEATURE_DMV, UH_INFORM_DMV_ALTA);
#endif
#ifdef DMV_LITE
	ret |= feature_set_state(FEATURE_DMV, UH_INFORM_DMV_LITE);
#endif
	return ret;
}

static int uh_inform_init(void) {
	int ret = UH_INFORM_TRUE;
	ret |= feature_init(FEATURE_ROPP,UH_INFORM_NORMAL_FEATURE ,UH_INFORM_INACTIVE, "ropp");
	ret |= feature_init(FEATURE_JOPP,UH_INFORM_NORMAL_FEATURE ,UH_INFORM_INACTIVE, "jopp");
	ret |= feature_init(FEATURE_RKP,UH_INFORM_NORMAL_FEATURE ,UH_INFORM_INACTIVE, "rkp");
	ret |= feature_init(FEATURE_KDP,UH_INFORM_NORMAL_FEATURE ,UH_INFORM_INACTIVE, "kdp");
	ret |= feature_init(FEATURE_DMV,UH_INFORM_DMV_FEATURE ,UH_INFORM_INACTIVE, "dmv");
	ret |= feature_init(DMV_MOUNTED_ODM,UH_INFORM_PARTITION ,DMV_UNMOUNTED, "odm");
	ret |= feature_init(DMV_MOUNTED_SYSTEM,UH_INFORM_PARTITION ,DMV_UNMOUNTED, "system");
	ret |= feature_init(DMV_MOUNTED_VENDOR,UH_INFORM_PARTITION ,DMV_UNMOUNTED, "vendor");
	ret |= feature_get_config();
	return ret;
}

int feature_set_state(int feature_id, int feature_state) {
	if(feature_id >= UH_INFORM_SIZE_FEATURE ||
		feature_state >= UH_INFORM_SIZE_STATE )
		return UH_INFORM_FALSE;
	feature_uh_s[feature_id].feature_state_str = feature_state_char[feature_state];

	if(feature_uh_s[feature_id].feature_state_str == NULL)
		return UH_INFORM_FALSE;
	return UH_INFORM_TRUE;
}


#if defined(CONFIG_DEBUG_FS)

static const char* dm_str = "/dev/block/dm";

static int find_mount_dir(int feature_id) {
    struct list_head *head;
    struct nsproxy *nsp = current->nsproxy;
    struct mnt_namespace *ns = nsp->mnt_ns;
    char *dir = feature_uh_s[feature_id].feature_id_str;
    if(feature_id >= UH_INFORM_SIZE_FEATURE)
		return UH_INFORM_FALSE;
    list_for_each(head, &(ns->list)) {
		struct mount *r = list_entry(head, struct mount, mnt_list);
		if(!strncmp(dir,r->mnt_mountpoint->d_iname,strlen(dir))) {
			if(!strncmp(dm_str,r->mnt_devname,strlen(dm_str)))
				return DMV_MOUNTED;
			else
				return DMV_UNMOUNTED;
		}
    }
    return UH_INFORM_INFORM_ERROR;
}

static char *get_feature_id_str(int feature_id) {
	if(feature_id >= UH_INFORM_SIZE_FEATURE)
		return NULL;
	return feature_uh_s[feature_id].feature_id_str;
}

static char *get_feature_state_str(int feature_id) {
	if(feature_id >= UH_INFORM_SIZE_FEATURE)
		return NULL;
	return feature_uh_s[feature_id].feature_state_str;
}

static int get_partition_info(void) {
	int feature_n;
	int ret = UH_INFORM_TRUE;
	for(feature_n = 0; feature_n < UH_INFORM_SIZE_FEATURE; feature_n++) {
		if(feature_uh_s[feature_n].feature_type == UH_INFORM_PARTITION)	{
			ret |= feature_set_state(feature_n, find_mount_dir(feature_n));
		}
	}
	return ret;
}

static void print_feature(struct seq_file *m) {
	int feature_n;
	for(feature_n = 0; feature_n < UH_INFORM_SIZE_FEATURE; feature_n++)
		seq_printf(m, "%s %s\n", get_feature_id_str(feature_n),
			get_feature_state_str(feature_n));
}

static int uh_inform_debug_show(struct seq_file *m, void *private) {
	int ret = UH_INFORM_TRUE;

	ret |= get_partition_info();
	print_feature(m);
	return 0;
}

static int uh_inform_debug_open(struct inode *inode, struct file *file) {
	return single_open(file, uh_inform_debug_show, inode->i_private);
}

static ssize_t uh_inform_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos) {
	u8 buffer[2] = {0,};
	unsigned long missing;
	u8 feature_id = 0, arg = 0;

	if (sizeof(buffer) < count)
		return -EFAULT;

	missing = copy_from_user(buffer, buf, count);
	if (missing)
		return -EFAULT;

	feature_id = buffer[0];

	if(count >= 2)
		arg = buffer[1];

	if(feature_id > UH_INFORM_SIZE_FEATURE)
		return -EFAULT;

	feature_set_state(feature_id, arg);

	return count;
}

static const struct file_operations uh_inform_debug_fops = {
	.open = uh_inform_debug_open,
	.read = seq_read,
	.write = uh_inform_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init uh_inform_init_debugfs(void) {
	struct dentry *root = debugfs_create_dir("uh", NULL);
	int ret = UH_INFORM_TRUE;
	ret |= uh_inform_init();

	if (!root)
		return -ENXIO;
	debugfs_create_file("uh_inform", S_IRUGO, root, NULL, &uh_inform_debug_fops);

	return ret;
}

device_initcall(uh_inform_init_debugfs);
#endif /*CONFIG_DEBUG_FS*/


