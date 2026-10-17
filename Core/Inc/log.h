/* log.h */
#ifndef LOG_H
#define LOG_H
#include <stdint.h>

typedef enum {
    LOG_ERROR = 0,   /* 只有坏了才打 */
    LOG_WARN  = 1,   /* 可疑但还能跑 */
    LOG_INFO  = 2,   /* 正常的关键节点（开机、模式切换） */
    LOG_DEBUG = 3    /* 排查时才要的细节，默认这一档最多 */
} log_level_t;

/* 编译期上限：比它更啰嗦的语句在编译时就被扔掉，不占 Flash 也不占时间 */
#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL  LOG_DEBUG
#endif

void        log_init(void (*write)(const char *s, uint16_t n)); /* 注入"怎么发出去" */
void        log_set_level(log_level_t lv);                      /* 运行期也能改 */
log_level_t log_get_level(void);
uint32_t    log_sent(void);       /* 已经发出去的条数 */
uint32_t    log_dropped(void);    /* 因为缓冲满被丢掉的条数 */

void log_printf(log_level_t lv, char tag, const char *fmt, ...); /* 主循环/普通代码里用 */
void log_flush(void);                                           /* 主循环里反复调 */

#define LOG_E(tag, ...) log_printf(LOG_ERROR, tag, __VA_ARGS__)
#define LOG_W(tag, ...) log_printf(LOG_WARN,  tag, __VA_ARGS__)
#define LOG_I(tag, ...) log_printf(LOG_INFO,  tag, __VA_ARGS__)
#define LOG_D(tag, ...) log_printf(LOG_DEBUG, tag, __VA_ARGS__)

#endif
