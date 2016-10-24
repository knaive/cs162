#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tokenizer.h"

int split_old(char *exe, char separator, char* tokens[]) {
    int count = 0;
    char last = separator;
    while(*exe != 0) {
        if(*exe == separator) {
            last = *exe;
            *exe = 0;
        } else if(last == separator) {
            last = *exe;
            tokens[count] = exe;
            count++;
        }
        exe++;
    }
    tokens[count] = NULL;
    return count;
}

static int split(char *exe, char separator, char* tokens[]) {
    char *saveptr;
    int count = 0;
    
    for(tokens[count] = strtok_r(exe, &separator, &saveptr); tokens[count]!=NULL;
            tokens[++count] = strtok_r(NULL, &separator, &saveptr));

    return count;
}

//of course we can use execvp, but the course prohibit it
char* get_full_path(char *cmd) {
    if(access(cmd, F_OK) != -1) return cmd;

    const char* env = getenv("PATH");
    int len = 0, count = 1, cmd_len = strlen(cmd);
    const char *p = env;
    while(*p!='\0') {
        if(*p == ':') {
            count++;
        }
        p++;
        len++;
    }

    char *env_copy = (char *) malloc(len);
    char **paths = (char **) malloc(sizeof(char *)*count);
    strcpy(env_copy, env);
    split(env_copy, ':', paths);
    char *fullpath = (char *) malloc(len+cmd_len+2);

    char **temp_paths = paths;
    while(count--) {
        *fullpath = '\0';
        strcat(fullpath, *temp_paths);
        int size = strlen(fullpath);
        if(fullpath[size-1]!='/') {
            fullpath[size] = '/';
            fullpath[size+1] = '\0';
        }
        strcat(fullpath, cmd);
        if(access(fullpath, F_OK) != -1) {
            free(env_copy);
            free(paths);
            return fullpath;
        }
        temp_paths++;
    }
    free(env_copy);
    free(paths);
    free(fullpath);
    return NULL;
}

struct tokens* tokens_create(char *command) {
    if(command==NULL) return NULL; 

    struct tokens* ts = (struct tokens*) malloc(sizeof(struct tokens));
    
    ts->cmd_len = strlen(command);
    ts->command = (char*) malloc(sizeof(char)*(ts->cmd_len+1));
    strcpy(ts->command, command);

    ts->tokens = (char**)malloc(sizeof(char*) * 4096);
    ts->count = split(command, ' ', ts->tokens);
    return ts;
}

int tokens_lookup(char* target, struct tokens* tokens) {
    assert((tokens!=NULL));
    assert((target!=NULL));

    char** p = tokens->tokens;
    int index = 0;
    while(*p != NULL) {
        if(strcmp(*p, target) == 0) {
            return index;
        }
        index++;
        p++;
    }
    return -1;
}

char* tokens_get(struct tokens* ts, int index) {
    assert((ts!=NULL));

    return index<ts->count ? ts->tokens[index] : NULL;
}

void tokens_destroy(struct tokens* ts) {
    assert((ts!=NULL));

    free(ts->command);
    free(ts->tokens);
    free(ts);
}

