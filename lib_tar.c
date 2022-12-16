#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include "lib_tar.h"

bool is_zeros(const void *buf, size_t size) {
    for (int i = 0; i < size; i++) {
        if (*((char *) buf) != 0) {
            return false;
        }
    }
    return true;
}

bool check_checksum(tar_file_t *tar) {
    int correct_checksum = TAR_INT(tar->header.chksum);

    // store the raw bytes from the header
    char header_buf[sizeof(tar_header_t)];
    memcpy(&header_buf, &tar->header, sizeof(tar_header_t));

    // set the header bytes to ASCII spaces, this way we can compute the checksum without the original checksum getting in the way
    memset((void *) &header_buf[148], ' ', 8);

    int checksum = 0;
    // get the sum of the unsigned byte values of the header
    for (int i = 0; i < sizeof(tar_header_t); ++i) {
        checksum += (unsigned char) header_buf[i];
    }

    return correct_checksum == checksum;
}

int check_eof(int tar_fd, tar_file_t *tar) {
    // check to see if the header was full of 0s, if that's the case, check that the next block of length 512 is also 0s, if that's also the case then we reached the end of the tarball
    if (is_zeros((void *) &tar->header, 512)) {
        if (read(tar_fd, (void *) &tar->header, sizeof(tar_header_t)) == -1) {
            perror("Failed to read data from file");
            exit(EXIT_FAILURE);
        }
        if (is_zeros((void *) &tar->header, 512)) {
            // two empty blocks, this is the end of the tarball
            return 1;
        }
    }
    return 0;
}

int get_header(int tar_fd, tar_file_t *tar) {
    // get the header from the file
    if (read(tar_fd, (void *) &tar->header, sizeof(tar_header_t)) == -1) {
        perror("Failed to read header from file");
        exit(EXIT_FAILURE);
    }

    if (check_eof(tar_fd, tar) == 1) {
        return -1;
    }

    return 1;
}

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd) {
    // get the offset at the start of the function, this way we can go back there once we're done here
    long start_offset = lseek(tar_fd, 0, SEEK_CUR);
    lseek(tar_fd, 0, SEEK_SET);

    int file_count = 0;

    for (file_count = 0;; ++file_count) {
        tar_file_t tar;

        if (get_header(tar_fd, &tar) < 0) {
            break; // reached EOF
        }

        int file_size = TAR_INT(tar.header.size);

        if (memcmp(TMAGIC, tar.header.magic, TMAGLEN) != 0) {
            lseek(tar_fd, start_offset, SEEK_SET);
            return -1; // return -1 if the archive contains a header with an invalid magic value
        }

        if (memcmp(TVERSION, tar.header.version, TVERSLEN) != 0) {
            lseek(tar_fd, start_offset, SEEK_SET);
            return -2; // return -2 if the archive contains a header with an invalid version value
        }

        if (!check_checksum(&tar)) {
            lseek(tar_fd, start_offset, SEEK_SET);
            return -3; // return -3 when the archive contains a header with an invalid checksum value
        }

        int padding = (512 - file_size % 512) % 512;

        lseek(tar_fd, file_size + padding, SEEK_CUR);
    }

    lseek(tar_fd, start_offset, SEEK_SET); // reset the fd to the offset we read at the start of the function
    return file_count;
}

int check_file_type(int tar_fd, char *path, char typeflag) {
    // get the offset at the start of the function, this way we can go back there after we're done here
    long start_offset = lseek(tar_fd, 0, SEEK_CUR);
    lseek(tar_fd, 0, SEEK_SET);

    tar_file_t tar;
    while (get_header(tar_fd, &tar) >= 0) {

        int file_size = TAR_INT(tar.header.size);
        lseek(tar_fd, file_size, SEEK_CUR);

        // compare the filename with the path
        if (strncmp(tar.header.name, path, strlen(path)) != 0) {
            int padding = (512 - file_size % 512) % 512;
            lseek(tar_fd, padding, SEEK_CUR);
            continue;
        }

        // checks the header typeflag with the typeflag passed in as arg, and if the argument typeflag is REGTYPE, then we also want to check if the file perhaps has the old regular type AREGTYPE
        if (tar.header.typeflag == typeflag || (typeflag == REGTYPE && tar.header.typeflag == AREGTYPE)) {
            lseek(tar_fd, start_offset, SEEK_SET);
            return 1; // success, we found the file, and it is the correct type
        } else {
            lseek(tar_fd, start_offset, SEEK_SET);
            return 0; // we found the file, but it is not the correct type
        }
    }
    lseek(tar_fd, start_offset, SEEK_SET);

    return 0;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path) {
    // get the offset at the start of the function, this way we can go back there after we're done here
    long start_offset = lseek(tar_fd, 0, SEEK_CUR);
    lseek(tar_fd, 0, SEEK_SET);

    tar_file_t tar;
    while (get_header(tar_fd, &tar) >= 0) {
        // Check if current file have the same name as path
        if (strncmp(tar.header.name, path, strlen(path)) == 0) {
            lseek(tar_fd, start_offset, SEEK_SET); // Back to start of file
            return 1;
        }

        // Go to the next file
        int file_size = TAR_INT(tar.header.size);
        int padding = (512 - file_size % 512) % 512;
        lseek(tar_fd, file_size + padding, SEEK_CUR);
    }
    lseek(tar_fd, start_offset, SEEK_SET);
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    return check_file_type(tar_fd, path, DIRTYPE);
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    return check_file_type(tar_fd, path, REGTYPE);
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    
    return check_file_type(tar_fd, path, SYMTYPE);
}


/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    long start_offset = lseek(tar_fd, 0, SEEK_CUR);
    lseek(tar_fd, 0, SEEK_SET);

    int current = 0;
    tar_file_t tar;
    while (current < *no_entries && get_header(tar_fd, &tar) >= 0) {

        if (
                // check the path with the name
                (strncmp(path, tar.header.name, strlen(path)) == 0)
                // check if it's a link
                && (tar.header.typeflag == SYMTYPE || tar.header.typeflag == LNKTYPE)
                // and also check that the linked item is a directory
                && (is_dir(tar_fd, tar.header.linkname) != 0)
                ) {

            printf("linked directory: %s\n", tar.header.linkname);

            size_t linked_name_len = strlen(tar.header.linkname) + 2;
            char linkedname[linked_name_len];
            snprintf(linkedname, linked_name_len, "%s/", tar.header.linkname);

            lseek(tar_fd, start_offset, SEEK_SET);
            return list(tar_fd, linkedname, entries, no_entries);
        }

        char *dnamedup = strdup(tar.header.name);
        // get the directory of this file
        char *dirname_file = dirname(dnamedup);

        // if path is "" we replace it with "." to allow us to compare this with dirname_file
        char *path_to_search = strdup(path);
        if (strcmp(path_to_search, "") == 0) {
            strcpy(path_to_search, ".");
        }

        if (
            // check that the base directory is the same
                (strncmp(dirname_file, path_to_search, strlen(path_to_search) - 1) == 0)
                // check that the filename is not just the directory that we are listing
                && (strncmp(path, tar.header.name, strlen(tar.header.name)) < 0)
                // check that the directory for the path is the same as the directory for the filename
                && (strncmp(path_to_search, dirname_file, strlen(dirname_file)) == 0)
                ) {

            strncpy(entries[current++], tar.header.name, strlen(tar.header.name));
        }

        free(dnamedup);
        free(path_to_search);

        int file_size = TAR_INT(tar.header.size);
        int padding = (512 - file_size % 512) % 512;
        lseek(tar_fd, file_size + padding, SEEK_CUR);
    }

    lseek(tar_fd, start_offset, SEEK_SET);
    *no_entries = current;
    return current;
}


/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    lseek(tar_fd, 0, SEEK_SET);

    while (true) {

        tar_file_t tar;
        if (get_header(tar_fd, &tar) < 0) {
            break;
        }
        int file_size = TAR_INT(tar.header.size);

        if (strncmp(path, tar.header.name, strlen(path)) == 0) {
            if (tar.header.typeflag != REGTYPE && tar.header.typeflag != AREGTYPE && tar.header.typeflag != LNKTYPE && tar.header.typeflag != SYMTYPE) {
                return -1;
            }

            // handle symbolic links
            if (tar.header.typeflag == SYMTYPE || tar.header.typeflag != LNKTYPE) {

                // WARNING: this is the only thing that works on iniginous because the tests assume that the link and the linked-to files are in the root directory
//                lseek(tar_fd, 0, SEEK_SET);
//                return read_file(tar_fd, tar.header.linkname, offset, dest, len);

                char *linkedname_dup = strdup(tar.header.linkname);
                char *dirname_linkedname = dirname(linkedname_dup);

                printf("linked file: %s -> %s\n", tar.header.name, tar.header.linkname);

                // if the linked file is in the same directory as the linked-to file, then we have to add the full directory in front of the linked file's name
                // WARNING: while this is correct, it doesn't work on inginious for the reasons explained a few lines above
                if (strcmp(dirname_linkedname, ".") == 0) {
                    free(linkedname_dup);

                    // duplicate the current filename and get the directory
                    char *dnamedup = strdup(tar.header.name);
                    char *dirname_file = dirname(dnamedup);

                    size_t file_path_len = strlen(dirname_file) + 1 + strlen(tar.header.linkname) + 1;
                    char linked_file_path[file_path_len];
                    // create a new filepath with the filename
                    snprintf(linked_file_path, file_path_len, "%s/%s", dirname_file, tar.header.linkname);
                    free(dnamedup);

                    printf("resolving a linked file: %s -> %s\n", tar.header.name, linked_file_path);

                    // resolve the linked file
                    lseek(tar_fd, 0, SEEK_SET);
                    return read_file(tar_fd, linked_file_path, offset, dest, len);
                } else {
                    free(linkedname_dup);
                    lseek(tar_fd, 0, SEEK_SET);
                    return read_file(tar_fd, tar.header.linkname, offset, dest, len);
                }

            }

            if (offset > file_size) {
                return -2;
            }

            lseek(tar_fd, offset, SEEK_CUR);

            printf("filesize: %d, offset: %zu, filesize-offset = %zu\n", file_size, offset, file_size - offset);

            if (*len > file_size - offset) {
                *len = file_size - offset;
            }

            int res = read(tar_fd, (void *) dest, *len);
            if (res == -1) {
                perror("Failed to read from file");
                exit(EXIT_FAILURE);
            }

            *len = res;

            return (ssize_t) (file_size - res - offset);
        }

        int padding = (512 - file_size % 512) % 512;
        lseek(tar_fd, file_size + padding, SEEK_CUR); // we have to add file_size to the padding since we didn't read the data for this file
    }

    return -1;
}
