#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

struct stat {
    int st_mode;
    long st_size;
};

#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)

int stat(const char *path, struct stat *buf);
int mkdir(const char *path, int mode);

#endif
