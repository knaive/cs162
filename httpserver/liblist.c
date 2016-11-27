#include "liblist.h"

void insert(struct fd_pair* head, struct fd_pair* p) {
    assert(head!=NULL);
    static struct fd_pair* tail = NULL;

    if(tail == NULL) {
        while(head->next!=NULL) head = head->next;
        tail = head;
    }
    tail->next = p;
    p->next = NULL;
}

void delete(struct fd_pair* head, int fd) {
    assert(head!=NULL);
    struct fd_pair* p = head->next;

    while(p!=NULL) {
        if(p->client_fd == fd || p->server_fd == fd) break;
        head = p;
        p = p->next;
    }
    if(p!=NULL) {
        head = p->next;
        free(p);
    }
}

int find(struct fd_pair* head, int fd) {
    if(head == NULL) return 0;

    while(head->next!=NULL) {
        if(head->client_fd == fd) return head->server_fd;
        if(head->server_fd == fd) return head->client_fd;
        head = head->next;
    }

    return 0;
}
