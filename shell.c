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
            int fd, index;
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
