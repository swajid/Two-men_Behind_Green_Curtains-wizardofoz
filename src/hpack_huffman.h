//
// Created by gabriel on 18-07-19.
//

#ifndef HPACK_HUFFMAN_H
#define HPACK_HUFFMAN_H

#include <stdint.h>

#define HUFFMAN_TABLE_SIZE (256)
#define NUMBER_OF_CODE_LENGTHS (21)

//TODO move macro to conf file
#define HPACK_INCLUDE_HUFFMAN_LENGTH_TABLE (1) //DEFAULT

#ifdef HPACK_INCLUDE_HUFFMAN_LENGTH_TABLE
#define INCLUDE_HUFFMAN_LENGTH_TABLE (HPACK_INCLUDE_HUFFMAN_LENGTH_TABLE)
#endif

//TODO move macro to conf file
#define HPACK_INCLUDE_HUFFMAN_COMPRESSION (1) //DEFAULT

#ifdef HPACK_INCLUDE_HUFFMAN_COMPRESSION
#define INCLUDE_HUFFMAN_COMPRESSION (HPACK_INCLUDE_HUFFMAN_COMPRESSION)
#endif

#ifdef INCLUDE_HUFFMAN_COMPRESSION
typedef struct {
    uint32_t code;
    uint8_t length;
} huffman_encoded_word_t;

typedef struct {
    const uint32_t codes[HUFFMAN_TABLE_SIZE];
    const uint8_t symbols[HUFFMAN_TABLE_SIZE];
#ifdef INCLUDE_HUFFMAN_LENGTH_TABLE
    const uint8_t huffman_length[HUFFMAN_TABLE_SIZE];
#endif
    const uint8_t sR[NUMBER_OF_CODE_LENGTHS];
    const uint8_t code_length[NUMBER_OF_CODE_LENGTHS];
} hpack_huffman_tree_t;



int8_t hpack_huffman_encode(huffman_encoded_word_t *result, uint8_t sym);

int8_t hpack_huffman_decode(huffman_encoded_word_t *encoded, uint8_t* sym);
#endif
#endif //HPACK_HUFFMAN_H
