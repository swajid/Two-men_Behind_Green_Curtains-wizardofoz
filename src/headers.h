#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>

#ifndef CONF_MAX_HEADER_NAME_LEN
#define MAX_HEADER_NAME_LEN (16)
#else
#define MAX_HEADER_NAME_LEN (CONF_MAX_HEADER_NAME_LEN)
#endif

#ifndef CONF_MAX_HEADER_VALUE_LEN
#define MAX_HEADER_VALUE_LEN (32)
#else
#define MAX_HEADER_VALUE_LEN (CONF_MAX_HEADER_VALUE_LEN)
#endif


/** 
 * Data structure to store a single headers
 */
typedef struct {
    char name[MAX_HEADER_NAME_LEN + 1];
    char value[MAX_HEADER_VALUE_LEN + 1];
} header_t;

/**
 * Data structure to store a header list
 *
 * Should not be used directly, use provided API methods
 */
typedef struct {
    header_t * headers;
    int count;
    int maxlen;
} headers_t;


/**
 * Initialize header list with specified array and set list counter to zero
 * This function will reset provided memory
 *
 * @param headers headers data structure
 * @param hlist h pointer to header_t list of size maxlen
 * @param maxlen maximum number of elements that the header list can store
 * @return 0 if ok -1 if an error ocurred
 */
int headers_init(headers_t * headers, header_t * hlist, int maxlen);

/** 
 * Add a header to the list, if header already exists, concatenate value 
 * with a ',' as specified in 
 * https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
 *
 * @param headers headers data structure
 * @param name header name
 * @param value header value
 * @return 0 if ok -1 if an error ocurred
 */
int headers_add(headers_t * headers, const char * name, const char * value);

/**
 * Set the header for a given name, if the header is already set
 * it replaces the value with the new given value
 *
 * @param headers headers data structure
 * @param name header name
 * @param value header value
 * @return 0 if ok -1 if an error ocurred
 */
int headers_set(headers_t * headers, const char * name, const char * value);

/** 
 * Get a pointere to the value of the header with name 'name'
 *
 * @param headers header list data structure
 * @param name name of the header to search
 * @param value header value of the header with name 'name' or NULL if error
 * */
char * headers_get(headers_t * headers, const char * name);

/**
 * Return total number of headers in the header list
 */
int headers_count(headers_t * headers);

/**
 * Return header name from header in the position given by index
 */
char * headers_get_name_from_index(headers_t * headers, int index);

/**
 * Return header value from header in the position given by index
 */
char * headers_get_value_from_index(headers_t * headers, int index);

/** 
 * Calculate size of header list for http/2
 */
int headers_get_header_list_size(headers_t* headers);


#endif
