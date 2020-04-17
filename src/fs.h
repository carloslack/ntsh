#ifndef WALLY_FS_H
#define WALLY_FS_H

struct fs_file_node
{
    unsigned long long ino;
    const char *filename;
};

/**
 * Return hidden filename plus inode number.
 * This function allocates data that must
 * be freed when no longer needed.
 */
struct fs_file_node *fs_get_file_node(const struct task_struct *task);

#endif //WALLY_FS_H
