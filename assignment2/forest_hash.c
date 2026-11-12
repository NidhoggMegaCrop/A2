///////////////////////////////////////////////////////////////////////////////
// COMP1521 26T3 --- Assignment 2: `forest', a simple file archiver          //
// <https://www.cse.unsw.edu.au/~cs1521/26T3/assignments/ass2/index.html>     //
//                                                                            //
// 2024-03-27   v1.0    Team COMP1521 <cs1521 at cse.unsw.edu.au>             //
//                                                                            //
///////////////////// YOU DO NOT NEED TO MODIFY THIS FILE. /////////////////////

////////////////////////////// DO NOT CHANGE THIS FILE. //////////////////////////////
// THIS FILE IS NOT SUBMITTED AND WILL BE PROVIDED AS IS DURING TESTING AND MARKING //
////////////////////////////// DO NOT CHANGE THIS FILE. //////////////////////////////

#include <stdint.h>

// DO NOT CHANGE THIS FUNCTION - use it to calculate tree hashes (subsets 1, 2, and 3)

//
// The djb2-xor hash: hash(i) = hash(i - 1) * 33 ^ str[i];
// For more information  http://www.cse.yorku.ca/~oz/hash.html
//
// tree_hash(), given the current hash value and a byte value, returns the new hash value
// An tree hash is calculated by calling tree_hash once for each byte in the tree except the last byte
// current_hash_value should be zero, for the first byte in the tree

uint8_t tree_hash(uint8_t current_hash_value, uint8_t byte_value) {
    return ((current_hash_value * 33) & 0xff) ^ byte_value;
}

////////////////////////////// DO NOT CHANGE THIS FILE. //////////////////////////////
// THIS FILE IS NOT SUBMITTED AND WILL BE PROVIDED AS IS DURING TESTING AND MARKING //
////////////////////////////// DO NOT CHANGE THIS FILE. //////////////////////////////
