#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CMD_MAX_LEN 128

char cmd[CMD_MAX_LEN];

void prompt(char *cmd) {
    printf("$ ");
    scanf("%s", cmd);
}

void dispatch() {
    prompt(cmd);
    int len = strlen(cmd);
    if (strcmp(cmd, "exit") == 0) {
        exit(0);
    } else if (strcmp(cmd, "?") == 0) {
        printf("help...\n");
    }
}

int main(int argc, char *argv[])
{
    while(1) {
        dispatch();
    }
}

