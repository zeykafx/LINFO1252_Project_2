#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lib_tar.h"

int is_zeros(const void *buf, size_t size) {
    char zero_buffer[size];
    memset(zero_buffer, 0, size);
    return memcmp(buf, zero_buffer, size);
}

int compute_checksum(tar_file_t *tar) {
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

    return memcmp(&correct_checksum, &checksum, sizeof(int));
}

int check_eof(int tar_fd, tar_file_t *tar) {
    // check to see if the header was full of 0s, if that's the case, check that the next block of length 512 is also 0s, if that's also the case then we reached the end of the tarball
    if (is_zeros((void *) &tar->header, 512) == 0) {
        if (read(tar_fd, (void *) &tar->header, sizeof(tar_header_t)) == -1) {
            perror("Failed to read data from file");
            exit(EXIT_FAILURE);
        }
        if (is_zeros((void *) &tar->header, 512) == 0) {
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
    int file_count = 0;

    for (file_count = 0;; ++file_count) {
        tar_file_t tar;

        if (get_header(tar_fd, &tar) < 0) {
            break; // reached EOF
        }

        int file_size = TAR_INT(tar.header.size);
        uint8_t data[file_size + 1];

        if (read(tar_fd, (void *) &data, file_size) == -1) {
            perror("Failed to read data from file");
            exit(EXIT_FAILURE);
        }

        tar.block = data;

        if (memcmp(TMAGIC, tar.header.magic, TMAGLEN) != 0) {
            return -1; // return -1 if the archive contains a header with an invalid magic value
        }

        if (memcmp(TVERSION, tar.header.version, TVERSLEN) != 0) {
            return -2; // return -2 if the archive contains a header with an invalid version value
        }

        if (compute_checksum(&tar) != 0) {
            return -3; // return -3 when the archive contains a header with an invalid checksum value
        }

        int padding = (512 - file_size % 512) % 512;

        lseek(tar_fd, padding, SEEK_CUR);
    }
    return 0;
}

int check_file_type(int tar_fd, char *path, char typeflag) {
    while (1) {
        tar_file_t tar;

        if (get_header(tar_fd, &tar) < 0) {
            break; // reached EOF
        }

        int file_size = TAR_INT(tar.header.size);
        uint8_t data[file_size + 1];

        if (read(tar_fd, (void *) &data, file_size) == -1) {
            perror("Failed to read data from file");
            exit(EXIT_FAILURE);
        }

        tar.block = data;

        // compare the filename with the path
        // TODO: maybe check if the name is in the path, this way if the user forgot the '/' it would still find the file
        if (strncmp(tar.header.name, path, strlen(path)) != 0) {
            int padding = (512 - file_size % 512) % 512;
            lseek(tar_fd, padding, SEEK_CUR);
            continue;
        }

        char aregtypeflag = AREGTYPE;
        // checks the header typeflag with the typeflag passed in as arg, and if the argument typeflag is REGTYPE, then we also want to check if the file perhaps has the old regular type AREGTYPE
        if ((memcmp(&tar.header.typeflag, &typeflag, sizeof(typeflag)) == 0) || (typeflag == REGTYPE ? memcmp(&tar.header.typeflag, &aregtypeflag, sizeof(aregtypeflag)) == 0 : 0)) {
            return 1; // success, we found the file, and it is the correct type
        } else {
            return -1; // we found the file, but it is not the correct type
        }
    }

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
    // TODO
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
    return check_file_type(tar_fd, path, LNKTYPE);
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
    // TODO
    return 0;
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
    return 0;
}