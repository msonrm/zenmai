/* PS1 フリースタンディング用 mini-libc(MojoZork が要る分だけ)。
 *
 * 実装するもの: mem 系・str 系と、mojozork の書式(%d %u %x %s %c、幅・0埋め・左寄せ)を
 * 賄う vsnprintf/snprintf。
 * スタブ(呼ばれない経路のリンク解決のみ): fopen 系・printf 系・malloc 系・time・exit。
 * ★glibc ヘッダは含めない(宣言は自前。型は mips32 の ABI に一致させる)。
 */
#include <stdarg.h>

typedef unsigned int size_t;

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s)
        while (n--) *d++ = *s++;
    else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n--; x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}

/* ---- mini vsnprintf ---- */

static int fmt_num(char *tmp, unsigned int v, int base, int upper)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do { tmp[n++] = dig[v % (unsigned)base]; v /= (unsigned)base; } while (v);
    return n;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t out = 0;
    #define PUT(ch) do { if (out + 1 < size) buf[out] = (ch); out++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        int left = 0, zero = 0, width = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else break;
        }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        char tmp[16];
        const char *s = tmp;
        int len = 0, neg = 0;
        switch (*fmt) {
        case 'd': {
            int v = va_arg(ap, int);
            unsigned int u = v < 0 ? (neg = 1, (unsigned int)-v) : (unsigned int)v;
            len = fmt_num(tmp, u, 10, 0);
            break;
        }
        case 'u': len = fmt_num(tmp, va_arg(ap, unsigned int), 10, 0); break;
        case 'x': len = fmt_num(tmp, va_arg(ap, unsigned int), 16, 0); break;
        case 'X': len = fmt_num(tmp, va_arg(ap, unsigned int), 16, 1); break;
        case 'c': tmp[0] = (char)va_arg(ap, int); PUT(tmp[0]); continue;
        case 's': {
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            len = (int)strlen(s);
            int pad = width - len;
            if (!left) while (pad-- > 0) PUT(' ');
            for (int i = 0; i < len; i++) PUT(s[i]);
            if (left) while (pad-- > 0) PUT(' ');
            continue;
        }
        case '%': PUT('%'); continue;
        default: PUT('%'); PUT(*fmt); continue;
        }
        /* 数値: tmp は逆順。幅・0埋め・符号 */
        int total = len + neg;
        int pad = width - total;
        if (!left && !zero) while (pad-- > 0) PUT(' ');
        if (neg) PUT('-');
        if (!left && zero) while (pad-- > 0) PUT('0');
        for (int i = len - 1; i >= 0; i--) PUT(tmp[i]);
        if (left) while (pad-- > 0) PUT(' ');
    }
    if (size) buf[out < size ? out : size - 1] = '\0';
    return (int)out;
    #undef PUT
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- 呼ばれない経路のリンク解決用スタブ ---- */

void *stdin, *stdout, *stderr;
void *fopen(const char *a, const char *b) { (void)a; (void)b; return 0; }
size_t fread(void *p, size_t a, size_t b, void *f) { (void)p; (void)a; (void)b; (void)f; return 0; }
size_t fwrite(const void *p, size_t a, size_t b, void *f) { (void)p; (void)a; (void)b; (void)f; return 0; }
int fclose(void *f) { (void)f; return 0; }
int fseek(void *f, long o, int w) { (void)f; (void)o; (void)w; return -1; }
long ftell(void *f) { (void)f; return -1; }
char *fgets(char *s, int n, void *f) { (void)s; (void)n; (void)f; return 0; }
int printf(const char *f, ...) { (void)f; return 0; }
int fprintf(void *st, const char *f, ...) { (void)st; (void)f; return 0; }
int vfprintf(void *st, const char *f, va_list ap) { (void)st; (void)f; (void)ap; return 0; }
int fputs(const char *s, void *f) { (void)s; (void)f; return 0; }
int puts(const char *s) { (void)s; return 0; }
int putchar(int c) { return c; }
int fflush(void *f) { (void)f; return 0; }
/* LIFO アリーナ(print_zscii の「確保→使用→即解放」パターン用。
 * 先頭 4 バイトに直前の水位を控え、最上段の解放なら水位ごと巻き戻す)。 */
static unsigned char heap[24 * 1024];
static unsigned int heap_top;
void *malloc(size_t n)
{
    n = (n + 3u) & ~3u;
    if (heap_top + n + 8 > sizeof heap)
        return 0;
    unsigned char *p = heap + heap_top;
    ((unsigned int *)p)[0] = heap_top;                      /* 確保前の水位 */
    ((unsigned int *)p)[1] = heap_top + (unsigned int)n + 8; /* 確保後の水位 */
    heap_top += (unsigned int)n + 8;
    return p + 8;
}
void free(void *p)
{
    if (!p)
        return;
    unsigned char *q = (unsigned char *)p - 8;
    if (q >= heap && q + 8 <= heap + sizeof heap) {
        unsigned int prev = ((unsigned int *)q)[0];
        unsigned int end = ((unsigned int *)q)[1];
        if (end == heap_top)
            heap_top = prev;           /* 最上段だけ回収(使い方は常に LIFO) */
    }
}
char *strdup(const char *s) { (void)s; return 0; }
long time(long *t) { if (t) *t = 42; return 42; }
long long __time64(long long *t) { if (t) *t = 42; return 42; }   /* glibc の time64 別名 */
void *fopen64(const char *a, const char *b) { (void)a; (void)b; return 0; }
void rewind(void *f) { (void)f; }
long strtol(const char *s, char **e, int base) { (void)s; (void)e; (void)base; return 0; }

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n--; p++)
        if (*p == (unsigned char)c) return (void *)p;
    return 0;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n--; a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}
void exit(int c) { (void)c; for (;;) { } }
void abort(void) { for (;;) { } }
