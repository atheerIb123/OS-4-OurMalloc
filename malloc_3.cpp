#include <unistd.h>
#include <iostream>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAXSIZE 100000000
#define ALLOC_SIZE 32*128*1024
#define MAX_ORDER 11
#define LARGE_BLOCK 128 * 1024

int global_cookie = rand();

int findIndex(size_t size);
int mmapAllocs = 0;

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
size_t numOfBytes = 32*(128*1024 - sizeof(MMData));


class MMDataList
{
private:
    bool unassigned;
    MMData* head;
public:
    size_t getNumOfFreeBlocks();
    size_t getNumOfFreeBytes();
    size_t getNumOfAllocations();
    size_t getNumberOfAllocatedBytes();
    size_t getNumOfFreeBlocksHist();

    MMDataList() : unassigned(true), head(NULL) {}

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

    MMData* getNode(MMData* node)
    {
        MMData* current = head;
        while(current != NULL)
        {
            if(current == node)
            {
                return current;
            }
            current = current->nextInHist;
        }
        return NULL;
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
                exit(0xDEADBEEF);
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
                *(MMData*)newNodeMM = {global_cookie, 128*1024 - sizeof(MMData), true, MAX_ORDER - 1, NULL, previous, NULL, previous};

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

};

size_t MMDataList::getNumOfFreeBlocks()
{
    size_t cnt = 0;
    MMData* current = head;
    while(current != NULL)
    {
        if(current->cookie != global_cookie)
        {
            exit(0xDEADBEEF);
        }
        if(current->is_free)
            cnt++;
        current = current->next;
    }
    return cnt;
}

size_t MMDataList::getNumOfFreeBytes()
{
    size_t cnt = 0;
    MMData* current = head;

    while(current != NULL)
    {
        if (current->is_free)
            cnt += pow(2, current->order)*128 - sizeof(MMData);
        current = current->next;
    }

    return cnt;
}

size_t MMDataList::getNumOfAllocations()
{
    size_t cnt = 0;
    MMData* current = head;
    while(current != NULL)
    {
        cnt++;
        current = current->next;
    }

    return cnt;
}

size_t MMDataList::getNumberOfAllocatedBytes()
{
    MMData* current = this->head;
    size_t amount = 0;

    while(current != NULL)
    {
        amount++;
        current = current->next;
    }

    return amount;
}

size_t MMDataList::getNumOfFreeBlocksHist()
{
    size_t amount = 0;
    MMData* current = this->head;

    while(current != NULL)
    {
        if(current->cookie != global_cookie)
            exit(0xDEADBEAF);
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
    void* mergeBuddies(MMData* block, size_t sizeNeeded);
    MMData* mmapAllocation(size_t size);
    void freeBlock(void* ptr);

    size_t getNumOfFreeBlocks()
    {
        return memoryBlocks.getNumOfFreeBlocks();
    }

    size_t getNumOfFreeBytes()
    {
        return memoryBlocks.getNumOfFreeBytes();
    }

    size_t getAllocs()
    {
        return memoryBlocks.getNumOfAllocations();
    }

    MMData* getBuddyBlock(MMData* current);
};

MMDataDS::MMDataDS() : memoryBlocks()
{
    memoryBlocks.initMemory();
    hist[MAX_ORDER - 1] = memoryBlocks;
    blocksCountHist[MAX_ORDER - 1] = 32;
}

void* MMDataDS::mergeBuddies(MMData* block, size_t sizeNeeded)
{
    int countMerges = 0;
    int currentOrder = 0;

    MMData* buddy = this->getBuddyBlock(block);
    if(buddy < block)
        block = buddy;

    currentOrder = block->order;
    while(getBuddyBlock(block) != NULL)
    {
        if(block->cookie != global_cookie)
            exit(0xDEADBEAF);
        if(pow(++block->order, 2)*128 >= sizeNeeded)
            break;
        countMerges++;
    }


    if(getBuddyBlock(block) == NULL)
        return NULL;

    block->order = currentOrder;


    for(int i = 0; i < countMerges; i++)
    {
        hist[getBuddyBlock(block)->order].removeBlockFromHist(getBuddyBlock(block));

        if(buddy->prev)
        {
            buddy->prev->next = buddy->next;
        }
        if(buddy->next)
        {
            buddy->next->prev = buddy->prev;
        }
        block->order++;
    }

    return block;
}

MMDataList& MMDataDS::operator[](int index)
{
    if(index < MAX_ORDER)
    {
        return hist[index];
    }
    return hist[0];
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

MMData* MMDataDS::getBuddyBlock(MMData *current)
{
    uintptr_t intptr1 = reinterpret_cast<uintptr_t>(current);
    uintptr_t xorResult = intptr1 ^ (uintptr_t)(pow(2, current->order) * 128);
    MMData* buddy = reinterpret_cast<MMData*>(xorResult);

    if(!hist[findIndex(buddy->size)].getNode(buddy))
        return NULL;

    return buddy;
}

MMData* getBuddyBlockNoNULL(MMData *current)
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

    if(numOfSplits == 0)
    {
        numOfSplits++;
    }
    for (int i = 0; i < numOfSplits; i++)
    {
        if(current->cookie != global_cookie)
            exit(0xDEADBEAF);
        current->order -= 1;
        buddy = getBuddyBlockNoNULL(current);
        size_t power = pow(2, current->order);
        *(MMData*) buddy = {global_cookie, 128 * power - sizeof(buddy), true, current->order, NULL, NULL, NULL, NULL};

        buddy->next = current->next;
        if (current->next != NULL)
        {
            current->next->prev = buddy;
        }
        current->next = buddy;
        buddy->prev = current;


        hist[current->order].addNode(buddy);
    }



    hist[current->order].addNode(current);
}

MMData* MMDataDS::findOptimalBlock(size_t size)
{
    int index = findIndex(size);
    bool flag = false;

    if (hist[index].getNumOfFreeBlocksHist() > 0)
    {
        MMData* current = hist[index].removeHead();
        if(current->cookie != global_cookie)
            exit(0xDEADBEAF);
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
                flag = true;
                split(i, i - index);
                break;
            }
        }
    }

    if(flag == false)
    {
        return NULL;
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
        munmap((void*)((char*)to_remove - sizeof(MMData)), sizeof(MMData) + to_remove->size);
        numOfBytes -= to_remove->size;
        mmapAllocs--;
    }
    else
    {
        MMData* buddy = getBuddyBlockNoNULL(to_remove);

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

        while (buddy->is_free && buddy->order < 10)
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
            buddy = getBuddyBlockNoNULL(to_remove);
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
bool smallocFlag = false;

void* smalloc(size_t size)
{
    buddyAllocator.getNumOfFreeBlocks();
    smallocFlag = true;
    if (size > MAXSIZE)
        return NULL;

    if (size == 0)
        return NULL;

    void* block;

    if(size > LARGE_BLOCK - sizeof(MMData))
    {
        block = (void*)((char*)buddyAllocator.mmapAllocation(size) + (sizeof(MMData)));
        numOfBytes += size;
        mmapAllocs++;
    }
    else
    {
        void* block2 = buddyAllocator.findOptimalBlock(size);
        if(!block2)
        {
            return NULL;
        }

        block = (void*)((char*)block2 + (sizeof(MMData)));
    }

    return block;
}

void sfree(void* p)
{
    buddyAllocator.getNumOfFreeBlocks();

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

void* srealloc(void* oldp, size_t size)
{
    buddyAllocator.getNumOfFreeBlocks();

    if(size == 0 || size > MAXSIZE)
        return NULL;

    if(oldp == NULL)
        return (smalloc(size));

    MMData* oldpMD = (MMData*)((char*)oldp - sizeof(MMData));

    if(oldpMD->order == findIndex(size))
    {
        oldpMD->size = size;
        return oldp;
    }

    if(oldpMD->size + sizeof(MMData) > 128*1024) //mmapped block
    {
        void* newBlock = smalloc(size);
        MMData* newAllocated = (MMData*)((char*)newBlock - sizeof(MMData));
        memcpy(newAllocated, oldp, oldpMD->size);
        sfree(oldp);
        return newBlock;
    }

    //not mmapped block
    MMData* buddy = buddyAllocator.getBuddyBlock(oldpMD);


    if(!buddy || !buddyAllocator.mergeBuddies(oldpMD, size)) //oldP doesn't have buddies
    {
        sfree(oldp);
        void* newBlock_2 = smalloc(size);
        MMData* newAllocated_2 = (MMData*)((char*)newBlock_2 - sizeof(MMData));
        memcpy(newAllocated_2, oldp, oldpMD->size);
        return newAllocated_2;
    }

    return NULL;
}


size_t _num_free_blocks()
{
    if(smallocFlag == false)
    {
        return 0;
    }
    return buddyAllocator.getNumOfFreeBlocks();
}

size_t _num_free_bytes()
{
    if(smallocFlag == false)
    {
        return 0;
    }

    return buddyAllocator.getNumOfFreeBytes();
}

size_t _num_allocated_blocks()
{
    if(smallocFlag == false)
    {
        return 0;
    }

    return buddyAllocator.getAllocs() + mmapAllocs;
}

size_t _num_meta_data_bytes()
{
    return _num_allocated_blocks() * sizeof(MMData);
}

size_t _num_allocated_bytes()
{
    if(smallocFlag == false)
    {
        return 0;
    }

    return numOfBytes - (_num_allocated_blocks() - mmapAllocs - 32)*sizeof(MMData);
}

size_t _size_meta_data()
{
    return sizeof(MMData);
}
int main()
{
    void* ptr1 = smalloc(40);
    void* ptr2 = srealloc(ptr1, 128*pow(2,2) -64);

}