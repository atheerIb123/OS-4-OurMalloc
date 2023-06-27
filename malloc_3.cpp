#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAXSIZE 100000000
#define ALLOC_SIZE 32*128*1024
#define MAX_ORDER 10

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
        return temp;
    }

    void addNode(MMData* newNode)
    {
        MMData* current = head;
        MMData* lastNode = head;
        while(current != NULL)
        {
            if(current > newNode)
            {
                if(current == head)
                {
                    MMData* temp = head;
                    head = current;
                    temp->prevInHist = current;
                    head->nextInHist = temp;
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

    void initMemory()
    {
        if(this->unassigned)
        {
            void* pb = sbrk(0);
            size_t alignedSize = alignSize((size_t)(pb), ALLOC_SIZE);
            void* alignedPb = sbrk(alignedSize - (size_t)pb);
            alignedPb = sbrk(ALLOC_SIZE);
            MMData* previous = NULL;
            for(int i = 0; i < 32; i++)
            {
                MMData* newNodeMM = (MMData*) ((i * 128 * 1024) + (int*)alignedPb);
                *(MMData*)newNodeMM = (MMData){global_cookie, 128*1024 - sizeof(newNodeMM), true, MAX_ORDER - 1, NULL, previous, NULL, previous};

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

    size_t getNumOfFreeBlocks();
    size_t getNumOfFreeBytes();
    size_t getNumOfAllocations();
    size_t getNumberOfAllocatedBytes();
};


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

    for(int i = 0; i < MAX_ORDER; i++)
    {
        if(size <= comparisonSize - sizeof(MMData))
        {
            return i;
        }
        comparisonSize *= 2;
    }

    return -1;
}


void MMDataDS::split(int index, int numOfSplits)
{
    //inside the loop
    MMData* current = hist[index].removeHead();
    for(int i = 0; i < numOfSplits; i++)
    {
        uintptr_t intptr1 = reinterpret_cast<uintptr_t>(current);
        uintptr_t xorResult = intptr1 ^ (uintptr_t)(pow(2, index) * 128);
        MMData* buddy = reinterpret_cast<MMData*>(xorResult);
        size_t power = pow(2, current->order - 1);
        *(MMData*) buddy = (MMData){global_cookie, 128 * power - sizeof(buddy), true, current->order - 1, NULL, NULL, NULL, NULL};
        current->order -= 1;
        buddy->next = current->next;
        if (current->next != NULL)
        {
            current->next->prev = buddy;
            current->next = buddy;
        }
        buddy->prev = current;
        hist[current->order].addNode(buddy);
    }


}

MMData* MMDataDS::findOptimalBlock(size_t size)
{
    int index = findIndex(size);
    if (hist[index].getNumOfFreeBlocks() > 0)
    {
        MMData* current = hist[index].removeHead();
        current->is_free = false;
        return current;
    }
    else
    {
        for(int i = index+1; i < MAX_ORDER; i++)
        {
            if (hist[i].getNumOfFreeBlocks() > 0)
            {


            }
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

void* smalloc(size_t size)
{

}
