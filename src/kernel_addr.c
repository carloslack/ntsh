#include "kernel_addr.h"

K_FUNC_FACTORY(void, attach_pid, ATTACH_PID_SGN) = NULL;

void kall_load_addr(void)
{
    K_FUNC_FACTORY_LOAD(void, attach_pid, ATTACH_PID_SGN);
}
