#ifndef LIBLIST_H
#define LIBLIST_H

struct fd_pair {
    int client_fd;
    int server_fd;
    struct fd_pair *next;
};

void insert(struct fd_pair* head, struct fd_pair* p);
void delete(struct fd_pair* head, int fd);
int find(struct fd_pair* head, int fd);

#endif
