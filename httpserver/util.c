#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

char* join_string(char *str1, char *str2, size_t *size) {
    assert(str1!=NULL && str2!=NULL);
    char *ret = (char *) malloc(strlen(str1) + strlen(str2) + 1), *p = ret;
    for(; (*p=*str1); p++, str1++);
    for(; (*p=*str2); p++, str2++);
    if(size != NULL) *size = (p-ret)*sizeof(char);
    return ret;
}

char* join_path(char *path, char *subpath, size_t *size) {
    assert(path!=NULL && subpath!=NULL);
    int path_len = strlen(path), subpath_len = strlen(subpath);
    char *fullpath = (char *) malloc(path_len+subpath_len+2), *p = fullpath;
    char prev = '\0';
    for(; *path; path++) {
        if(*path==prev && prev=='/') continue;
        *p = *path;
        prev = *path;
        p++;
    }
    if(prev!='/') {
        *p++ = '/';
        prev = '/';
    }
    for(; *subpath; subpath++) {
        if(*subpath==prev && prev=='/') continue;
        *p = *subpath;
        prev = *subpath;
        p++;
    }
    *p = '\0';
    if(size!=NULL) *size = (p-fullpath)*sizeof(char);
    return fullpath;
}

char* get_parent_name(char *dir_name) {
    size_t size;
    char *fullpath = join_path(dir_name, "/", &size);
    if(size <= 1) return fullpath;

    char *p = fullpath+size-2; 
    while(p!=fullpath && *p!='/') p--; 
    *p++ = '/';
    *p = '\0';
    return fullpath;
}
