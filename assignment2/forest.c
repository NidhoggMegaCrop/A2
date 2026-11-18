////////////////////////////////////////////////////////////////////////
// COMP1521 26T3 --- Assignment 2: `forest', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/26T3/assignments/ass2/index.html>
//
// Written by YOUR-NAME-HERE (z5555555) on INSERT-DATE-HERE.
//
// 2024-03-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "forest.h"


// ADD ANY extra #defines HERE

#define BITS_PER_BYTE 8

// Room for a mode string such as "-rwxr-xr-x" (GALAXY_LENGTH_MODE chars)
// plus its terminating '\0'. GALAXY_LENGTH_MODE is not a constant
// expression in C, so it can't size an array inside a struct.
#define MODE_STRING_SIZE 11


// ADD YOUR FUNCTION PROTOTYPES (AND STRUCTS IF ANY) HERE

// The fields of a leaf which come before its contents.
struct leaf_header {
    uint8_t format;                          // LEAF_FMT_6, _7 or _8
    char mode[MODE_STRING_SIZE];             // e.g. "-rw-r--r--"
    char *pathname;                          // malloc'd, NUL-terminated
    uint64_t content_length;                 // length of the original file
};

static FILE *open_file(char *pathname, char *mode);
static void close_file(FILE *stream, char *pathname);
static uint8_t read_byte(FILE *stream, uint8_t *hash);
static void write_byte(FILE *stream, uint8_t byte, char *pathname);
static uint64_t read_little_endian(FILE *stream, int n_bytes, uint8_t *hash);
static int read_leaf_header(FILE *tree, struct leaf_header *header,
                            uint8_t *hash);
static uint64_t stored_content_length(uint8_t format, uint64_t content_length);
static void read_leaf_contents(FILE *tree, struct leaf_header *header,
                               uint8_t *hash, FILE *output, char *out_path);
static void verify_leaf_hash(FILE *tree, uint8_t computed_hash);
static void extract_file(FILE *tree, struct leaf_header *header,
                         uint8_t *hash);
static mode_t mode_string_to_permissions(char *mode);


// print the files & directories stored in tree_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes
// are also printed (subset 0)

void list_tree(char *tree_pathname, int long_listing) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf_header header;
    uint8_t hash;
    while (read_leaf_header(tree, &header, &hash)) {
        // Read past the leaf's contents (hashing them), and check its hash
        // byte before printing anything about the leaf.
        read_leaf_contents(tree, &header, &hash, NULL, NULL);
        verify_leaf_hash(tree, hash);

        if (long_listing) {
            printf("%s  %c  %5lu  %s\n", header.mode, header.format,
                   (unsigned long)header.content_length, header.pathname);
        } else {
            printf("%s\n", header.pathname);
        }
        free(header.pathname);
    }

    close_file(tree, tree_pathname);
}


// check the files & directories stored in tree_pathname (subset 1)
//
// prints the files & directories stored in tree_pathname with a message
// either, indicating the hash byte is correct, or indicating the hash byte
// is incorrect, what the incorrect value is and the correct value would be

void check_tree(char *tree_pathname) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf_header header;
    uint8_t computed_hash;
    while (read_leaf_header(tree, &header, &computed_hash)) {
        read_leaf_contents(tree, &header, &computed_hash, NULL, NULL);
        uint8_t stored_hash = read_byte(tree, NULL);

        if (computed_hash == stored_hash) {
            printf("%s - correct hash\n", header.pathname);
        } else {
            printf("%s - incorrect hash 0x%02x should be 0x%02x\n",
                   header.pathname, computed_hash, stored_hash);
        }
        free(header.pathname);
    }

    close_file(tree, tree_pathname);
}


// extract the files/directories stored in tree_pathname (subset 1 & 3)

void extract_tree(char *tree_pathname) {
    FILE *tree = open_file(tree_pathname, "rb");

    struct leaf_header header;
    uint8_t hash;
    while (read_leaf_header(tree, &header, &hash)) {
        extract_file(tree, &header, &hash);
        verify_leaf_hash(tree, hash);
        free(header.pathname);
    }

    close_file(tree, tree_pathname);
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

    // REPLACE THIS CODE PRINTFS WITH YOUR CODE

    printf("create_tree called to create tree: '%s'\n", tree_pathname);
    printf("format = %x\n", format);
    printf("append = %d\n", append);
    printf("These %d pathnames specified:\n", n_pathnames);
    for (int p = 0; p < n_pathnames; p++) {
        printf("%s\n", pathnames[p]);
    }
}


// ADD YOUR EXTRA FUNCTIONS HERE


// Open pathname with fopen, exiting with an error message on failure.
static FILE *open_file(char *pathname, char *mode) {
    FILE *stream = fopen(pathname, mode);
    if (stream == NULL) {
        perror(pathname);
        exit(1);
    }
    return stream;
}


// Close stream with fclose, exiting with an error message on failure.
// (For a file being written, fclose is where a failed write can show up.)
static void close_file(FILE *stream, char *pathname) {
    if (fclose(stream) == EOF) {
        perror(pathname);
        exit(1);
    }
}


// Read one byte from stream, exiting with an error if the stream ends
// early or a read error occurs.
//
// If hash is not NULL, the byte is also added to the running leaf hash
// it points to.
static uint8_t read_byte(FILE *stream, uint8_t *hash) {
    int byte = fgetc(stream);
    if (byte == EOF) {
        if (ferror(stream)) {
            perror("fgetc");
        } else {
            fprintf(stderr, "error: unexpected end of tree\n");
        }
        exit(1);
    }
    if (hash != NULL) {
        *hash = tree_hash(*hash, byte);
    }
    return (uint8_t)byte;
}


// Write one byte to stream, exiting with an error message on failure.
static void write_byte(FILE *stream, uint8_t byte, char *pathname) {
    if (fputc(byte, stream) == EOF) {
        perror(pathname);
        exit(1);
    }
}


// Read an unsigned little-endian integer n_bytes long from stream,
// adding each byte to the running hash.
static uint64_t read_little_endian(FILE *stream, int n_bytes, uint8_t *hash) {
    uint64_t value = 0;
    for (int i = 0; i < n_bytes; i++) {
        uint64_t byte = read_byte(stream, hash);
        value |= byte << (i * BITS_PER_BYTE);
    }
    return value;
}


// Read the fields of the next leaf in tree, up to (but not including)
// its contents, into header.
//
// Starts a new running hash in *hash, covering every byte read.
//
// Returns 1 if a leaf header was read, or 0 if the tree has no more leafs.
// The caller is responsible for freeing header->pathname.
static int read_leaf_header(FILE *tree, struct leaf_header *header,
                            uint8_t *hash) {
    int magic = fgetc(tree);
    if (magic == EOF) {
        if (ferror(tree)) {
            perror("fgetc");
            exit(1);
        }
        return 0;
    }
    if (magic != LEAF_MAGIC) {
        fprintf(stderr, "error: incorrect first leaf byte: 0x%02x "
                "should be 0x%02x\n", magic, LEAF_MAGIC);
        exit(1);
    }
    *hash = tree_hash(0, magic);

    header->format = read_byte(tree, hash);
    if (header->format != LEAF_FMT_6 && header->format != LEAF_FMT_7 &&
        header->format != LEAF_FMT_8) {
        fprintf(stderr, "error: invalid leaf format: 0x%02x\n",
                header->format);
        exit(1);
    }

    for (int i = 0; i < GALAXY_LENGTH_MODE; i++) {
        header->mode[i] = read_byte(tree, hash);
    }
    header->mode[GALAXY_LENGTH_MODE] = '\0';

    uint64_t pathname_length = read_little_endian(tree,
                                                  GALAXY_LENGTH_PATHNLEN,
                                                  hash);
    header->pathname = malloc(pathname_length + 1);
    if (header->pathname == NULL) {
        perror("malloc");
        exit(1);
    }
    for (uint64_t i = 0; i < pathname_length; i++) {
        header->pathname[i] = read_byte(tree, hash);
    }
    header->pathname[pathname_length] = '\0';

    header->content_length = read_little_endian(tree, GALAXY_LENGTH_CONTLEN,
                                                hash);
    return 1;
}


// Return the number of bytes a leaf's contents occupy in the tree, given
// the leaf's format and the length of the original (unencoded) file.
//
// n-bit formats pack each original byte into n bits, rounded up to a
// whole number of bytes.
static uint64_t stored_content_length(uint8_t format,
                                      uint64_t content_length) {
    uint64_t bits_per_value = format - '0';
    uint64_t total_bits = content_length * bits_per_value;
    return (total_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
}


// Read the contents of the leaf described by header, adding each byte to
// the running hash.
//
// If output is not NULL, the bytes are also written to it (out_path is
// used in error messages). Contents are copied exactly as stored, which
// is correct for 8-bit leafs.
static void read_leaf_contents(FILE *tree, struct leaf_header *header,
                               uint8_t *hash, FILE *output, char *out_path) {
    uint64_t n_stored = stored_content_length(header->format,
                                              header->content_length);
    for (uint64_t i = 0; i < n_stored; i++) {
        uint8_t byte = read_byte(tree, hash);
        if (output != NULL) {
            write_byte(output, byte, out_path);
        }
    }
}


// Read a leaf's stored hash byte and compare it with computed_hash, the
// hash of all the leaf's other bytes. Exits with an error if they differ.
static void verify_leaf_hash(FILE *tree, uint8_t computed_hash) {
    uint8_t stored_hash = read_byte(tree, NULL);
    if (computed_hash != stored_hash) {
        fprintf(stderr, "error: incorrect leaf hash 0x%02x should be 0x%02x\n",
                computed_hash, stored_hash);
        exit(1);
    }
}


// Extract the file described by header from tree: write its contents to
// header->pathname (overwriting any existing file), then set the file's
// permissions to those recorded in the leaf.
static void extract_file(FILE *tree, struct leaf_header *header,
                         uint8_t *hash) {
    printf("Extracting: %s\n", header->pathname);

    FILE *output = open_file(header->pathname, "wb");
    read_leaf_contents(tree, header, hash, output, header->pathname);
    close_file(output, header->pathname);

    mode_t permissions = mode_string_to_permissions(header->mode);
    if (chmod(header->pathname, permissions) != 0) {
        perror(header->pathname);
        exit(1);
    }
}


// Convert an ls-style mode string such as "-rwxr-xr-x" into permission
// bits such as 0755.
//
// Characters 1-9 of the string correspond, in order, to the permission
// bits from 0400 (owner read) down to 0001 (others execute); a '-' means
// that bit is not set.
static mode_t mode_string_to_permissions(char *mode) {
    const mode_t permission_bits[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH,
    };
    int n_bits = sizeof permission_bits / sizeof permission_bits[0];

    mode_t permissions = 0;
    for (int i = 0; i < n_bits; i++) {
        // mode[0] is the file type ('-' or 'd'), so permissions start at 1.
        if (mode[i + 1] != '-') {
            permissions |= permission_bits[i];
        }
    }
    return permissions;
}
