#include "filesystem.h"

void test_FindInodeByName() {
    Filesystem* fs = init_filesystem();
    InodeContainer* inodesContainer = fs->inodes_list;

    Inode* file1Inode = init_inode(11, NULL, NULL, NULL);
    Inode* file2Inode = init_inode(22, NULL, NULL, NULL);
    Directory* root_dir = fs->root->data;
    add_entry(root_dir, "subdir", 2);
    add_entry(root_dir, "file2", 22);

    Directory* sub_directory = malloc(sizeof(Directory));
    struct stat* dir_stat = malloc(sizeof(struct stat));
    dir_stat->st_mode = S_IRWXO | S_IRWXG | S_IRWXU | __S_IFDIR;
    Inode* subdirInode = init_inode(2, dir_stat, sub_directory, NULL);
    add_entry(sub_directory, "file1", 11);


    struct stat* sss = malloc(sizeof(struct stat));
    file1Inode->st = sss;

    add_inode_to_container(inodesContainer, 2, subdirInode);
    add_inode_to_container(inodesContainer, 11, file1Inode);
    add_inode_to_container(inodesContainer, 22, file2Inode);

    Inode* file3Inode = init_inode(33, NULL, NULL, NULL);
    struct stat* stat = malloc(sizeof(struct stat));
    file3Inode->st = stat;

    add_inode_to_container(inodesContainer, 33, file3Inode);
    printf("SSS %d\n", inodesContainer->inode_table[33]->node_number);
    if (!add_node_by_path("/subdir/file3", file3Inode, inodesContainer)){
        printf("Sosat...");
        return;
    }
    

    Inode* foundInode = get_inode_by_path("/subdir/file1", fs->inodes_list);
    if (foundInode != file1Inode) {
        printf("Ошибка: Неверный результат для поиска\n");
    } else {
        printf("Тест для поиска инода file1Inode пройден успешно.\n");
        printf("%d\n", foundInode->node_number);
        printf("%d", fs->root->parent_node->node_number);
    }

    Inode* foundInode2 = get_inode_by_path("/file2", fs->inodes_list);
    if (foundInode2 != file2Inode) {
        printf("Ошибка: Неверный результат для поиска\n");
    } else {
        printf("Тест для поиска инода file2Inode пройден успешно.\n");
        printf("%d\n", foundInode->node_number);
        printf("%d", fs->root->parent_node->node_number);
    }

    Inode* foundInode3 = get_inode_by_path("/subdir/file3", fs->inodes_list);
    if (foundInode3 != file3Inode) {
        printf("Ошибка: Неверный результат для поиска\n");
    } else {
        printf("Тест для поиска инода file3Inode пройден успешно.\n");
        printf("%d\n", foundInode->node_number);
        printf("%d", fs->root->parent_node->node_number);
    }

    remove_node_by_path("/subdir/file1", fs->inodes_list);
    foundInode = get_inode_by_path("/subdir/file1", fs->inodes_list);
    if (foundInode != file1Inode) {
        printf("Ошибка: Неверный результат для поиска\n");
    } else {
        printf("Тест для поиска инода file1Inode пройден успешно.\n");
        printf("%d\n", foundInode->node_number);
        printf("%d", fs->root->parent_node->node_number);
    }
    remove_node_by_path("/subdir/file3", fs->inodes_list);

    printf("DDF %d\n", ((Directory*)subdirInode->data)->num_entries);
    if (is_dir_empty(subdirInode)){printf("AAAA0");}
}

int main() {
    // const char* s = get_last_name("/123");
    // printf("%s\n", s);
    test_FindInodeByName();
    return 0;
}
