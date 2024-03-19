#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>



#define MAX_FILES 100
#define MAX_INODES 1000
#define MAX_FILE_NAME 255
#define MAX_PATH 10200


#define is_dir(node) S_ISDIR((node)->st->st_mode)
#define is_file(node) S_ISREG((node)->st->st_mode)
// Inode ------------------------------------------------------------------
typedef struct Inode{
    int node_number;
    struct stat *st;
    void *data;
    struct Inode *parent_node;
    char nopen;
} Inode;

Inode* init_inode(int node_number, struct stat *st, void *data, Inode *parent_node);
void destroy_inode(Inode *node);

// InodeNumbersTracker -----------------------------------------------------
// Manages node numbers
typedef struct InodesNumbersTracker{
    int inodes[MAX_INODES];
    int max_inodes;
} InodesNumbersTracker;

InodesNumbersTracker* init_inodes_numbers_tracker(int maxInodes);
int allocate_inode_number(InodesNumbersTracker *tracker);
void free_inode_number(InodesNumbersTracker *tracker, int node_number);
void destroy_inode_tracker(InodesNumbersTracker *tracker);
bool isInodeNumberFree(int number, InodesNumbersTracker* tracker);

// InodeContainer ----------------------------------------------------------
typedef struct InodeContainer{
    Inode *inode_table[MAX_INODES+1];
} InodeContainer;

InodeContainer* init_inode_container(int maxInodes);
Inode *get_inode_from_container(InodeContainer *container, int node_number);

bool add_inode_to_container(InodeContainer *container, int node_number, Inode *node);
bool remove_inode_from_container(InodeContainer *container, int node_number);
bool is_valid_number(int node_number);

// Directory ---------------------------------------------------------------
typedef struct DirectoryEntry{
    char name[MAX_FILE_NAME];
    int node_number;
} DirectoryEntry;

typedef struct Directory{
    DirectoryEntry entries[MAX_FILES];
    int num_entries;
} Directory;

bool add_entry(Directory *dir, const char *name, int node_number);
bool remove_entry(Directory *dir, const char *name);
char check_entry(Directory* dir, const char *name);

// Filesystem --------------------------------------------------------------
typedef struct Filesystem{
    Inode *root;
    InodeContainer *inodes_list;
    InodesNumbersTracker *inodes_numbers_tracker;
} Filesystem;

Filesystem* init_filesystem();
Inode* get_inode_by_path(const char* path, InodeContainer* inodes_container);
char* get_last_name(const char* path);
bool add_node_by_path(const char * path, Inode* node, InodeContainer* inodes_container);
bool remove_node_by_path(const char* path, InodeContainer* inodes_container);
bool is_dir_empty(Inode* node);
bool move_node(const char* path, const char* new_path, InodeContainer* container);
int add_node_to_directory(Inode* dir_node, Inode* node, const char* name);
Inode* get_parent_directory(const char* path, InodeContainer* inodes_container);
#endif /* FILE_SYSTEM_H */