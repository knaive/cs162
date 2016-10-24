#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
    int pid, status;
    int newfd;/* new file descriptor */

    if (argc != 2) {
        fprintf(stderr, "usage: %s output_file\n", argv[0]);
        exit(1);
    }
    if ((newfd = open(argv[1], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
        perror(argv[1]);/* open failed */
        exit(1);
    }
    printf("This goes to the standard output\n");
    printf("Now the standard output will go to \"%s\"\n", argv[1]);

    /* this new file will become the standard output */
    /* standard output is file descriptor 1, so we use dup2 to */
    /* to copy the new file descriptor onto file descriptor 1 */
    /* dup2 will close the current standard output */

    dup2(newfd, 1); 

    printf("This goes to the standard output too.\n");
    exit(0);
}
