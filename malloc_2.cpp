#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <string.h>


#define MAXSIZE 100000000
int ALLOCATEDBYTES = 0;


struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
}; typedef struct MallocMetadata MMData;

class MMDataList
{
private:
    size_t numberOfNodes;
    MMData* head;
public:
    MMDataList() : numberOfNodes(0), head(NULL){}

    void* allocateBlock(size_t size);
    void freeMemBlock(void* ptr);
    size_t getNumOfFreeBlocks();
    size_t getNumFreeBytes();
    size_t getNumOfAllocations();
    size_t getNumberOfAllocatedBytes();
};


void* MMDataList::allocateBlock(size_t size)
{
    if(this->head == NULL)
    {
        size_t incrementation = size + sizeof(MMData);
        void* allocated = sbrk(incrementation);

        if (allocated == (void*)(-1))
        {
            return NULL;
        }


        MMData * allocatedMM = (MMData *) allocated;

        allocatedMM->size = size;
        allocatedMM->is_free = false;
        ALLOCATEDBYTES += size;
        allocatedMM->next = NULL;
        allocatedMM->prev = NULL;

        this->head = allocatedMM;
        this->numberOfNodes++;

        return allocated;
    }
    else
    {
        MMData* current = this->head;
        MMData* previous = NULL;

        while (current != NULL)
        {
            if(current->is_free && current->size >= size)
            {
                current->is_free = false;
                current->size = size;
                return current;
            }
            previous = current;
            current = current->next;
        }


        if(current == NULL)
        {
            void* newNode = sbrk(size + sizeof(MMData));

            if(newNode == (void*)(-1))
            {
                return NULL;
            }

            ALLOCATEDBYTES += size;
            MMData* newNodeMM = (MMData*) newNode;
            *(MMData*)newNodeMM = {size, false, NULL, previous};

            previous->next = newNodeMM;
            this->numberOfNodes++;
            return newNode;
        }
    }

    return nullptr;
}
void MMDataList::freeMemBlock(void *ptr)
{
    MMData* metadata = (MMData*)((char*)ptr - sizeof(MMData));
    metadata->is_free = true;
}

size_t MMDataList::getNumOfFreeBlocks()
{
    size_t amount = 0;
    MMData* current = this->head;

    while(current != NULL)
    {
        if(current->is_free)
            amount++;
        current = current->next;
    }

    return amount;
}

size_t MMDataList::getNumFreeBytes()
{
    size_t amount = 0;
    MMData* current = this->head;

    while(current != NULL)
    {
        if(current->is_free)
        {
            amount += current->size;
        }
        current = current->next;
    }

    return amount;
}

size_t MMDataList::getNumOfAllocations()
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

size_t MMDataList::getNumberOfAllocatedBytes()
{
    size_t amount = 0;
    MMData* current = this->head;

    while(current != NULL)
    {
        if(current->is_free)
        {
            amount += current->size;
        }
        current = current->next;
    }

    return amount;
}

MMDataList memoryBlocks = MMDataList();

void* smalloc(size_t size)
{
    if (size > MAXSIZE)
        return NULL;

    if (size == 0)
        return NULL;

    return (void*)((char*)memoryBlocks.allocateBlock(size) + sizeof(MMData));
}

void* scalloc(size_t num, size_t size)
{
    size_t totalBytes = num * size;

    if(totalBytes == 0 || totalBytes > MAXSIZE)
    {
        return NULL;
    }

    void* allocatedBlock = smalloc(totalBytes);

    return memset(allocatedBlock, 0, totalBytes);
}

void sfree(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    memoryBlocks.freeMemBlock(ptr);
}

void* srealloc(void* oldp, size_t size)
{
    if (size == 0 || size > MAXSIZE)
    {
        return NULL;
    }

    if (oldp == NULL)
    {
        return smalloc(size);
    }

    MMData* oldpMetaData = (MMData*)((char*)oldp - sizeof(MMData));

    if(oldpMetaData->size >= size)
    {
        return oldp;
    }

    void* newAllocated = smalloc(size);

    memcpy(newAllocated, oldp, oldpMetaData->size);
    sfree(oldp);

    return newAllocated;
}

size_t _num_free_blocks()
{
    return memoryBlocks.getNumOfFreeBlocks();
}

size_t _num_free_bytes()
{
    return memoryBlocks.getNumFreeBytes();
}

size_t _num_allocated_blocks()
{
    return memoryBlocks.getNumOfAllocations();
}

size_t _num_meta_data_bytes()
{
    return _num_allocated_blocks() * sizeof(MMData);
}

size_t _size_meta_data()
{
    size_t mmdata_size = sizeof(MallocMetadata);
    return mmdata_size;
}
size_t _num_allocated_bytes()
{
    return ALLOCATEDBYTES;
}