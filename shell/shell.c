#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "builtin.h"
#include "tokenizer.h"

#define COMMAND_MAX_LEN 4096
char line[COMMAND_MAX_LEN];

void dispatch() {
    getcwd(line, COMMAND_MAX_LEN);
    printf("[shell %s]$ ", line);
    fgets(line, COMMAND_MAX_LEN, stdin);
    line[strlen(line)-1] = '\0';
    struct tokens *ts = tokens_create(line);

    if(tokens_get(ts, 0) == NULL) return;

    // for builtin commands
    if(try_execute_builtin(ts->tokens)) return;

    // for external commands
    char *exe = get_full_path(tokens_get(ts, 0));
    if(exe == NULL) {
        printf("Command Not Found!\n");
        return;
    }

    int is_background = 0;
    if(strcmp(tokens_get(ts, ts->count-1), "&") == 0) {
        is_background = 1;
        ts->tokens[ts->count-1] = NULL;
    }

    pid_t pid = fork();
    if (pid < 0) exit(3);
    if (pid > 0) {
        if (is_background) add_background_proc(pid, ts->command);
        else {
            /* also do this in parent process  */
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, getpgid(pid));

            int status;
            wait(&status);

            /* get terminal control from the shell that started custom shell */
            if(tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
                perror("tcsetpgrp");
                exit(1);
            }
        } 

        tokens_destroy(ts);

    } else if(pid == 0) {

        /* set default reaction to signal in child process */
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        int fd, index;
        if(is_background) {
            fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO);
        }

        char **argv = ts->tokens;
        if((index = tokens_lookup("<", ts)) != -1) {
            if(index+1 >= ts->count) goto invalid_format;
            fd = open(argv[index+1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            argv[index] = NULL;
        } 

        if((index = tokens_lookup(">", ts)) != -1) {
            if(index+1 >= ts->count) goto invalid_format;
            fd = open(argv[index+1], O_WRONLY|O_CREAT|O_TRUNC);
            dup2(fd, STDOUT_FILENO);
            argv[index] = NULL;
        }

        execv(exe, argv);
        printf("Something's wrong!\n");
        exit(1);
    }
    return;

invalid_format:
    printf("Invalid format\n");
    exit(2);
}

int main(int argc, char *argv[])
{
    /* ignore all signals in shell */
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
