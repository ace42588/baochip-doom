/*
 * Minimal libc + heap for bare-metal doomgeneric on Baochip-1x.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "bao.h"
#include "board.h"
#include "bao_wad.h"
#include "bao_display.h"

int errno;

#ifndef BAO_HEAP_SIZE
#define BAO_HEAP_SIZE (1400u * 1024u)
#endif

typedef struct {
    size_t size;
} alloc_hdr_t;

static uint8_t g_heap[BAO_HEAP_SIZE] __attribute__((aligned(8)));
static size_t g_heap_used;

void *malloc(size_t n)
{
    size_t align = 8;
    size_t used = (g_heap_used + (align - 1)) & ~(align - 1);
    size_t need;
    alloc_hdr_t *h;
    if (n == 0) {
        n = 1;
    }
    need = (sizeof(alloc_hdr_t) + n + (align - 1)) & ~(align - 1);
    if (used + need > BAO_HEAP_SIZE) {
        return NULL;
    }
    h = (alloc_hdr_t *)&g_heap[used];
    h->size = n;
    g_heap_used = used + need;
    return h + 1;
}

void free(void *p)
{
    (void)p;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t n = nmemb * size;
    void *p = malloc(n);
    if (p) {
        memset(p, 0, n);
    }
    return p;
}

void *realloc(void *ptr, size_t size)
{
    alloc_hdr_t *h;
    void *n;
    size_t copy;
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        return NULL;
    }
    h = (alloc_hdr_t *)ptr - 1;
    n = malloc(size);
    if (!n) {
        return NULL;
    }
    copy = h->size < size ? h->size : size;
    memcpy(n, ptr, copy);
    return n;
}

/* ---- Strings / ctype ---- */

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = 0;
    }
    return dst;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && (*a == *b)) {
        a++;
        b++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strcasecmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = ca - 'A' + 'a';
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = cb - 'A' + 'a';
        }
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
    }
    {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = ca - 'A' + 'a';
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = cb - 'A' + 'a';
        }
        return ca - cb;
    }
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *b) {
        int ca = *a;
        int cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = ca - 'A' + 'a';
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = cb - 'A' + 'a';
        }
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst + strlen(dst);
    while ((*d++ = *src++)) {
    }
    return dst;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    if (c == 0) {
        return (char *)s;
    }
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    size_t n;
    if (!needle || !*needle) {
        return (char *)haystack;
    }
    n = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, n) == 0) {
            return (char *)haystack;
        }
    }
    return NULL;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) {
        memcpy(d, s, n);
    }
    return d;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) {
        return dst;
    }
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dst;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int isalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isprint(int c)
{
    return c >= 0x20 && c <= 0x7e;
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

long labs(long x)
{
    return x < 0 ? -x : x;
}

int atoi(const char *s)
{
    int sign = 1;
    int v = 0;
    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (isdigit((unsigned char)*s)) {
        v = v * 10 + (*s - '0');
        s++;
    }
    return sign * v;
}

double atof(const char *s)
{
    /* Enough for doom config floats: parse as integer part only or simple decimal */
    int sign = 1;
    double v = 0;
    double frac = 0;
    double base = 0.1;
    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (isdigit((unsigned char)*s)) {
        v = v * 10.0 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        while (isdigit((unsigned char)*s)) {
            frac += (*s - '0') * base;
            base *= 0.1;
            s++;
        }
    }
    return sign * (v + frac);
}

int sscanf(const char *str, const char *fmt, ...)
{
    /* Minimal stub: support "%i" / "%d" / "%x" single conversion used by m_config */
    va_list ap;
    int count = 0;
    va_start(ap, fmt);
    if (fmt[0] == '%' && (fmt[1] == 'i' || fmt[1] == 'd')) {
        int *out = va_arg(ap, int *);
        *out = atoi(str);
        count = 1;
    } else if (fmt[0] == '%' && fmt[1] == 'x') {
        unsigned *out = va_arg(ap, unsigned *);
        unsigned v = 0;
        const char *p = str;
        while (*p) {
            char c = *p++;
            int d;
            if (c >= '0' && c <= '9') {
                d = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                d = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                d = c - 'A' + 10;
            } else {
                break;
            }
            v = (v << 4) | (unsigned)d;
        }
        *out = v;
        count = 1;
    }
    va_end(ap);
    return count;
}

/* ---- stdio ---- */

struct FILE {
    int is_wad;
    size_t pos;
    size_t len;
    const uint8_t *data;
};

static FILE g_stdout_file;
static FILE g_stderr_file;
static FILE g_stdin_file;
static FILE g_wad_file;

FILE *stdout = &g_stdout_file;
FILE *stderr = &g_stderr_file;
FILE *stdin = &g_stdin_file;

int putchar(int c)
{
    uart_putc(BOARD_UART, (char)c);
    return c;
}

int puts(const char *s)
{
    uart_puts(BOARD_UART, s);
    uart_putc(BOARD_UART, '\r');
    uart_putc(BOARD_UART, '\n');
    bao_display_log_line(s);
    return 0;
}

int fputc(int c, FILE *stream)
{
    (void)stream;
    return putchar(c);
}

int fputs(const char *s, FILE *stream)
{
    (void)stream;
    uart_puts(BOARD_UART, s);
    return 0;
}

int printf(const char *fmt, ...)
{
    /* Route through mini_printf-compatible path: limited but enough for DOOM logs. */
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    /* Very small formatter: reuse mini_printf by expanding via bao helper */
    extern void mini_vprintf(const char *fmt, va_list ap);
    /* Fall back: format into UART via mini_printf if available as varargs only.
     * mini_printf is varargs; call it indirectly by copying fmt-only messages. */
    (void)ap;
    va_end(ap);

    /* Use a tiny hand-rolled path: print format literally if no %, else mini_printf. */
    if (!strchr(fmt, '%')) {
        uart_puts(BOARD_UART, fmt);
        bao_display_log_line(fmt);
        return 0;
    }

    /* Forward known DOOM prints via mini_printf by re-calling with captured args — 
     * implement proper vsnprintf below. */
    va_start(ap, fmt);
    {
        /* Inline minimal vsnprintf */
        extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
        int n = bao_vsnprintf(buf, sizeof(buf), fmt, ap);
        uart_puts(BOARD_UART, buf);
        bao_display_log_line(buf);
        va_end(ap);
        return n;
    }
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    (void)stream;
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
    int n = bao_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_puts(BOARD_UART, buf);
    if (stream == stderr && buf[0] && buf[0] != '\n') {
        bao_display_fatal(buf);
    } else {
        bao_display_log_line(buf);
    }
    return n;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    (void)stream;
    char buf[256];
    extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
    int n = bao_vsnprintf(buf, sizeof(buf), fmt, ap);
    uart_puts(BOARD_UART, buf);
    if (stream == stderr && buf[0] && buf[0] != '\n') {
        bao_display_fatal(buf);
    } else {
        bao_display_log_line(buf);
    }
    return n;
}

int sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
    int n = bao_vsnprintf(str, 1024, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
    int n = bao_vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return n;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    extern int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap);
    return bao_vsnprintf(str, size, fmt, ap);
}

int bao_vsnprintf(char *out, size_t out_sz, const char *fmt, va_list ap)
{
    size_t o = 0;
    if (out_sz == 0) {
        return 0;
    }

    while (*fmt && o + 1 < out_sz) {
        if (*fmt != '%') {
            out[o++] = *fmt++;
            continue;
        }
        fmt++;

        int zero_pad = 0;
        int width = 0;
        int prec = -1;
        int long_mod = 0;

        while (*fmt == '0' || *fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#') {
            if (*fmt == '0') {
                zero_pad = 1;
            }
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            prec = 0;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                prec = prec * 10 + (*fmt++ - '0');
            }
        }
        if (*fmt == 'l') {
            long_mod = 1;
            fmt++;
        }
        char spec = *fmt ? *fmt++ : '\0';

        if (spec == '%') {
            out[o++] = '%';
            continue;
        }

        if (spec == 's') {
            const char *s = va_arg(ap, const char *);
            int n = 0;
            if (!s) {
                s = "(null)";
            }
            while (s[n] && (prec < 0 || n < prec)) {
                n++;
            }
            while (width > n && o + 1 < out_sz) {
                out[o++] = ' ';
                width--;
            }
            while (n-- > 0 && o + 1 < out_sz) {
                out[o++] = *s++;
            }
            continue;
        }

        if (spec == 'c') {
            out[o++] = (char)va_arg(ap, int);
            continue;
        }

        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'p') {
            char tmp[24];
            int n = 0;
            int neg = 0;
            int base = (spec == 'x' || spec == 'X' || spec == 'p') ? 16 : 10;
            unsigned long v;
            int min_digits;

            if (spec == 'p') {
                v = (unsigned long)va_arg(ap, void *);
            } else if (spec == 'd' || spec == 'i') {
                long sv = long_mod ? va_arg(ap, long) : va_arg(ap, int);
                if (sv < 0) {
                    neg = 1;
                    v = (unsigned long)(-(sv + 1)) + 1UL;
                } else {
                    v = (unsigned long)sv;
                }
            } else {
                v = long_mod ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            }

            if (v == 0) {
                tmp[n++] = '0';
            }
            while (v > 0 && n < (int)sizeof(tmp)) {
                int d = (int)(v % (unsigned)base);
                tmp[n++] = (char)(d < 10 ? '0' + d : (spec == 'X' ? 'A' : 'a') + d - 10);
                v /= (unsigned)base;
            }

            min_digits = (prec >= 0) ? prec : (zero_pad ? width - neg : 0);
            if (spec == 'p' && o + 2 < out_sz) {
                out[o++] = '0';
                out[o++] = 'x';
            }
            if (neg && o + 1 < out_sz) {
                out[o++] = '-';
            }
            while (n < min_digits && n < (int)sizeof(tmp)) {
                tmp[n++] = '0';
            }
            while (width > n + neg && o + 1 < out_sz) {
                out[o++] = ' ';
                width--;
            }
            while (n-- > 0 && o + 1 < out_sz) {
                out[o++] = tmp[n];
            }
            continue;
        }

        out[o++] = '%';
        if (o + 1 < out_sz && spec) {
            out[o++] = spec;
        }
    }
    out[o] = 0;
    return (int)o;
}

FILE *fopen(const char *path, const char *mode)
{
    (void)mode;
    if (bao_wad_path_match(path)) {
        g_wad_file.is_wad = 1;
        g_wad_file.pos = 0;
        g_wad_file.data = bao_wad_data();
        g_wad_file.len = bao_wad_size();
        if (!g_wad_file.data || g_wad_file.len == 0) {
            return NULL;
        }
        return &g_wad_file;
    }
    return NULL;
}

int fclose(FILE *f)
{
    if (f) {
        f->is_wad = 0;
        f->pos = 0;
        f->data = NULL;
        f->len = 0;
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || !stream->is_wad || !stream->data) {
        return 0;
    }
    size_t bytes = size * nmemb;
    size_t left = stream->len - stream->pos;
    if (bytes > left) {
        bytes = left;
    }
    memcpy(ptr, stream->data + stream->pos, bytes);
    stream->pos += bytes;
    return size ? (bytes / size) : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)stream;
    /* Pretend success for config writes; ignore data. */
    return nmemb;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream || !stream->is_wad) {
        return -1;
    }
    size_t np = stream->pos;
    if (whence == 0) {
        np = (size_t)offset;
    } else if (whence == 1) {
        np = stream->pos + (size_t)offset;
    } else if (whence == 2) {
        np = stream->len + (size_t)offset;
    } else {
        return -1;
    }
    if (np > stream->len) {
        return -1;
    }
    stream->pos = np;
    return 0;
}

long ftell(FILE *stream)
{
    if (!stream) {
        return -1;
    }
    return (long)stream->pos;
}

int feof(FILE *stream)
{
    if (!stream) {
        return 1;
    }
    return stream->pos >= stream->len;
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

void exit(int code)
{
    mini_printf("\r\nexit(%d)\r\n", code);
    for (;;) {
        __asm__ volatile ("wfi");
    }
}

void abort(void)
{
    exit(1);
}

int atexit(void (*func)(void))
{
    (void)func;
    return 0;
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

int system(const char *cmd)
{
    (void)cmd;
    return -1;
}

int remove(const char *path)
{
    (void)path;
    return 0;
}

int rename(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

int access(const char *path, int mode)
{
    (void)mode;
    return bao_wad_path_match(path) ? 0 : -1;
}

int stat(const char *path, struct stat *buf)
{
    if (!bao_wad_path_match(path) || !buf) {
        return -1;
    }
    buf->st_mode = 0100644;
    buf->st_size = (long)bao_wad_size();
    return 0;
}

int mkdir(const char *path, int mode)
{
    (void)path;
    (void)mode;
    return 0;
}

/* Math stubs used rarely; soft-float from libgcc covers most. */
double fabs(double x)
{
    return x < 0 ? -x : x;
}

float fabsf(float x)
{
    return x < 0 ? -x : x;
}
