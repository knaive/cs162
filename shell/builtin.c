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

char buffer[BUF_MAX_LEN];

struct background_proc {
    pid_t pid;
    char *argv;
} bg_proc[PROC_MAX];
int most_recent = -1;
static void wait_all_bgproc(char *argv[]) {
    int status;
    while(most_recent) {
        waitpid(bg_proc[most_recent].pid, &status, 0);
        printf("[JOB %d]\t%d\tDone\t %s\n", most_recent, bg_proc[most_recent].pid, bg_proc[most_recent].argv);
        bg_proc[most_recent].pid = 0;
        free(bg_proc[most_recent].argv);
        bg_proc[most_recent].argv = NULL;
        most_recent--;
    }
}

void add_background_proc(pid_t pid, char *argv) {
    most_recent++;
    bg_proc[most_recent].pid = pid;
    int n = strlen(argv)+1;
    bg_proc[most_recent].argv = (char *)malloc(sizeof(char)*n);
    strcpy(bg_proc[most_recent].argv, argv);
    printf("[JOB %d]: %d\n", most_recent+1, pid);
}

static void help(char *argv[]) {
    printf("---- Help ----\n");
    printf("\t exit -- quit the shell\n");
    printf("\t pwd  -- print current working directory\n");
    printf("\t cd   -- change directory\n");
    printf("\t ?    -- show this help info\n");
}

static void pwd(char *argv[]) {
    getcwd(buffer, BUF_MAX_LEN);
    printf("%s\n", buffer);
}

static void cd(char *argv[]) {
    int ret = chdir(argv[1]);
    if(ret) perror("cd");
}

static void self_exit(char *argv[]) {
    exit(0);
}

struct cmd_map {
    char *exe;
    void (*func)(char *[]);
} builtin_table[] = {
    {"exit", self_exit},
    {"?", help},
    {"pwd", pwd},
    {"cd", cd},
    {"wait", wait_all_bgproc},
    {NULL, NULL}
};

int try_execute_builtin(char **argv) {
    struct cmd_map *p = builtin_table;
    while(p->exe != NULL) {
        if(strcmp(p->exe, argv[0])==0) {
            p->func(argv);
            return 1;
        }
        p++;
    }
    return 0;
}
