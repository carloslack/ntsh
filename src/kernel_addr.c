#include "kernel_addr.h"

K_FUNC_FACTORY(void, attach_pid, ATTACH_PID_SGN) = NULL;
K_FUNC_FACTORY(void, vfs_rmdir, VFS_RMDIR_SGN) = NULL;

void kall_load_addr(void)
{
    K_FUNC_FACTORY_LOAD(void, attach_pid, ATTACH_PID_SGN);
    K_FUNC_FACTORY_LOAD(void, vfs_rmdir, VFS_RMDIR_SGN);
}
