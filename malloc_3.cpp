#include <unistd.h>
#include <iostream>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAXSIZE 100000000
#define ALLOC_SIZE 32*128*1024
#define MAX_ORDER 10
#define LARGE_BLOCK 128 * 1024

int global_cookie = rand();

struct MallocMetadata {
    int cookie;
    size_t size;
    bool is_free;
    int order;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata* nextInHist;
    MallocMetadata* prevInHist;
}; typedef struct MallocMetadata MMData;

size_t alignSize(size_t value, size_t alignment);


class MMDataList
{
private:
    bool unassigned;
    MMData* head;
public:
    MMDataList() : head(NULL), unassigned(true){}
    MMData* removeHead()
    {
        MMData* temp = head;
        head = head->nextInHist;

        if (head != NULL)
        {
            head->prevInHist = NULL;
        }

        temp->nextInHist = NULL;
        return temp;
    }

    void addNode(MMData* newNode)
    {
        MMData* current = head;
        MMData* lastNode = head;

        if(head == NULL)
        {
            head = newNode;
            return;
        }

        while(current != NULL)
        {
            if(current > newNode)
            {
                if(current == head)
                {
                    head = newNode;
                    current->prevInHist = newNode;
                    head->nextInHist = current;
                }
                else
                {
                    current->prevInHist->nextInHist = newNode;
                    newNode->nextInHist = current;
                    newNode->prevInHist = current->prevInHist;
                }
                return;
            }
            lastNode = current;
            current = current->nextInHist;
        }
        lastNode->nextInHist = newNode;
        newNode->prevInHist = lastNode;
    }

    void removeBlockFromHist(MMData* to_delete)
    {
        MMData* current = this->head;
        MMData* prev = NULL;

        while(current != NULL)
        {
            if(current->cookie != global_cookie)
            {
                exit(0xdeadbeef);
            }

            if(current == to_delete)
            {
                if(prev == NULL)
                {
                    removeHead();
                }
                else
                {
                    prev->nextInHist = current->nextInHist;

                    if (current->nextInHist)
                    {
                        current->nextInHist->prevInHist = prev;
                    }
                }
            }

            prev = current;
            current = current->nextInHist;
        }
    }

    void initMemory()
    {
        if (this->unassigned)
        {
            void* pb = sbrk(0);
            size_t alignedSize = alignSize((size_t)(pb), ALLOC_SIZE);
            void* alignedPb = sbrk(alignedSize - (size_t)pb);
            alignedPb = sbrk(32*128*1024);
            MMData* previous = NULL;
            for(int i = 0; i < 32; i++)
            {
                MMData* newNodeMM = (MMData*) ((i * 128 * 1024) + (char*)alignedPb);
                *(MMData*)newNodeMM = (MMData){global_cookie, 128*1024 - sizeof(MMData), true, MAX_ORDER - 1, NULL, previous, NULL, previous};

                if( i == 0 )
                {
                    this->head = newNodeMM;
                    previous = head;
                }
                else
                {
                    previous->next = newNodeMM;
                    previous->nextInHist = newNodeMM;
                    previous = newNodeMM;
                }
            }
        }
    }

    size_t getNumOfFreeBlocksHist();
    size_t getNumOfFreeBytes();
    size_t getNumOfAllocations();
    size_t getNumberOfAllocatedBytes();
};

size_t MMDataList::getNumOfFreeBlocksHist()
{
    size_t amount = 0;
    MMData* current = this->head;

    while(current != NULL)
    {
        if(current->is_free)
        {
            amount++;
        }
        current = current->nextInHist;
    }

    return amount;
}

class MMDataDS
{
private:
    MMDataList memoryBlocks;
    MMDataList hist[MAX_ORDER];
    int blocksCountHist[MAX_ORDER] = {0};
public:
    MMDataDS();
    MMDataList& operator[](int index);
    MMData* findOptimalBlock(size_t size);
    void split(int index, int numOfSplits);
    MMData* mmapAllocation(size_t size);
    void freeBlock(void* ptr);
};

MMDataDS::MMDataDS() : memoryBlocks()
{
    memoryBlocks.initMemory();
    hist[MAX_ORDER - 1] = memoryBlocks;
    blocksCountHist[MAX_ORDER - 1] = 32;
}


MMDataList& MMDataDS::operator[](int index)
{
    if(index < MAX_ORDER)
    {
        return hist[index];
    }
}

int findIndex(size_t size)
{
    size_t comparisonSize = 128;

    for (int i = 0; i < MAX_ORDER; i++)
    {
        if (size <= comparisonSize - sizeof(MMData))
        {
            return i;
        }
        comparisonSize *= 2;
    }

    if (size <= comparisonSize - sizeof(MMData))
    {
        return MAX_ORDER - 1;
    }

    return -1;
}

MMData* getBuddyBlock(MMData* current)
{
    uintptr_t intptr1 = reinterpret_cast<uintptr_t>(current);
    uintptr_t xorResult = intptr1 ^ (uintptr_t)(pow(2, current->order) * 128);
    MMData* buddy = reinterpret_cast<MMData*>(xorResult);

    return buddy;
}

void MMDataDS::split(int index, int numOfSplits)
{
    MMData* current = hist[index].removeHead();
    MMData* buddy;

    for (int i = 0; i < numOfSplits; i++)
    {
        current->order -= 1;
        buddy = getBuddyBlock(current);
        size_t power = pow(2, current->order - 1);
        *(MMData*) buddy = (MMData){global_cookie, 128 * power - sizeof(buddy), true, current->order, NULL, NULL, NULL, NULL};
        buddy->next = current->next;
        if (current->next != NULL)
        {
            current->next->prev = buddy;
            current->next = buddy;
        }
        buddy->prev = current;
        hist[current->order].addNode(buddy);
    }

    hist[current->order].addNode(current);
}

MMData* MMDataDS::findOptimalBlock(size_t size)
{
    int index = findIndex(size);
    if (hist[index].getNumOfFreeBlocksHist() > 0)
    {
        MMData* current = hist[index].removeHead();
        current->is_free = false;
        current->size = size;
        return current;
    }
    else
    {
        for(int i = index+1; i < MAX_ORDER; i++)
        {
            if (hist[i].getNumOfFreeBlocksHist() > 0)
            {
                split(i, i - index);
                break;
            }
        }
    }

    MMData* returnedAddress = hist[index].removeHead();
    returnedAddress->size = size;

    returnedAddress->is_free = false;
    return returnedAddress;
}

MMData* MMDataDS::mmapAllocation(size_t size)
{
    void* allocated_block = mmap(NULL, sizeof(MMData) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (allocated_block == MAP_FAILED)
    {
        return NULL;
    }

    MMData* allocatedMMap = (MMData*)allocated_block;
    allocatedMMap->is_free = false;
    allocatedMMap->cookie = global_cookie;
    allocatedMMap->size = size;

    return allocatedMMap;
}

void MMDataDS::freeBlock(void *ptr)
{
    MMData* to_remove = (MMData*)((char*)ptr - sizeof(MMData));

    if(to_remove->is_free)
    {
        return;
    }

    to_remove->is_free = true;

    if (to_remove->size > LARGE_BLOCK - sizeof(MMData))
    {
        munmap(to_remove, sizeof(MMData) + to_remove->size);
    }
    else
    {
        MMData* buddy = getBuddyBlock(to_remove);

        if (buddy->is_free == false)
        {
            hist[to_remove->order].addNode(to_remove);
            return;
        }

        if (to_remove > buddy)
        {
            MMData* temp = buddy;
            buddy = to_remove;
            to_remove = temp;
        }

        while (buddy->is_free)
        {
            hist[to_remove->order].removeBlockFromHist(buddy);
            hist[to_remove->order].removeBlockFromHist(to_remove);

            to_remove->next = buddy->next;

            if (buddy->next)
            {
                buddy->next->prev = to_remove;
            }


            hist[to_remove->order + 1].addNode(to_remove);

            to_remove->order += 1;
            buddy = getBuddyBlock(to_remove);
        }
    }
}

size_t alignSize(size_t value, size_t alignment)
{
    size_t remainder = value % alignment;
    size_t padding;

    if(remainder == 0)
    {
        padding = 0;
    }
    else
    {
        padding = alignment - remainder;
    }

    return value + padding;
}

MMDataDS buddyAllocator = MMDataDS();

void* smalloc(size_t size)
{
    if (size > MAXSIZE)
        return NULL;

    if (size == 0)
        return NULL;

    void* block;

    if(size > LARGE_BLOCK - sizeof(MMData))
    {
        block = (void*)((char*)buddyAllocator.mmapAllocation(size) + (sizeof(MMData)));
    }
    else
    {
        block = (void*)((char*)buddyAllocator.findOptimalBlock(size) + (sizeof(MMData)));
    }

    return block;
}

void sfree(void* p)
{
    if (p == NULL)
    {
        return;
    }

    buddyAllocator.freeBlock(p);
}

void* scalloc(size_t num, size_t size)
{
    size_t total_bytes = num * size;

    if (num * size > MAXSIZE || num * size == 0)
    {
        return NULL;
    }

    void* allocatedBlock = smalloc(total_bytes);

    return memset(allocatedBlock, 0, total_bytes);
}



int main()
{
    int* arr = (int*)smalloc(sizeof(int) * 4);
    int* arr2 = (int*)smalloc(sizeof(int) * 4);

    size_t y = 128* 1024;

    int* arr3 = (int*)smalloc(y);

    sfree(arr);
    sfree(arr2);
    sfree(arr3);

    return 0;
}