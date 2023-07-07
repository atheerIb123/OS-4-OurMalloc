#include <unistd.h>
#define MAXSIZE 100000000

void* smalloc(size_t size)
{
    if (size > MAXSIZE)
        return NULL;

    if (size == 0)
        return NULL;

    void* pb = sbrk(size);

    if (pb == (void*)(-1))
        return NULL;

    return pb;
}