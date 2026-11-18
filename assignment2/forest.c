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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "forest.h"


// Constants for the leaf format

#define MAGIC_NUMBER            0x63    // first byte of every leaf ('c')
#define MODE_LENGTH             10      // e.g. "-rwxr-xr-x"
#define PATHNAME_LENGTH_BYTES   2       // size of the pathname length field
#define CONTENT_LENGTH_BYTES    6       // size of the content length field
#define BITS_PER_BYTE           8
#define BYTE_MASK               0xff    // keeps the lowest 8 bits
#define MAX_7_BIT_VALUE         0x7f    // largest byte 7-bit format can store

// The pathname length is stored in 2 bytes, so it can be at most 65535.
#define MAX_PATHNAME_LENGTH     65535


// Structs and function prototypes

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
void write_file_contents(FILE *tree, char *pathname, int format,
                         uint8_t *hash);
void add_leaf(FILE *tree, char *pathname, int format);
int encode_byte(int byte, int format);
int decode_value(int value, int format);
void check_file_encodable(char *pathname, int format);
void set_permissions(char *pathname, char *mode);
void extract_directory(FILE *tree, struct leaf *leaf, uint8_t *hash);
void get_stat(char *pathname, struct stat *s);
int is_directory(char *pathname);
int bits_per_value(int format);
void add_parent_directories(FILE *tree, char *pathname, int format);
void add_recursively(FILE *tree, char *pathname, int format);


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
        if (leaf.mode[0] == 'd') {
            extract_directory(tree, &leaf, &hash);
        } else {
            extract_file(tree, &leaf, &hash);
        }
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
        add_parent_directories(tree, pathnames[i], format);
        add_recursively(tree, pathnames[i], format);
    }

    if (fclose(tree) != 0) {
        perror(tree_pathname);
        exit(1);
    }
}


// Helper functions


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
    unsigned long total_bits = leaf->content_length *
                               bits_per_value(leaf->format);
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
//
// The contents are a stream of packed n-bit values (n = 6, 7 or 8).
// Bytes from the tree are added to the right of buffer; whenever buffer
// holds at least n bits, the leftmost n bits are the next value.
// Any bits left over at the end are padding.
void copy_contents(FILE *tree, struct leaf *leaf, uint8_t *hash,
                   FILE *output) {
    int value_bits = bits_per_value(leaf->format);
    unsigned long length = stored_length(leaf);
    unsigned long n_values = 0;

    unsigned int buffer = 0;
    int n_bits = 0;
    for (unsigned long i = 0; i < length; i++) {
        buffer = (buffer << BITS_PER_BYTE) | read_byte(tree, hash);
        n_bits = n_bits + BITS_PER_BYTE;

        while (n_bits >= value_bits && n_values < leaf->content_length) {
            n_bits = n_bits - value_bits;
            int value = (buffer >> n_bits) & ((1 << value_bits) - 1);
            if (fputc(decode_value(value, leaf->format), output) == EOF) {
                perror(leaf->pathname);
                exit(1);
            }
            n_values++;
        }
        // Keep only the bits not used yet.
        buffer = buffer & ((1 << n_bits) - 1);
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

    set_permissions(leaf->pathname, leaf->mode);
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
        int byte = (number >> (i * BITS_PER_BYTE)) & BYTE_MASK;
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


// Copy the file at pathname into tree as packed n-bit values
// (n = 6, 7 or 8, from format), adding each byte written to the hash.
//
// Each value is added to the right of buffer; whenever buffer holds at
// least 8 bits, the leftmost 8 are written out. Any bits left at the end
// are written in one last byte, padded on the right with zeros.
void write_file_contents(FILE *tree, char *pathname, int format,
                         uint8_t *hash) {
    FILE *input = open_file(pathname, "rb");
    int value_bits = bits_per_value(format);

    unsigned int buffer = 0;
    int n_bits = 0;
    int byte;
    while ((byte = fgetc(input)) != EOF) {
        buffer = (buffer << value_bits) | encode_byte(byte, format);
        n_bits = n_bits + value_bits;

        while (n_bits >= BITS_PER_BYTE) {
            n_bits = n_bits - BITS_PER_BYTE;
            write_byte(tree, (buffer >> n_bits) & BYTE_MASK, hash);
        }
        // Keep only the bits not written yet.
        buffer = buffer & ((1 << n_bits) - 1);
    }

    if (n_bits > 0) {
        int padding_bits = BITS_PER_BYTE - n_bits;
        write_byte(tree, (buffer << padding_bits) & BYTE_MASK, hash);
    }

    fclose(input);
}


// Add one leaf to tree for the file or directory at pathname:
// header fields, then the file's contents, then the leaf hash.
// A directory's leaf has no contents.
void add_leaf(FILE *tree, char *pathname, int format) {
    struct stat s;
    get_stat(pathname, &s);

    int directory = S_ISDIR(s.st_mode);
    unsigned long content_length = 0;
    if (!directory) {
        check_file_encodable(pathname, format);
        content_length = s.st_size;
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
    write_number(tree, content_length, CONTENT_LENGTH_BYTES, &hash);
    if (!directory) {
        write_file_contents(tree, pathname, format, &hash);
    }

    // The hash byte itself is not part of the hash.
    put_byte(tree, hash);
}


// Convert a byte of a file into the value stored for it in format.
// Returns -1 if the byte can't be stored in that format.
int encode_byte(int byte, int format) {
    if (format == LEAF_FMT_6) {
        return tree_to_6_bit(byte);
    }
    if (format == LEAF_FMT_7 && byte > MAX_7_BIT_VALUE) {
        return -1;
    }
    return byte;
}


// Convert a value stored in format back into the original byte.
// This is the reverse of encode_byte.
int decode_value(int value, int format) {
    if (format == LEAF_FMT_6) {
        return tree_from_6_bit(value);
    }
    return value;
}


// Check every byte of the file at pathname can be stored in format.
// Prints an error and exits at the first byte that can't.
void check_file_encodable(char *pathname, int format) {
    FILE *input = open_file(pathname, "rb");

    int byte;
    while ((byte = fgetc(input)) != EOF) {
        if (encode_byte(byte, format) < 0) {
            fprintf(stderr, "error: byte 0x%02x can not be represented in "
                    "%c-bit format\n", byte, format);
            exit(1);
        }
    }

    fclose(input);
}


// Set the permissions of pathname to those in an ls-style mode string.
void set_permissions(char *pathname, char *mode) {
    if (chmod(pathname, mode_to_permissions(mode)) != 0) {
        perror(pathname);
        exit(1);
    }
}


// Extract one directory from tree: create it if it doesn't already
// exist, then set its permissions.
void extract_directory(FILE *tree, struct leaf *leaf, uint8_t *hash) {
    printf("Creating directory: %s\n", leaf->pathname);

    mode_t permissions = mode_to_permissions(leaf->mode);
    if (mkdir(leaf->pathname, permissions) != 0 && errno != EEXIST) {
        perror(leaf->pathname);
        exit(1);
    }
    set_permissions(leaf->pathname, leaf->mode);

    // A directory leaf has no contents, but read past them anyway so the
    // hash is calculated the same way as for a file.
    skip_contents(tree, leaf, hash);
}


// Get the metadata (type, permissions, size) of pathname with stat.
// Prints an error and exits if it fails.
void get_stat(char *pathname, struct stat *s) {
    if (stat(pathname, s) != 0) {
        perror(pathname);
        exit(1);
    }
}


// Return 1 if pathname is a directory, 0 otherwise.
int is_directory(char *pathname) {
    struct stat s;
    get_stat(pathname, &s);
    return S_ISDIR(s.st_mode);
}


// Return how many bits each byte of a file takes up in format:
// 6, 7 or 8 (the format bytes are the characters '6', '7' and '8').
int bits_per_value(int format) {
    return format - '0';
}


// Add a leaf for each directory in pathname before the last part,
// e.g. for "a/b/file.txt" add leafs for "a" and "a/b".
void add_parent_directories(FILE *tree, char *pathname, int format) {
    char parent[MAX_PATHNAME_LENGTH + 1];

    for (int i = 1; pathname[i] != '\0'; i++) {
        if (pathname[i] == '/') {
            strncpy(parent, pathname, i);
            parent[i] = '\0';
            add_leaf(tree, parent, format);
        }
    }
}


// Add a leaf for pathname. If pathname is a directory, also add
// everything inside it, in alphabetical order.
void add_recursively(FILE *tree, char *pathname, int format) {
    add_leaf(tree, pathname, format);
    if (!is_directory(pathname)) {
        return;
    }

    // scandir gives the directory's entries sorted by alphasort.
    struct dirent **entries;
    int n_entries = scandir(pathname, &entries, NULL, alphasort);
    if (n_entries < 0) {
        perror(pathname);
        exit(1);
    }

    for (int i = 0; i < n_entries; i++) {
        char *name = entries[i]->d_name;
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            char child[MAX_PATHNAME_LENGTH + 1];
            snprintf(child, sizeof child, "%s/%s", pathname, name);
            add_recursively(tree, child, format);
        }
        free(entries[i]);
    }
    free(entries);
}
