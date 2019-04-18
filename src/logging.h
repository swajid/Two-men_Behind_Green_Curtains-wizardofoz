/**
 * Logging and debugging API
 *
 * Defines the DEBUG() INFO() WARN() and ERROR() macros that will print
 * a message depending on the level defined by the variable LEVEL (default is INFO)
 *
 * When debugging, it might be useful to set level to DEBUG, either by setting the variable
 * ENABLE_DEBUG to 1, by using the -DENABLE_DEBUG=1 on compilation, or by directly setting the LEVEL
 * variable to DEBUG, by using -DLEVEL=DEBUG
 *
 * @author Felipe Lalanne <flalanne@niclabs.cl>
 */
#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

typedef enum
{
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
} level_t;

#define LOGGING_MAX_MSG_LEN (256)

#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG (0)
#elif (ENABLE_DEBUG != 0)
#define LEVEL (DEBUG)
#endif

#ifndef LEVEL
#define LEVEL (INFO)
#endif

#ifndef LOGGING_EXIT_ON_ERROR
#define LOGGING_EXIT_ON_ERROR (0)
#endif

#define LEVEL_STR(level) #level

#define LOG(level, func, file, line, msg, ...)                                                                       \
    do                                                                                                               \
    {                                                                                                                \
        if (level >= LEVEL)                                                                                          \
        {                                                                                                            \
            switch (level)                                                                                           \
            {                                                                                                        \
            case DEBUG:                                                                                              \
            case INFO:                                                                                               \
            case WARN:                                                                                               \
                fprintf(stderr, "%s() in %s:%d [" LEVEL_STR(level) "]: " msg "\n", func, file, line, ##__VA_ARGS__); \
            case ERROR:                                                                                              \
                if (errno > 0)                                                                                       \
                {                                                                                                    \
                    fprintf(stderr, "(%s)", strerror(errno));                                                        \
                }                                                                                                    \
                break;                                                                                               \
            case FATAL:                                                                                              \
                exit(EXIT_FAILURE);                                                                                  \
            }                                                                                                        \
        }                                                                                                            \
    } while (0)

// Macro to print debugging messages
#define DEBUG(...) LOG(DEBUG, __func__, __FILE__, __LINE__, __VA_ARGS__)

// Macro to print information messages
#define INFO(...) LOG(INFO, __func__, __FILE__, __LINE__, __VA_ARGS__)

// Macro to print warning messages
#define WARN(...) LOG(WARN, __func__, __FILE__, __LINE__, __VA_ARGS__)

// Macro to print error messages
#define ERROR(...) LOG(ERROR, __func__, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOGGING_H */
