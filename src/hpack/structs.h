
#ifndef TWO_HPACK_STRUCTS
#define TWO_HPACK_STRUCTS


#include <stdint.h>             /* for int8_t, uint8_t, uint32_t, uint16_t*/
#include "headers.h"            /* for MAX_HEADER_SETTINGS */
#include "config.h"


#ifdef HPACK_CONF_MAXIMUM_INTEGER
#define HPACK_MAXIMUM_INTEGER (HPACK_CONF_MAXIMUM_INTEGER)
#else
#define HPACK_MAXIMUM_INTEGER (4096)
#endif

#ifdef CONFIG_INCLUDE_DYNAMIC_TABLE
#define HPACK_INCLUDE_DYNAMIC_TABLE (CONFIG_INCLUDE_DYNAMIC_TABLE)
#else
#define HPACK_INCLUDE_DYNAMIC_TABLE (1)
#endif

#ifdef CONFIG_HEADER_TABLE_SIZE
#define HPACK_MAX_DYNAMIC_TABLE_SIZE (CONFIG_HEADER_TABLE_SIZE)
#else
#define HPACK_MAX_DYNAMIC_TABLE_SIZE (256)
#endif

typedef enum __attribute__((__packed__)) {
    INDEXED_HEADER_FIELD                            = (uint8_t) 128,
    LITERAL_HEADER_FIELD_WITH_INCREMENTAL_INDEXING  = (uint8_t) 64,
    LITERAL_HEADER_FIELD_WITHOUT_INDEXING           = (uint8_t) 0,
    LITERAL_HEADER_FIELD_NEVER_INDEXED              = (uint8_t) 16,
    DYNAMIC_TABLE_SIZE_UPDATE                       = (uint8_t) 32
} hpack_preamble_t;

#pragma pack(push, 1)

typedef struct {
    uint32_t index;
    uint32_t name_length;
    uint32_t value_length;
    uint8_t *name_string;
    uint8_t *value_string;
    uint16_t dynamic_table_size;
    uint8_t huffman_bit_of_name;
    uint8_t huffman_bit_of_value;
    hpack_preamble_t preamble;
} hpack_encoded_header_t;

//typedefs for dinamic
//size: 10 bytes in params + size of buffer
typedef
    #if HPACK_INCLUDE_DYNAMIC_TABLE
    struct {
    uint16_t max_size;
    uint16_t first;
    uint16_t next;
    uint16_t actual_size;
    uint16_t n_entries;
    uint32_t settings_max_table_size;
    char buffer[HPACK_MAX_DYNAMIC_TABLE_SIZE];
}
    #else
    char //case No dynamic table mode, this conversion to char makes the code cleaner
    #endif
hpack_dynamic_table_t;

#pragma pack(pop)

typedef enum __attribute__((__packed__)){
    PROTOCOL_ERROR  = (int8_t) - 1,
    INTERNAL_ERROR  = (int8_t) - 2
} hpack_error_t;

#endif
