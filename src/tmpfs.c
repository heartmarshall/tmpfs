#define FUSE_USE_VERSION 26

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "filesystem.h"

int tmp_getattr(const char *path, struct stat *statbuf)
{
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (node == NULL){
        return -ENOENT;
    }
    printf("getattr: node id: %d\n", node->node_number);
    memcpy(statbuf, node->st, sizeof(struct stat));
    return 0;
}


int tmp_mknod(const char *path, mode_t mode, dev_t dev)
{
    struct fuse_context* ctx = fuse_get_context();
    Filesystem* fs = ctx->private_data;
    int node_number = allocate_inode_number(fs->inodes_numbers_tracker);
    
    Inode* parent_dir_node = get_parent_directory(path, fs->inodes_list);
    if (parent_dir_node == NULL) {
        errno = ENOENT;
        return -1;
    }

    const char* name = get_last_name(path);
    Directory* parent_dir = parent_dir_node->data;
    add_entry(parent_dir, name, node_number);

    struct stat* st = malloc(sizeof(struct stat));
    st->st_mode = mode;
    st->st_nlink = 1;
    st->st_ino = node_number;
    st->st_uid = ctx->uid;
    st->st_gid = ctx->gid;
    Inode* node = init_inode(node_number, st, NULL, parent_dir_node);
    if (node == NULL) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}


int tmp_mkdir(const char *path, mode_t mode)
{   
    printf("MKDIR: %s\n", path);
    struct fuse_context* ctx = fuse_get_context();
    Filesystem* fs = ctx->private_data;
    
    struct stat* dir_stat = malloc(sizeof(struct stat));
    if (dir_stat == NULL) {
        errno = ENOMEM;
        return -1;
    }
    dir_stat->st_mode = mode | __S_IFDIR;
    dir_stat->st_uid = ctx->uid;
    dir_stat->st_gid = ctx->gid;

    Inode* parent_dir_node = get_parent_directory(path, fs->inodes_list);
    if (parent_dir_node == NULL) {
        errno = ENOENT;
        return -1;
    }
    
    Directory* dir_data = malloc(sizeof(Directory));
    if (dir_data == NULL) {
        errno = ENOMEM;
        return -1;
    }
    int node_number = allocate_inode_number(fs->inodes_numbers_tracker);
    if (node_number < 0) {
        errno = ENOSPC;
        return -1;
    }
    Inode* dir_node = init_inode(node_number, dir_stat, dir_data, parent_dir_node);
    if (dir_node == NULL) {
        errno = ENOMEM;
        return -1;
    }

    add_entry(dir_data, ".", node_number);
    dir_node->st->st_nlink++;
    add_entry(dir_data, "..", node_number);
    parent_dir_node->st->st_nlink++;

    return 0;
}



int tmp_link(const char *path, const char *newpath)
{
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (node == NULL) {
        errno = ENOENT;
        return -1;
    }
    if (is_dir(node)) {
        errno = EPERM;
        return -1;
    }
    if (!add_node_by_path(path, node, fs->inodes_list)) {
        errno = EIO;
        return -1;
    }
    return 0;
}


int tmp_unlink(const char *path)
{
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node) {
        errno = ENOENT;
        return -1;
    }
    if (is_dir(node)) {
        errno = EPERM; 
        return -1;
    }
    if (node->nopen) {
        errno = EBUSY;
        return -1;
    }
    if (!remove_node_by_path(path, fs->inodes_list)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int tmp_opendir(const char *path, struct fuse_file_info *fi)
{
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node) return -ENOENT;
    if (!is_dir(node)) return -ENOTDIR; 
    fi->fh = (uint64_t)node;
    return 0; 
}


int tmp_rmdir(const char *path)
{
    Filesystem* fs = fuse_get_context()->private_data;
    if (strcmp(path, "/") == 0) {
        errno = EACCES; 
        return -1;
    }

    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node || !is_dir(node)) {
        errno = ENOENT;
        return -1;
    }

    if (!remove_node_by_path(path, fs->inodes_list)) {
        errno = EIO;
        return -1;
    }

    return 0;
}
int tmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node) return -ENOENT;
    if (!is_dir(node)) return -ENOTDIR;

    Directory* node_dir = node->data;

    for (int i = 0; i < node_dir->num_entries; i++) {
        filler(buf, node_dir->entries[i].name, NULL, 0);
    }
    return 0;
}

int tmp_releasedir(const char *path, struct fuse_file_info *fi)
{
    fi->fh = 0;
    return 0;
}

int tmp_rename(const char *path, const char *newpath) {
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node) {
        errno = ENOENT;
        return -1;
    }

    if (strchr(newpath, '.') || strchr(path, '.')) {
        errno = EINVAL;
        return -1;
    }

    Inode* possible_node = get_inode_by_path(newpath, fs->inodes_list);
    if (possible_node != NULL) {
        errno = EEXIST;
        return -1;
    }
    if (!move_node(path, newpath, fs->inodes_list)) {
        errno = EIO;
        return -1;
    }

    return 0; // Успех
}

int tmp_open(const char *path, struct fuse_file_info *fi) {
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node) {
        errno = ENOENT; 
        return -1;
    }
    if (is_dir(node)) {
        errno = EISDIR;
        return -1;
    }
    node->nopen++;
    fi->fh = (uint64_t)node;
    return 0;
}


int tmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    Inode* node = (Inode*)fi->fh;
    if (!node->nopen) {
        errno = EBADF;
        return -1;
    }

    if (is_dir(node)) {
        errno = EISDIR;
        return -1;
    }

    if (offset + size > node->st->st_size) {
        size = node->st->st_size - offset;
        if (size <= 0) {
            return 0;
        }
    }
    memcpy(buf, (char*)node->data + offset, size);
    return (int)size;
}   


int tmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    Inode* node = (Inode*)fi->fh;
    if (!node->nopen) {
        errno = EBADF;
        return -1;
    }
    if (is_dir(node)) {
        errno = EISDIR;
        return -1;
    }
    if (offset + size > node->st->st_size) {
        char* data = realloc(node->data, offset + size);
        if (!data) {
            errno = ENOMEM;
            return -1;
        }
        node->data = data;
        node->st->st_size = (off_t)(size + offset);
    }
    memcpy((char*)node->data + offset, buf, size);
    return (int)size;
}


int tmp_truncate(const char* path, off_t offset) {
    Filesystem* fs = fuse_get_context()->private_data;
    Inode* node = get_inode_by_path(path, fs->inodes_list);
    if (!node->nopen) {
        errno = EBADF;
        return -1;
    }
    if (is_dir(node)) {
        errno = EISDIR;
        return -1;
    }
    if (offset == 0) {
        if (node->data) {
            free(node->data);
            node->data = NULL;
        }
    } else {
        char* data = realloc(node->data, offset);
        if (!data) {
            errno = ENOMEM;
            return -1;
        }
        node->data = data;
    }
    if (offset > node->st->st_size) {
        memset(node->data + node->st->st_size, 0, offset - node->st->st_size);
    }
    node->st->st_size = (off_t)offset;
    return 0;
}


int tmp_release(const char *path, struct fuse_file_info *fi) {
    Filesystem* fs = fuse_get_context()->private_data; 
    Inode* node = (Inode*)fi->fh;
    if (!node->nopen) {
        errno = EBADF;
        return -1;
    }
    if (--node->nopen == 0 && node->st->st_nlink == 0) {
        if (!remove_node_by_path(path, fs->inodes_list)) {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

void* tmp_init(struct fuse_conn_info *conn) {
    Filesystem* fs = init_filesystem();
    return fs;
}

void tmp_destroy(void *userdata) {
    Filesystem* fs = fuse_get_context()->private_data;
    // TODO нужно рекурсивно пройти по fs;
    free(fs);
}


struct fuse_operations operations = {
    .getattr = tmp_getattr,
    .mknod = tmp_mknod,
    .mkdir = tmp_mkdir,
    .unlink = tmp_unlink,
    .rmdir = tmp_rmdir,
    .rename = tmp_rename,
    .link = tmp_link,
    .open = tmp_open,
    .read = tmp_read,
    .write = tmp_write,
    .release = tmp_release,
    .truncate = tmp_truncate,
    .opendir = tmp_opendir,
    .readdir = tmp_readdir,
    .releasedir = tmp_releasedir,
    .init = tmp_init,
    .destroy = tmp_destroy
};


int main(int argc, char *argv[])
{
    int fuse_stat = fuse_main(argc, argv, &operations, NULL);
    return fuse_stat;
}
