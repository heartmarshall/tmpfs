#include <stdio.h>
#include <string.h>

const char *getNextPathComponent(const char **path) {
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

int main() {
    const char *path = "/subdir/file1///";
    printf("Path: %s\n", path);
    // Извлечение и вывод только имени директории из пути
    path++;

    const char* component;
    while ((component = getNextPathComponent(&path)) != NULL) {
        if (*component == '/') continue;
        int length = (int)(path - component);
        if (component[length - 1] == '/') {
            length--;
        }
        char dirname[length + 1];
        strncpy(dirname, component, length);
        dirname[length] = '\0';
        printf("Component: %.*s\n", length, component);
    }

    return 0;
}