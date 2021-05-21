#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/pid_namespace.h>
#include <linux/fs_struct.h>
#include <linux/slab.h>
#include <linux/rmap.h>
#include <linux/version.h>
#include "fs.h"

struct fs_file_node* fs_get_file_node(const struct task_struct *task) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    u32 req_mask = STATX_INO;
    unsigned int query_mask = AT_STATX_SYNC_AS_STAT;
#endif
    struct fs_file_node *fnode;
    struct inode *i;
    struct kstat stat;
    struct file *f;
    const struct inode_operations *op;

    if(!task)
        return NULL;

    /**
     * Not error, it is kernel task
     * and there is no file associated with it.
     */
    if(!task->mm)
        return NULL;

    /*
     * It's regular task and there is
     * executable file.
     */
    f = task->mm->exe_file;
    if(!f)
        return NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    i = f->f_dentry->d_inode;
#else
    i = f->f_inode;
#endif
    if(!i)
        return NULL;

    op = i->i_op;
    if(!op || !op->getattr)
        return NULL;

    memset(&stat, 0, sizeof(struct kstat));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    op->getattr(&f->f_path, &stat, req_mask, query_mask);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
    op->getattr(task_active_pid_ns(current)->proc_mnt, f->f_path.dentry, &stat);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
    op->getattr(task_active_pid_ns(current)->proc_mnt, f->f_dentry, &stat);
#endif

    fnode = kcalloc(1, sizeof(struct fs_file_node), GFP_KERNEL);
    if(!fnode)
        return NULL;

    /**
     * Once you know the inode it number it is easy to get the
     * executable full path by relying to find command:
     *
     * # find /path/to/mountpoint -inum <inode number> 2>/dev/null
     */
    fnode->ino = stat.ino;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
    fnode->filename = (const char *)f->f_dentry->d_name.name;
#else
    fnode->filename = (const char *)f->f_path.dentry->d_name.name;
#endif

    return fnode;
}
