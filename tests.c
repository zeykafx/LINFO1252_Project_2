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
//
    /*            is ___ test:            */
    int is_directory = is_symlink(fd, "dir/file1_symlink.txt");
    printf("is_symlink returned %d\n", is_directory);


    /*            exists test :            */
    int file_exists = exists(fd, "dir/file1_symlink.txt");
    printf("exists returned : %d\n", file_exists);

//    /*            list test:            */
//    size_t no_entries = 6;
//    char **entries = malloc(no_entries * sizeof(char *));
//    for (int i = 0; i < no_entries; ++i) {
//        entries[i] = malloc(1000);
//    }
//    int list_res = list(fd, "dir/", entries, &no_entries);
//    printf("list returned : %d\n", list_res);
//    for (int i = 0; i < no_entries; ++i) {
//        printf("entry %d: %s\t", i, entries[i]);
//        free(entries[i]);
//    }
//    printf("\n");
//    free(entries);


    /*            read_file test:            */
    size_t size = 1000;
    uint8_t *buf = malloc(size);
    int file_read = read_file(fd, "dir/file1_symlink.txt", 0, buf, &size);
    printf("read_file returned : %d\n", file_read);
    for (int i = 0; i < size; i++)
        printf("%c", buf[i]);
    free(buf);


    return 0;
}