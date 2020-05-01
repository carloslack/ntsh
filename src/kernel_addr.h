#ifndef __KERNEL_ADDR_H
#define __KERNEL_ADDR_H
#include <linux/version.h>
#include <linux/kernel_stat.h>
#include <linux/kallsyms.h>

static inline unsigned long
kall_load(const char* objname)
{
    return (unsigned long)kallsyms_lookup_name(objname);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
#define ATTACH_PID_SGN (struct task_struct *, enum pid_type, struct pid *)
#else
#define ATTACH_PID_SGN (struct task_struct *, enum pid_type)
#endif
#define VFS_RMDIR_SGN (struct inode *, struct dentry *)

#define K_FUNC_FACTORY(ret, name, sgn) ret (*k_##name) sgn
#define K_FUNC_FACTORY_LOAD(ret, name, sgn) k_##name = (ret(*)sgn)kall_load(#name)

K_FUNC_FACTORY(extern void, attach_pid, ATTACH_PID_SGN);
K_FUNC_FACTORY(extern void, vfs_rmdir, VFS_RMDIR_SGN);


void kall_load_addr(void);
#endif
