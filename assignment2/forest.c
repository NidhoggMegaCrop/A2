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


// ADD YOUR FUNCTION PROTOTYPES (AND STRUCTS IF ANY) HERE


// print the files & directories stored in tree_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes
// are also printed (subset 0)

void list_tree(char *tree_pathname, int long_listing) {

    // REPLACE THIS CODE WITH YOUR CODE

    printf("list_tree called to list tree: '%s'\n", tree_pathname);

    if (long_listing) {
        printf("long listing with permissions & sizes specified\n");
    }
}


// check the files & directories stored in tree_pathname (subset 1)
//
// prints the files & directories stored in tree_pathname with a message
// either, indicating the hash byte is correct, or indicating the hash byte
// is incorrect, what the incorrect value is and the correct value would be

void check_tree(char *tree_pathname) {

    // REPLACE THIS PRINTF WITH YOUR CODE

    printf("check_tree called to check tree: '%s'\n", tree_pathname);
}


// extract the files/directories stored in tree_pathname (subset 1 & 3)

void extract_tree(char *tree_pathname) {

    // REPLACE THIS PRINTF WITH YOUR CODE

    printf("extract_tree called to extract tree: '%s'\n", tree_pathname);
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
