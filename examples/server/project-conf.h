#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* QUEUEBUF_CONF_NUM specifies the number of queue buffers. Queue
   buffers are used throughout the Contiki netstack but the
   configuration option can be tweaked to save memory. Performance can
   suffer with a too low number of queue buffers though.

   We reduce the queue buffer size to allow the two implementation to fit
   in the .bss memory section. */
#define QUEUEBUF_CONF_NUM 4

/**
 * Not enough memory in contiki for multiple clients
 */
#define CONFIG_HTTP2_MAX_CLIENTS (1)

/** 
 * Disable hpack dynamic table by default
 */
#define CONFIG_HTTP2_HEADER_TABLE_SIZE (0)

#endif
