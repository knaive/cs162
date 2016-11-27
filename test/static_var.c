#include<stdio.h>

void foo(int a) {
    static int count = 0;
    count++;
    printf("%d\n", count);
}

int main() {
    foo(5);
    foo(5);
}
