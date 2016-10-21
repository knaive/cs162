#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define CMD_MAX_LEN 128
#define BUF_MAX_LEN 128
#define PROC_MAX 128

char input[CMD_MAX_LEN];
char buffer[BUF_MAX_LEN];
char* argv[BUF_MAX_LEN];

struct background_proc {
    pid_t pid;
    char *argv;
} bg_proc[PROC_MAX];
int most_recent = -1;
pid_t current_proc;

int prompt() {
    int len = 0;
    printf("$ ");
    fgets(input, CMD_MAX_LEN, stdin);
    len = strlen(input)-1;
    input[len] = '\0';
}

void handler(int signum) {
    kill(current_proc, signum);
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

void wait_all_bgproc() {
    int status;
    while(most_recent) {
        waitpid(bg_proc[most_recent], &status, 0);
        printf("[JOB %d]\t%d\tDone\t %s", most_recent, bg_proc[most_recent].pid, bg_proc[most_recent].argv);
        bg_proc[most_recent].pid = 0;
        free(bg_proc[most_recent].argv);
        bg_proc[most_recent].argv = NULL;
        most_recent--;
    }
}

void dispatch() {
    prompt();
    char *str = (char *)malloc(strlen(input)+1);
    strcpy(str, input);
    int len = split(input, ' ', argv);
    char* exe = argv[0];
    if(exe == NULL) return;
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
    } else if (strcmp(exe, "wait") == 0) {
        wait_all_bgproc();
    } else {
        exe = get_full_path(exe);
        if(exe == NULL) goto command_not_found;
        pid_t pid = fork();
        current_proc = pid;
        int status, is_bg = 0;
        if(strcmp(argv[len-1], "&") == 0) {
            is_bg = 1;
            argv[len-1] = NULL;
        }
        if(pid > 0) {
            setpgid(pid, pid);
            if(!is_bg) wait(&status);
            else {
                most_recent++;
                bg_proc[most_recent].pid = pid;
                bg_proc[most_recent].argv = str;
                printf("[JOB %d]: %d\n", most_recent+1, pid);
            }
        } else if(pid == 0) {
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
    signal(SIGINT, handler);
    while(1) {
        dispatch();
    }
}
