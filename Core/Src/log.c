#include "log.h"
#include "main.h"      /* 引入 stm32f1xx_hal.h → cmsis_gcc.h，才有 __get_PRIMASK 等 */
#include <stdio.h>
#include <stdarg.h>

/*
 * 临界区保护：保存 PRIMASK 后关中断，退出时恢复原值。
 * 为什么不是简单的 __disable_irq()/__enable_irq()？
 * 因为如果本函数是在中断里被调用的，出来时无条件开中断会破坏
 * 调用者原本的中断状态（本该继续禁止的中断被放开了）。
 * 保存再恢复，才是嵌套安全的写法。
 */
#define LOG_LOCK(pm)     do { (pm) = __get_PRIMASK(); __disable_irq(); } while (0)
#define LOG_UNLOCK(pm)   do { __set_PRIMASK(pm); } while (0)

static uint8_t s_buf[512];
static volatile uint16_t s_head = 0, s_tail = 0;
static uint32_t s_sent = 0, s_dropped = 0;
static log_level_t s_level = LOG_INFO;
static void (*s_write)(const char *s, uint16_t n) = 0;

void log_init(void (*write)(const char *s, uint16_t n))
{
    s_write = write;
}

void log_printf(log_level_t lv, char tag, const char *fmt, ...)
{
    char line[96];
    va_list ap;
    int off = 0;
    int body_n;
    int total = 0;

    if (lv > LOG_COMPILE_LEVEL) return;
    if (lv > s_level) return;

    /* 约定枚举值越小越"重要"：LOG_ERROR < LOG_WARN < LOG_INFO < LOG_DEBUG */
    {
        static const char lvl_char[] = "EWID";
        uint32_t tick = HAL_GetTick();
        char tmp[10];
        int nd = 0;

        /* 头部 [tick]：手写十进制，不走 snprintf —— 省栈也快 */
        line[off++] = '[';

        if (tick == 0) {
            tmp[nd++] = '0';
        } else {
            while (tick > 0 && nd < (int)sizeof(tmp)) {
                tmp[nd++] = (char)('0' + (tick % 10u));
                tick /= 10u;
            }
        }
        while (nd > 0 && off < (int)sizeof(line) - 1) {
            line[off++] = tmp[--nd];                   /* 逆序取回，才是个位在右 */
        }
        line[off++] = ']';
        line[off++] = '[';

        /* 等级首字母 E/W/I/D */
        line[off++] = (lv <= LOG_DEBUG && lv >= LOG_ERROR) ? lvl_char[lv] : '?';
        line[off++] = ']';
        line[off++] = '[';

        /* 模块标签，一个字符 */
        line[off++] = tag ? tag : '-';
        line[off++] = ']';
        line[off++] = ' ';

        va_start(ap, fmt);
        body_n = vsnprintf(line + off, sizeof(line) - off, fmt, ap);
        va_end(ap);
        if (body_n < 0) return;                        /* 编码错误 */

        if (body_n >= (int)(sizeof(line) - off)) {     /* 被截断 */
            total = (int)sizeof(line) - 1;
        } else {
            total = off + body_n;
        }

        /* 末尾保证有 \r\n：串口终端只有 \n 不会回到行首，会显示成阶梯状 */
        if (total <= (int)sizeof(line) - 2) {
            line[total++] = '\r';
            line[total++] = '\n';
        } else {
            /* 实在挤不下，就占掉正文最后两个字符 */
            line[(int)sizeof(line) - 2] = '\r';
            line[(int)sizeof(line) - 1] = '\n';
            total = (int)sizeof(line);
        }
    }

    {
        uint32_t pm;
        uint16_t cap, h, t, used, i;
        uint16_t need = (uint16_t)total;

        LOG_LOCK(pm);
        cap  = (uint16_t)sizeof(s_buf);                /* 512，2 的幂 */
        h    = s_head;
        t    = s_tail;
        used = (uint16_t)((h - t) & (cap - 1));

        if ((uint16_t)(used + need) > (uint16_t)(cap - 1)) {
            /* 放不下：整行丢弃（不做部分写入，避免半行日志）
               s_dropped 记的是"条数"不是字节数 —— 丢一条只加一 */
            s_dropped++;
        } else {
            i = 0;
            while (i < need) {
                s_buf[h] = (uint8_t)line[i++];
                h = (uint16_t)((h + 1) & (cap - 1));
            }
            s_head = h;
            s_sent++;   /* 入队就算"发出去了"一条：flush 只负责搬运，不再重复计数 */
        }
        LOG_UNLOCK(pm);
    }
}

void log_flush(void)
{
    /* 单消费者模型：只有本函数推进 s_tail；生产者只推进 s_head。 */
    if (!s_write) return;

    for (;;) {
        uint32_t pm;
        uint16_t cap, h, t, chunk;

        LOG_LOCK(pm);
        cap = (uint16_t)sizeof(s_buf);
        h   = s_head;
        t   = s_tail;
        if (h == t) {                                  /* 缓冲空 */
            LOG_UNLOCK(pm);
            break;
        }
        /* 线性可读长度：保证不回绕，一次只发连续一段 */
        chunk = (h > t) ? (uint16_t)(h - t)
                        : (uint16_t)(cap - t);
        LOG_UNLOCK(pm);

        /* 发出去时中断开着：此时生产者只会往 [h,...) 追加，
           [t, t+chunk) 段是稳定的（缓冲区满时生产者会丢包，不会回绕覆盖） */
        s_write((const char *)&s_buf[t], chunk);

        /* 消费完成，推进 tail；s_sent 已经在入队时加过，这里不再加 */
        LOG_LOCK(pm);
        s_tail = (uint16_t)((t + chunk) & (cap - 1));
        LOG_UNLOCK(pm);
    }
}

void log_set_level(log_level_t lv) { s_level = lv; }
log_level_t log_get_level(void) { return s_level; }
uint32_t log_sent(void) { return s_sent; }
uint32_t log_dropped(void) { return s_dropped; }
