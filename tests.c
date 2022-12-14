#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len) {
    for (int i = 0; i < len;) {
        printf("%04x:  ", (int) i);

        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++) {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

//    /*            check archive test:            */
//    int ret = check_archive(fd);
//    printf("check_archive returned %d (positive number == success)\n", ret);
//    lseek(fd, 0, SEEK_SET);


//    /*            is ___ test:            */
//    int is_directory = is_dir(fd, "dir/file1_og.txt");
//    printf("is_symlink returned %d\n", is_directory);
//    lseek(fd, 0, SEEK_SET);

//    /*            exists test :            */
//    int file_exists = exists(fd, "di");
//    printf("exists returned : %d\n", file_exists);
//    lseek(fd, 0, SEEK_SET);


//    /*            list test:            */
//    size_t arr_size = 6;
//    size_t no_entries = 6;
//    char **entries = malloc(no_entries * sizeof(char *));
//    for (int i = 0; i < arr_size; ++i) {
//        entries[i] = calloc(512, sizeof(char));
//    }
//    int list_res = list(fd, "linked_dir2", entries, &no_entries);
//    printf("list returned : %d\n", list_res);
//    for (int i = 0; i < arr_size; ++i) {
//        printf("entry %d: %s\n", i, entries[i]);
//        free(entries[i]);
//    }
//    printf("\n");
//    free(entries);
//    lseek(fd, 0, SEEK_SET);


    /*            read_file test:            */
    size_t size = 841;
    uint8_t *buf = malloc(size);
    lseek(fd, 0, SEEK_SET);

    printf("len before: %zu\n", size);
    ssize_t file_read = read_file(fd, "dir/file1_symlink.txt", 800, buf, &size);
    for (int i = 0; i < size; i++)
        printf("%c", buf[i]);
    printf("\n");
    printf("read_file returned : %zd\n", file_read);
    printf("len after: %zu\n", size);


    free(buf);
    lseek(fd, 0, SEEK_SET);


    return 0;
}
