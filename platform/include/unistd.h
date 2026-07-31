#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ssize_t;

int isatty(int fd);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

#ifdef __cplusplus
}
#endif

#endif
