#include<stdio.h>
#include<stdlib.h>
#include<netdb.h>
#include<arpa/inet.h>

void print_ip(struct in_addr* p) {
    unsigned long addr = p->s_addr;
    unsigned char a = *((char*)(&addr));
    unsigned char b = *((char*)(&addr)+1);
    unsigned char c = *((char*)(&addr)+2);
    unsigned char d = *((char*)(&addr)+3);
    printf("%d.%d.%d.%d\n", a,b,c,d);
}

int main(int argc, char* argv[]) {
    if(argc!=2) {
        printf("Usage: getIpByHostname <hostname>\n");
        exit(1);
    }

    char* buffer = argv[1];
    printf("%s\n", buffer);
    struct hostent* ent = gethostbyname(buffer);
    struct in_addr** p = (struct in_addr**) ent->h_addr_list;

    while(*p) {
        print_ip(*p);
        printf("%s\n", inet_ntoa(**p));
        p++;
    }
}
