/*
 * log.h
 *
 *  Created on: 2026年10月17日
 *      Author: ASUS
 */
#ifndef LOG_H
#define LOG_H
#include <stdint.h>

typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL LOG_DEBUG
#endif

void log_init(void (*write)(const char *s, uint16_t n));
void log_set_level(log_level_t lv);
log_level_t log_get_level(void);
uint32_t log_sent(void);
uint32_t log_dropped(void);
void log_printf(log_level_t lv, char tag, const char *fmt, ...);
void log_flush(void);

#define LOG_E(tag, ...) log_printf(LOG_ERROR, tag, __VA_ARGS__)
#define LOG_W(tag, ...) log_printf(LOG_WARN,  tag, __VA_ARGS__)
#define LOG_I(tag, ...) log_printf(LOG_INFO,  tag, __VA_ARGS__)
#define LOG_D(tag, ...) log_printf(LOG_DEBUG, tag, __VA_ARGS__)

#endif



