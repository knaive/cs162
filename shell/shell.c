#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "builtin.h"


char input[CMD_MAX_LEN];
char* argv[BUF_MAX_LEN];

pid_t foreground_pid;

int prompt() {
    int len = 0;
    printf("$ ");
    fgets(input, CMD_MAX_LEN, stdin);
    len = strlen(input)-1;
    input[len] = '\0';
}

int split(char *exe, char separator, char* tokens[]) {
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

int search(char* target, char* argv[]) {
    char** p = argv;
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
    char **argv = (char **) malloc(sizeof(char *)*count);
    strcpy(env_copy, env);
    int ret = split(env_copy, ':', argv);
    char *fullpath = (char *) malloc(len+cmd_len+2);
    while(count--) {
        *fullpath = '\0';
        strcat(fullpath, *argv);
        int size = strlen(fullpath);
        if(fullpath[size-1]!='/') {
            fullpath[size] = '/';
            fullpath[size+1] = '\0';
        }
        strcat(fullpath, cmd);
        if(access(fullpath, F_OK) != -1) return fullpath;
        argv++;
    }
    return NULL;
}

void dispatch() {
    prompt();
    char *str = (char *)malloc(strlen(input)+1);
    strcpy(str, input);
    int len = split(input, ' ', argv);
    char* exe = argv[0];
    if(exe == NULL) return;

    // for builtin commands
    try_execute_builtin(argv);

    // for external commands
    exe = get_full_path(exe);
    if(exe == NULL) goto command_not_found;
    int status, is_bg = 0;
    if(strcmp(argv[len-1], "&") == 0) {
        is_bg = 1;
        argv[len-1] = NULL;
    }

    pid_t pid = fork();
    if(pid > 0) {
        foreground_pid = pid;
        if(!is_bg){

            /* also do this in parent process  */
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, getpgid(pid));

            wait(&status);

            /* get terminal control from the shell that started custom shell */
            if(tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
                perror("tcsetpgrp");
                exit(1);
            }
        } 
        else {
            add_background_proc(pid, str);
        }
    } else if(pid == 0) {
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        if(setpgid(0, 0) < 0) {
            perror("setpgid");
            exit(1);
        }

        /* transfer controlling terminal */
        if(tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
            perror("tcsetpgrp");
            exit(1);
        }

        int fd, index;
        if(is_bg) {
            fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO);
        }
        if((index = search("<", argv)) != -1) {
            if(index+1 >= len) goto invalid_format;
            fd = open(argv[index+1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            argv[index] = NULL;
        } else if((index = search(">", argv)) != -1) {
            if(index+1 >= len) goto invalid_format;
            fd = open(argv[index+1], O_WRONLY|O_CREAT|O_TRUNC);
            dup2(fd, STDOUT_FILENO);
            argv[index] = NULL;
        }
        execv(exe, argv);
        printf("Something's wrong!\n");
        exit(0);
    } else {
        printf("Error\n");
    }
    return;

invalid_format:
    printf("Invalid format\n");
    exit(0);
command_not_found:
    printf("Command not found\n");
    return;
}

int main(int argc, char *argv[])
{
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    /* make custom shell its process group leader */
    if(setpgid(getpid(), getpid())<0) {
        perror("setpgid");
        exit(1);
    }

    /* get terminal control from the shell that started custom shell */
    if(tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
        perror("tcsetpgrp");
        exit(1);
    }

    while(1) {
        dispatch();
    }
}
