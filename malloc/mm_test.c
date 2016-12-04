#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
}

int main() {
    load_alloc_functions();

    int *data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);

    data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data = mm_realloc(data, 4*sizeof(int));
    memset(data, 0, sizeof(int)*4);
    int i = 0;
    for(; i<4; i++) data[i] = i;
    for(i=0; i<4; i++) printf("%d\n", data[i]);
    mm_free(data);
    mm_free(data);

    printf("malloc test successful!\n");
    return 0;
}
