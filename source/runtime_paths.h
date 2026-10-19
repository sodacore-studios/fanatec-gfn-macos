// SPDX-License-Identifier: GPL-2.0-only
#ifndef RUNTIME_PATHS_H
#define RUNTIME_PATHS_H
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
// Resolve per-user writable paths at runtime; release binaries contain no build paths.
static inline int runtime_path(char *out,size_t capacity,const char *leaf) {
    struct passwd *user=getpwuid(getuid());
    if(!user||!user->pw_dir)return 0;
    char directory[PATH_MAX];
    int n=snprintf(directory,sizeof(directory),"%s/Library/Application Support/FanatecGFN",user->pw_dir);
    if(n<0||(size_t)n>=sizeof(directory))return 0;
    struct stat info;
    if(mkdir(directory,0700) && (lstat(directory,&info)||!S_ISDIR(info.st_mode)||info.st_uid!=getuid()))return 0;
    n=snprintf(out,capacity,"%s/%s",directory,leaf);
    return n>=0&&(size_t)n<capacity;
}
#endif
