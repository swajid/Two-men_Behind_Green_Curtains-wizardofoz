#ifndef HTTP2_STRUCTS_H
#define HTTP2_STRUCTS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "headers.h"
#include "frames.h"

/*Definition of max headers frame buffer size*/
#define HTTP2_MAX_HBF_BUFFER 16384
/*Definition of max buffer size*/
#define HTTP2_MAX_BUFFER_SIZE 16384
/*Define max count of headers*/
#define HTTP2_MAX_HEADER_COUNT 256


/*Default settings values*/
#define DEFAULT_HEADER_TABLE_SIZE 4096
#define DEFAULT_ENABLE_PUSH 0
#define DEFAULT_MAX_CONCURRENT_STREAMS 1
#define DEFAULT_INITIAL_WINDOW_SIZE 65535
#define DEFAULT_MAX_FRAME_SIZE 16384
#define DEFAULT_MAX_HEADER_LIST_SIZE 99999

/*Error codes*/
typedef enum{
  HTTP2_NO_ERROR = (uint32_t) 0x0,
  HTTP2_PROTOCOL_ERROR = (uint32_t) 0x1,
  HTTP2_INTERNAL_ERROR = (uint32_t) 0x2,
  HTTP2_FLOW_CONTROL_ERROR = (uint32_t) 0x3,
  HTTP2_SETTINGS_TIMEOUT = (uint32_t) 0x4,
  HTTP2_STREAM_CLOSED = (uint32_t) 0x5,
  HTTP2_FRAME_SIZE_ERROR = (uint32_t) 0x6,
  HTTP2_REFUSED_STREAM = (uint32_t) 0x7,
  HTTP2_CANCEL = (uint32_t) 0x8,
  HTTP2_COMPRESSION_ERROR = (uint32_t) 0x9,
  HTTP2_CONNECT_ERROR = (uint32_t) 0xa,
  HTTP2_ENHANCE_YOUR_CALM = (uint32_t) 0xb,
  HTTP2_INADEQUATE_SECURITY = (uint32_t) 0xc,
  HTTP2_HTTP_1_1_REQUIRED = (uint32_t) 0xd
}h2_error_code_t;

/*Enumerator for stream states*/
typedef enum {
    STREAM_IDLE,
    STREAM_OPEN,
    STREAM_HALF_CLOSED_REMOTE,
    STREAM_HALF_CLOSED_LOCAL,
    STREAM_CLOSED
} h2_stream_state_t;

/*Enumerator for settings parameters*/
typedef enum SettingsParameters{
  HEADER_TABLE_SIZE = (uint16_t) 0x1,
  ENABLE_PUSH = (uint16_t) 0x2,
  MAX_CONCURRENT_STREAMS = (uint16_t) 0x3,
  INITIAL_WINDOW_SIZE = (uint16_t) 0x4,
  MAX_FRAME_SIZE = (uint16_t) 0x5,
  MAX_HEADER_LIST_SIZE = (uint16_t) 0x6
}sett_param_t;

/*Struct for HTTP2 Stream*/
typedef struct HTTP2_STREAM {
    uint32_t stream_id;
    h2_stream_state_t state;
} h2_stream_t;

typedef struct HTTP2_WINDOW_MANAGER {
    uint32_t window_size;
    uint32_t window_used;
} h2_window_manager_t;

typedef struct HTTP2_DATA {
    uint32_t size;
    uint8_t buf[DEFAULT_INITIAL_WINDOW_SIZE]; /*Placeholder*/
    uint32_t processed;
} http2_data_t;


/*Struct for storing HTTP2 states*/
typedef struct HTTP2_STATES {
    uint8_t is_server;
    uint32_t remote_settings[6];
    uint32_t local_settings[6];
    /*uint32_t local_cache[6]; Could be implemented*/
    uint8_t wait_setting_ack;
    h2_stream_t current_stream;
    uint32_t last_open_stream_id;
    uint8_t header_block_fragments[HTTP2_MAX_HBF_BUFFER];
    uint8_t header_block_fragments_pointer; //points to the next byte to write in
    uint8_t waiting_for_end_headers_flag;   //bool
    uint8_t received_end_stream;
    h2_window_manager_t incoming_window;
    h2_window_manager_t outgoing_window;
    uint8_t sent_goaway;
    uint8_t received_goaway;        // bool
    uint8_t debug_data_buffer[0];   // TODO not implemented yet
    uint8_t debug_size;             // TODO not implemented yet
    //Hpack dynamic table
    hpack_states_t hpack_states;

    frame_header_t header;
    http2_data_t data;
    header_t headers_buf[HTTP2_MAX_HEADER_COUNT]; /*TODO: remove this and make it dinamic*/
    headers_t headers;
} h2states_t;



#endif