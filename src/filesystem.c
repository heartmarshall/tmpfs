#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>


#include "filesystem.h"
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_FILES 100
#define MAX_INODES 1000
#define MAX_FILE_NAME 255

// Inode ------------------------------------------------------------
Inode* init_inode(int node_number, struct stat *st, void *data, Inode *parent_node) {
    Inode *node = (Inode *)malloc(sizeof(Inode));
    if (node == NULL) {
        perror("Ошибка при выделении памяти для иноды");
        exit(EXIT_FAILURE);
    }

    node->node_number = node_number;
    node->st = st;
    node->parent_node = parent_node;
    node->data = data;
    node->nopen = 0;

    return node;
}

void destroy_inode(Inode *node) {
    free(node->st);
    if (node->data) free(node->data);
    free(node);
}

// InodesNumbersTracker ------------------------------------------------------------
InodesNumbersTracker* init_inodes_numbers_tracker(int max_inodes) {
    InodesNumbersTracker* inode_tracker = malloc(sizeof(InodesNumbersTracker));
    inode_tracker->max_inodes = max_inodes;
    for (int i = 0; i < max_inodes; ++i) {
        inode_tracker->inodes[i] = 0;
    }
    return inode_tracker;
}

int allocate_inode_number(InodesNumbersTracker *tracker) {
    for (int i = 0; i < tracker->max_inodes; ++i) {
        if (tracker->inodes[i] == 0) {
            tracker->inodes[i] = 1;
            return i + 1; // Нумерация инодов начинается с 1, поэтому добавляем 1 к индексу
        }
    }
    fprintf(stderr, "Все номера для инод заняты.\n");
    return -1;
}

void free_inode_number(InodesNumbersTracker *tracker, int node_number) {
    if (node_number <= 0 || node_number > tracker->max_inodes) {
        fprintf(stderr, "Некорректный номер инода.\n");
        return;
    }
    tracker->inodes[node_number - 1] = 0; // Освобождаем инод
}

void destroy_inode_tracker(InodesNumbersTracker *tracker) {
    free(tracker->inodes);
}

bool isInodeNumberFree(int number, InodesNumbersTracker* tracker){
    if (tracker->inodes[number-1] == 0){
        return true;
    }
    return false;
}

// InodeContainer -----------------------------------------------------------------------
bool is_valid_number(int node_number) {
    if (node_number < 0 || node_number >= MAX_INODES) {
        return false;
    }
    return true;
}

InodeContainer* init_inode_container(int max_inodes) {
    InodeContainer* container = malloc(sizeof(InodeContainer));
    for (int i = 0; i < MAX_INODES; i++) {
        container->inode_table[i] = NULL;
    }
    return container;
}

bool add_inode_to_container(InodeContainer *container, int node_number, Inode *inode) {
    if (!is_valid_number(node_number)){ return false;}
    container->inode_table[node_number] = inode;
    return true;
}

Inode *get_inode_from_container(InodeContainer *container, int node_number) {
    if (!is_valid_number(node_number)){ return NULL;}
    return container->inode_table[node_number];
}

bool remove_inode_from_container(InodeContainer *container, int node_number) {
    if (!is_valid_number(node_number)){ return false;}
    container->inode_table[node_number] = NULL;
    return true;
}

// Directory ------------------------------------------------------------------------------
// Структура для хранения пары <name>:<номер inode>
bool add_entry(Directory *dir, const char *name, int node_number) {
    if (dir->num_entries >= MAX_FILES) {
        fprintf(stderr, "Ошибка: Каталог полон, невозможно добавить больше записей.\n");
        return false;
    }
    
    if (strlen(name) >= MAX_FILE_NAME) {
        fprintf(stderr, "Ошибка: Слишком длинное имя файла.\n");
        return false;
    }

    strcpy(dir->entries[dir->num_entries].name, name);
    dir->entries[dir->num_entries].node_number = node_number;
    dir->num_entries++;
    return true;
}

// Функция для удаления записи из каталога
bool remove_entry(Directory *dir, const char *name) {
    int i;
    for (i = 0; i < dir->num_entries; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            // Найдена запись, сдвигаем все последующие записи на один влево
            for (int j = i; j < dir->num_entries - 1; j++) {
                strcpy(dir->entries[j].name, dir->entries[j + 1].name);
                dir->entries[j].node_number = dir->entries[j + 1].node_number;
            }
            dir->num_entries--;
            return true;
        }
    }
    fprintf(stderr, "Ошибка: Файл с именем \"%s\" не найден в каталоге.\n", name);
    return false;
}

// Проверяет есть ли такая запись в директории
char check_entry(Directory* dir, const char *name){
    int i;
    for (i = 0; i < dir->num_entries; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Filesystem ------------------------------------------------------------------------------

Filesystem* init_filesystem(){
    InodesNumbersTracker* inodes_numbers_tracker = init_inodes_numbers_tracker(MAX_INODES);
    InodeContainer* inodes_container = init_inode_container(MAX_INODES);
    Filesystem* fs = malloc(sizeof(Filesystem));
    if (fs == NULL) {
        fprintf(stderr, "Ошибка выделения памяти для файловой системы.\n");
        exit(EXIT_FAILURE);
    }

    // Задаём данные дирректори для root 
    Directory* root_directory = malloc(sizeof(Directory));
    if (root_directory == NULL) {
        fprintf(stderr, "Ошибка выделения памяти для данных корневой директории.\n");
        exit(EXIT_FAILURE);
    }
    // Добавляем в дирректорию root . и ..
    add_entry(root_directory, ".", 1);
    add_entry(root_directory, "..", 1);

    struct stat* root_stat = malloc(sizeof(struct stat));
    root_stat->st_nlink = 1;   
    root_stat->st_mode = S_IRWXO | S_IRWXG | S_IRWXU | __S_IFDIR;
    
    // Создаём иноду для root
    Inode* root_inode = init_inode(1, root_stat, root_directory, NULL);
    root_inode->parent_node = root_inode;
    root_inode->data = root_directory;
    root_inode->st->st_nlink = 2;
    add_inode_to_container(inodes_container, 1, root_inode);

    // Назначаем значения полей структуры Filesystem 
    fs->inodes_list = inodes_container;
    fs->root = root_inode;
    fs->inodes_numbers_tracker = inodes_numbers_tracker;
    return fs;
}

// Вот тут начинаются "высокоуровневые" операции

// /dir/dir2/file.txt -> [dir1, dir2, file.txt]
const char *get_next_path_component(const char **path) {
    const char *next = strchr(*path, '/');
    if (next) {
        const char *current = *path;
        *path = next + 1;
        return current;
    } else if (**path != '\0') {
        const char *current = *path;
        *path += strlen(*path);
        return current;
    }
    return NULL;
}

// /dir1/dir2/file.txt -> file.txt
char* get_last_name(const char* path) {
    const char* last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        return (char*)(last_slash + 1);
    }
    return (char*)path;
}

// Finds the Inode corresponding to the given path starting from the root
// Works only with absolute paths
Inode* get_inode_by_path(const char* path, InodeContainer* inodes_container) {
    if (path == NULL || inodes_container == NULL || *path != '/')
        return NULL;
    

    Inode *current_inode = get_inode_from_container(inodes_container, 1);
    path++;
    if (!*path){
        printf("Был запрошен ROOT\n");
        return current_inode;
    } 
    
    
    const char* component;
    while ((component = get_next_path_component(&path)) != NULL) {
        if (*component == '/') continue;
        int length = (int)(path - component);
        if (component[length - 1] == '/') {
            length--;
        }
        char dirname[length + 1];
        strncpy(dirname, component, length);
        dirname[length] = '\0'; //
        if (!is_dir(current_inode)){
            return NULL;
        }
        Directory* directory = (Directory*)current_inode->data;
        // Поиск имени файла в текущем каталоге
        int i;
        for (i = 0; i < directory->num_entries; ++i) {
            if (strcmp(directory->entries[i].name, dirname) == 0) {
                int node_number = directory->entries[i].node_number;
                current_inode = get_inode_from_container(inodes_container, node_number);
                break;
            }
        }
        
        // Если директория не найдена, возвращаем NULL
        if (i == directory->num_entries) {
            return NULL;
        }
    }
    return current_inode;
}

// /dir1/dir2/file.txt -> /dir/dir2
char* get_prefix(const char* path) {
    if (!path || *path == '\0')
        return NULL;
    
    int length = strlen(path);
    if (length == 1 && *path == '/')
        return strdup(path);
    
    const char* last_slash = strrchr(path, '/'); 
    if (!last_slash)
        return NULL;
    
    int prefix_length = last_slash - path;
    char* prefix = (char*)malloc(prefix_length + 1);
    if (!prefix)
        return NULL;
    
    strncpy(prefix, path, prefix_length);
    prefix[prefix_length] = '\0';
    
    return prefix;
}

Inode* get_parent_directory(const char* path, InodeContainer* inodes_container){
    char* dir_path = get_prefix(path);
    Inode* dir_node = get_inode_by_path(dir_path, inodes_container);
    if (!dir_node) return NULL;
    else if (!is_dir(dir_node)) {
        errno = ENOTDIR;
        return NULL;
    }
    free(dir_path);
    return dir_node;
}

int add_node_to_directory(Inode* dir_node, Inode* node, const char* name){
    if (!is_dir(dir_node)) return -1;
    add_entry(dir_node->data, name, node->node_number);
    node->st->st_nlink++;
    if (!node->parent_node) node->parent_node = dir_node;
    return 0;
}


bool add_node_by_path(const char * path, Inode* node, InodeContainer* inodes_container){
    char* node_name = get_last_name(path);
    Inode* parent_inode = get_parent_directory(path, inodes_container);
    if (parent_inode == NULL) {
        printf("Не найдена дирректория размещения");
        return false;
    }
    if (!is_dir(parent_inode)){
        printf("Родительская дирректория - не дирректория");
        return false;
    }
    Directory* parent_dir =  parent_inode->data;
    char name_already_taken = check_entry(parent_dir, node_name);
    if(name_already_taken) {
        errno = EEXIST;
        printf("Имя занято");
        return false;
    }

    add_entry(parent_dir, node_name, node->node_number);
    node->st->st_nlink++;
    return true;
}

// Удаляет запись о ноде в указанной дирректории. Если на ноду больше нет ссылок, то она удаляется.
bool remove_node_by_path(const char* path, InodeContainer* inodes_container){
    char* node_name = get_last_name(path);
    Inode* node = get_inode_by_path(path, inodes_container);
    if (node == NULL){
        return false;
    }
    Inode* parent_dir = get_parent_directory(path, inodes_container);

    if (is_dir(node)){
        if (is_dir_empty(node)){
            errno = ENOTEMPTY;
            return false;
        }
        destroy_inode(node);
    } else if (!node->nopen && --node->st->st_nlink == 0){
        destroy_inode(node);
    }

    if (!remove_entry(parent_dir->data, node_name)) {
        return false;
    }
    return true;
}

bool is_dir_empty(Inode* node){
    if (!is_dir(node)){
        errno = ENOTDIR;
        return false;
    }
    Directory* directory = node->data;
    if (directory->num_entries == 2){
        return true;
    }
    return false;
}


bool move_node(const char* path, const char* new_path, InodeContainer* container) {
    const char *old_file_name, *new_file_name;
    Inode* node = get_inode_by_path(path, container);
    Inode* source_dir_node = get_parent_directory(path, container);
    Inode* dist_dir_node = get_parent_directory(new_path, container);
    old_file_name = get_last_name(path);
    new_file_name = get_last_name(new_path);

    if (!source_dir_node || !dist_dir_node) return NULL;
    if (!is_dir(source_dir_node) || !is_dir(dist_dir_node)){
        errno = ENOTDIR;
        return false;
    }
    
    if (!remove_entry(source_dir_node->data, old_file_name)){
        errno = ENOENT;
        return false;
    }

    if (!add_entry(dist_dir_node->data, new_file_name, node->node_number)){
        errno = EOVERFLOW;
        return false;
    }
    

    return node;
}

