#ifndef _RANDBYTES_H
#define _RANDBYTES_H

#include <linux/slab.h>
#include <linux/random.h>

/**
 * This function allocates dynamic memory
 * and it is up to the caller to free it.
 */
static inline char* ntsh_get_random_bytes(size_t size) {
    static int i = 0;
    char *buf = NULL;
    if(!size) {
        printk(KERN_ERR "Wrong size parameter!!\n");
        return NULL;
    }

    buf = kcalloc(1, size+1, GFP_KERNEL);
    if(!buf) {
        printk(KERN_ERR "Could not allocate memory!\n");
        return NULL;
    }

    /**
     * HW implemented get_random_bytes_arch() is faster
     * but is likely to be already backdoored by NSA.
     */
    get_random_bytes(buf, size);
    buf[0] = '_'; /* overwrite first byte, @see simple_strtol() */

    for(i = 1; i < size; ++i) {
        int byte = (int)buf[i];
        byte < 0 ? byte = ~byte : 0;
        /* ascii from 48 to 90 */
        buf[i] = byte % (90 - (48 + 1)) + 48;
    }
    return buf;
}

#endif
