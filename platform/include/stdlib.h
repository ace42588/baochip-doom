#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t n);
void free(void *p);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
int atoi(const char *s);
double atof(const char *s);
int abs(int x);
long labs(long x);
void exit(int code);
void abort(void);
int atexit(void (*func)(void));
char *getenv(const char *name);
int system(const char *cmd);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
int access(const char *path, int mode);

#ifdef __cplusplus
}
#endif

#endif
