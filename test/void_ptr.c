#include <stdio.h>

int main() {
    int a = 1;
    void* p = &a;
    printf("%x\n", p);
    p++;
    void* q = p;
    printf("%x\n", q);
    printf("%x\n", q+1);
}
