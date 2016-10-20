#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CMD_MAX_LEN 128
#define BUF_MAX_LEN 128

char input[CMD_MAX_LEN];
char buffer[BUF_MAX_LEN];
char* argv[BUF_MAX_LEN];

void prompt() {
    printf("$ ");
    fgets(input, CMD_MAX_LEN, stdin);
    input[strlen(input)-1] = '\0';
}

int split(char *exe, char separator, char*argv[]) {
    int first, count = 0;
    while(*exe != 0) {
        if(*exe == separator) {
            *exe = 0;
            first = 0;
            exe++;
            continue;
        }
        first++;
        if(first == 1) {
            argv[count] = exe;
            count++;
        }
        exe++;
    }
    return count;
}

//of course we can use execvp, but the course prohibit it
char* get_full_path(char *cmd) {
    if(access(cmd, F_OK) != -1) return cmd;

    const char* env = getenv("PATH");
    int len = 0, count = 1, cmd_len = strlen(cmd);
    char *p = env;
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
    printf("ret: %d, count: %d \n", ret, count);

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
    int len = split(input, ' ', argv);
    char* exe = argv[0];
    if (strcmp(exe, "exit") == 0) {
        exit(0);
    } else if (strcmp(exe, "?") == 0) {
        printf("---- Help ----\n");
        printf("\t exit -- quit the shell\n");
        printf("\t pwd  -- print current working directory\n");
        printf("\t cd   -- change directory\n");
        printf("\t ?    -- show this help info\n");
    } else if (strcmp(exe, "pwd") == 0) {
        getcwd(buffer, BUF_MAX_LEN);
        printf("%s\n", buffer);
    } else if (strcmp(exe, "cd") == 0) {
        chdir(argv[1]);
    } else {
        exe = get_full_path(exe);
        if(exe == NULL) goto command_not_found;
        pid_t pid = fork();
        int status;
        if(pid > 0) {
            wait(&status);
        } else if(pid == 0) {
            int fd;
            if(len>1 && strcmp(argv[1], "<") == 0) {
                if(len<=2) goto invalid_format;
                fd = open(argv[2], O_RDONLY|O_CREAT|O_TRUNC);
                dup2(fd, STDIN_FILENO);
            } else if(len>2&&strcmp(argv[2], ">") == 0) {
                if(len<=3) goto invalid_format;
                fd = open(argv[3], O_WRONLY);
                dup2(fd, STDOUT_FILENO);
            }
            execv(exe, argv);
            exit(0);
        } else {
            printf("Error\n");
        }
    }
    return;

invalid_format:
    printf("Invalid format\n");
    return;
command_not_found:
    printf("Command not found\n");
}

int main(int argc, char *argv[])
{
    while(1) {
        dispatch();
    }
}
