////////////////////////////////////////////////////////////////////////
// COMP1521 26T3 --- Assignment 2: `forest', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/26T3/assignments/ass2/index.html>
//
// Written by Ziqi Xie (z5634028) on 1/11/2026.
//
// 2024-03-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "forest.h"


// ADD ANY extra #defines HERE

#define MAGIC_NUMBER            0x63    // first byte of every leaf ('c')
#define MODE_LENGTH             10      // e.g. "-rwxr-xr-x"
#define PATHNAME_LENGTH_BYTES   2       // size of the pathname length field
#define CONTENT_LENGTH_BYTES    6       // size of the content length field
#define BITS_PER_BYTE           8

// The pathname length is stored in 2 bytes, so it can be at most 65535.
#define MAX_PATHNAME_LENGTH     65535


// ADD YOUR FUNCTION PROTOTYPES (AND STRUCTS IF ANY) HERE

// The fields at the start of a leaf, before its contents.
struct leaf {
    int format;                             // '6', '7' or '8'
    char mode[MODE_LENGTH + 1];             // e.g. "-rw-r--r--"
    char pathname[MAX_PATHNAME_LENGTH + 1];
    unsigned long content_length;           // size of the original file
};

FILE *open_file(char *pathname, char *mode);
int get_byte(FILE *tree);
int read_byte(FILE *tree, uint8_t *hash);
unsigned long read_number(FILE *tree, int n_bytes, uint8_t *hash);
int read_leaf_header(FILE *tree, struct leaf *leaf, uint8_t *hash);
unsigned long stored_length(struct leaf *leaf);
void skip_contents(FILE *tree, struct leaf *leaf, uint8_t *hash);
void copy_contents(FILE *tree, struct leaf *leaf, uint8_t *hash,
                   FILE *output);
void check_hash(FILE *tree, uint8_t hash);
void extract_file(FILE *tree, struct leaf *leaf, uint8_t *hash);
mode_t mode_to_permissions(char *mode);
void put_byte(FILE *tree, int byte);
void write_byte(FILE *tree, int byte, uint8_t *hash);
void write_number(FILE *tree, unsigned long number, int n_bytes,
                  uint8_t *hash);
void write_string(FILE *tree, char *string, int length, uint8_t *hash);
void permissions_to_mode(mode_t st_mode, char *mode);
void write_file_contents(FILE *tree, char *pathname, uint8_t *hash);
void add_file(FILE *tree, char *pathname, int format);


// print the files & directories stored in tree_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes
// are also printed (subset 0)

void list_tree(char *tree_pathname, int long_listing) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf leaf;
    uint8_t hash;
    while (read_leaf_header(tree, &leaf, &hash)) {
        skip_contents(tree, &leaf, &hash);
        check_hash(tree, hash);

        if (long_listing) {
            printf("%s  %c  %5lu  %s\n", leaf.mode, leaf.format,
                   leaf.content_length, leaf.pathname);
        } else {
            printf("%s\n", leaf.pathname);
        }
    }

    fclose(tree);
}


// check the files & directories stored in tree_pathname (subset 1)
//
// prints the files & directories stored in tree_pathname with a message
// either, indicating the hash byte is correct, or indicating the hash byte
// is incorrect, what the incorrect value is and the correct value would be

void check_tree(char *tree_pathname) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf leaf;
    uint8_t hash;
    while (read_leaf_header(tree, &leaf, &hash)) {
        skip_contents(tree, &leaf, &hash);

        int stored_hash = get_byte(tree);
        if (hash == stored_hash) {
            printf("%s - correct hash\n", leaf.pathname);
        } else {
            printf("%s - incorrect hash 0x%02x should be 0x%02x\n",
                   leaf.pathname, hash, stored_hash);
        }
    }

    fclose(tree);
}


// extract the files/directories stored in tree_pathname (subset 1 & 3)

void extract_tree(char *tree_pathname) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf leaf;
    uint8_t hash;
    while (read_leaf_header(tree, &leaf, &hash)) {
        extract_file(tree, &leaf, &hash);
        check_hash(tree, hash);
    }

    fclose(tree);
}


// create tree_pathname containing the files or directories specified in
// pathnames (subset 2 & 3)
//
// if append is zero tree_pathname should be over-written if it exists
// if append is non-zero trees should be instead appended to tree_pathname
//                       if it exists
//
// format specifies the tree format to use, it must be one LEAF_FMT_6,
// LEAF_FMT_7 or LEAF_FMT_8

void create_tree(char *tree_pathname, int append, int format,
                   int n_pathnames, char *pathnames[n_pathnames]) {
    // "ab" adds to the end of an existing tree; "wb" starts a new one.
    FILE *tree;
    if (append) {
        tree = open_file(tree_pathname, "ab");
    } else {
        tree = open_file(tree_pathname, "wb");
    }

    for (int i = 0; i < n_pathnames; i++) {
        add_file(tree, pathnames[i], format);
    }

    if (fclose(tree) != 0) {
        perror(tree_pathname);
        exit(1);
    }
}


// ADD YOUR EXTRA FUNCTIONS HERE


// Open a file with fopen, printing an error and exiting if it fails.
FILE *open_file(char *pathname, char *mode) {
    FILE *stream = fopen(pathname, mode);
    if (stream == NULL) {
        perror(pathname);
        exit(1);
    }
    return stream;
}


// Read one byte from tree.
// Prints an error and exits if the tree ends in the middle of a leaf.
int get_byte(FILE *tree) {
    int byte = fgetc(tree);
    if (byte == EOF) {
        fprintf(stderr, "error: unexpected end of tree\n");
        exit(1);
    }
    return byte;
}


// Read one byte from tree, and add it to the leaf's running hash.
int read_byte(FILE *tree, uint8_t *hash) {
    int byte = get_byte(tree);
    *hash = tree_hash(*hash, byte);
    return byte;
}


// Read an n_bytes long little-endian number from tree.
// The first byte read is the least significant.
unsigned long read_number(FILE *tree, int n_bytes, uint8_t *hash) {
    unsigned long number = 0;
    for (int i = 0; i < n_bytes; i++) {
        unsigned long byte = read_byte(tree, hash);
        number = number | (byte << (i * BITS_PER_BYTE));
    }
    return number;
}


// Read the start of the next leaf (everything before its contents)
// into leaf, starting a new running hash in *hash.
//
// Returns 1 if a leaf was read, or 0 if there are no more leafs.
int read_leaf_header(FILE *tree, struct leaf *leaf, uint8_t *hash) {
    int magic = fgetc(tree);
    if (magic == EOF) {
        return 0;
    }
    if (magic != MAGIC_NUMBER) {
        fprintf(stderr, "error: incorrect first leaf byte: 0x%02x "
                "should be 0x%02x\n", magic, MAGIC_NUMBER);
        exit(1);
    }
    *hash = tree_hash(0, magic);

    leaf->format = read_byte(tree, hash);
    if (leaf->format != LEAF_FMT_6 && leaf->format != LEAF_FMT_7 &&
        leaf->format != LEAF_FMT_8) {
        fprintf(stderr, "error: invalid leaf format: 0x%02x\n",
                leaf->format);
        exit(1);
    }

    for (int i = 0; i < MODE_LENGTH; i++) {
        leaf->mode[i] = read_byte(tree, hash);
    }
    leaf->mode[MODE_LENGTH] = '\0';

    int pathname_length = read_number(tree, PATHNAME_LENGTH_BYTES, hash);
    for (int i = 0; i < pathname_length; i++) {
        leaf->pathname[i] = read_byte(tree, hash);
    }
    leaf->pathname[pathname_length] = '\0';

    leaf->content_length = read_number(tree, CONTENT_LENGTH_BYTES, hash);
    return 1;
}


// Return how many bytes a leaf's contents take up in the tree.
//
// In 6-bit and 7-bit format each byte of the original file is packed
// into 6 or 7 bits, so the contents are shorter (rounded up to a byte).
unsigned long stored_length(struct leaf *leaf) {
    unsigned long bits_per_byte = leaf->format - '0';
    unsigned long total_bits = leaf->content_length * bits_per_byte;
    return (total_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
}


// Read past a leaf's contents, adding them to the running hash.
void skip_contents(FILE *tree, struct leaf *leaf, uint8_t *hash) {
    unsigned long length = stored_length(leaf);
    for (unsigned long i = 0; i < length; i++) {
        read_byte(tree, hash);
    }
}


// Copy a leaf's contents to output, adding them to the running hash.
void copy_contents(FILE *tree, struct leaf *leaf, uint8_t *hash,
                   FILE *output) {
    unsigned long length = stored_length(leaf);
    for (unsigned long i = 0; i < length; i++) {
        int byte = read_byte(tree, hash);
        if (fputc(byte, output) == EOF) {
            perror(leaf->pathname);
            exit(1);
        }
    }
}


// Read a leaf's stored hash byte, and compare it with the hash we
// calculated from the rest of the leaf. Prints an error and exits if
// they are different.
void check_hash(FILE *tree, uint8_t hash) {
    int stored_hash = get_byte(tree);
    if (hash != stored_hash) {
        fprintf(stderr, "error: incorrect leaf hash 0x%02x should be 0x%02x\n",
                hash, stored_hash);
        exit(1);
    }
}


// Extract one file from tree: write its contents to leaf->pathname
// (overwriting it if it exists), then set its permissions.
void extract_file(FILE *tree, struct leaf *leaf, uint8_t *hash) {
    printf("Extracting: %s\n", leaf->pathname);

    FILE *output = open_file(leaf->pathname, "wb");
    copy_contents(tree, leaf, hash, output);
    if (fclose(output) != 0) {
        perror(leaf->pathname);
        exit(1);
    }

    if (chmod(leaf->pathname, mode_to_permissions(leaf->mode)) != 0) {
        perror(leaf->pathname);
        exit(1);
    }
}


// Convert an ls-style mode string, e.g. "-rwxr-xr-x", into permission
// bits, e.g. 0755. mode[0] is the file type, so permissions start at mode[1].
mode_t mode_to_permissions(char *mode) {
    mode_t permissions = 0;

    if (mode[1] == 'r') {
        permissions = permissions | S_IRUSR;
    }
    if (mode[2] == 'w') {
        permissions = permissions | S_IWUSR;
    }
    if (mode[3] == 'x') {
        permissions = permissions | S_IXUSR;
    }

    if (mode[4] == 'r') {
        permissions = permissions | S_IRGRP;
    }
    if (mode[5] == 'w') {
        permissions = permissions | S_IWGRP;
    }
    if (mode[6] == 'x') {
        permissions = permissions | S_IXGRP;
    }

    if (mode[7] == 'r') {
        permissions = permissions | S_IROTH;
    }
    if (mode[8] == 'w') {
        permissions = permissions | S_IWOTH;
    }
    if (mode[9] == 'x') {
        permissions = permissions | S_IXOTH;
    }

    return permissions;
}


// Write one byte to tree.
// Prints an error and exits if the write fails.
void put_byte(FILE *tree, int byte) {
    if (fputc(byte, tree) == EOF) {
        perror("fputc");
        exit(1);
    }
}


// Write one byte to tree, and add it to the leaf's running hash.
void write_byte(FILE *tree, int byte, uint8_t *hash) {
    put_byte(tree, byte);
    *hash = tree_hash(*hash, byte);
}


// Write number to tree as an n_bytes long little-endian number.
// The least significant byte is written first.
void write_number(FILE *tree, unsigned long number, int n_bytes,
                  uint8_t *hash) {
    for (int i = 0; i < n_bytes; i++) {
        int byte = (number >> (i * BITS_PER_BYTE)) & 0xff;
        write_byte(tree, byte, hash);
    }
}


// Write the first length characters of string to tree (without a '\0').
void write_string(FILE *tree, char *string, int length, uint8_t *hash) {
    for (int i = 0; i < length; i++) {
        write_byte(tree, string[i], hash);
    }
}


// Convert a file's st_mode (from stat) into an ls-style mode string,
// e.g. "-rwxr-xr-x". This is the reverse of mode_to_permissions.
void permissions_to_mode(mode_t st_mode, char *mode) {
    if (S_ISDIR(st_mode)) {
        mode[0] = 'd';
    } else {
        mode[0] = '-';
    }

    mode[1] = (st_mode & S_IRUSR) ? 'r' : '-';
    mode[2] = (st_mode & S_IWUSR) ? 'w' : '-';
    mode[3] = (st_mode & S_IXUSR) ? 'x' : '-';

    mode[4] = (st_mode & S_IRGRP) ? 'r' : '-';
    mode[5] = (st_mode & S_IWGRP) ? 'w' : '-';
    mode[6] = (st_mode & S_IXGRP) ? 'x' : '-';

    mode[7] = (st_mode & S_IROTH) ? 'r' : '-';
    mode[8] = (st_mode & S_IWOTH) ? 'w' : '-';
    mode[9] = (st_mode & S_IXOTH) ? 'x' : '-';

    mode[MODE_LENGTH] = '\0';
}


// Copy every byte of the file at pathname into tree, adding each byte
// to the running hash.
void write_file_contents(FILE *tree, char *pathname, uint8_t *hash) {
    FILE *input = open_file(pathname, "rb");

    int byte;
    while ((byte = fgetc(input)) != EOF) {
        write_byte(tree, byte, hash);
    }

    fclose(input);
}


// Add one leaf to tree for the file at pathname:
// header fields, then the file's contents, then the leaf hash.
void add_file(FILE *tree, char *pathname, int format) {
    struct stat s;
    if (stat(pathname, &s) != 0) {
        perror(pathname);
        exit(1);
    }

    printf("Adding: %s\n", pathname);

    char mode[MODE_LENGTH + 1];
    permissions_to_mode(s.st_mode, mode);
    int pathname_length = strlen(pathname);

    uint8_t hash = 0;
    write_byte(tree, MAGIC_NUMBER, &hash);
    write_byte(tree, format, &hash);
    write_string(tree, mode, MODE_LENGTH, &hash);
    write_number(tree, pathname_length, PATHNAME_LENGTH_BYTES, &hash);
    write_string(tree, pathname, pathname_length, &hash);
    write_number(tree, s.st_size, CONTENT_LENGTH_BYTES, &hash);
    write_file_contents(tree, pathname, &hash);

    // The hash byte itself is not part of the hash.
    put_byte(tree, hash);
}
