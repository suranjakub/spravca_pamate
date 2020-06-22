#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct block_header {
    int size;
    struct block_header* prev;
    struct block_header* next;
} BL_HEAD;

typedef struct memory_header {
    int size;
    BL_HEAD* linked_list;
} MEM_HEAD;

//global pointer to memory header
MEM_HEAD* memory_head_ptr;

void memory_init(void* ptr, int size) {
    //set address of ptr to global variable
    memory_head_ptr = (MEM_HEAD*)ptr;
    //declare first block head
    BL_HEAD* block_head_ptr = (BL_HEAD*)((void*)memory_head_ptr + sizeof(MEM_HEAD));

    //set values of memory header
    memory_head_ptr->size = size - sizeof(MEM_HEAD);
    memory_head_ptr->linked_list = block_head_ptr;

    //set real size, which can be allocated in first block
    block_head_ptr->size = (memory_head_ptr->size) - sizeof(BL_HEAD);
    block_head_ptr->prev = block_head_ptr->next = NULL;

    //initialize free space with '0"
    int freeMem = memory_head_ptr->linked_list->size;
    for (int i = 0; i < freeMem; ++i)
        *((char*)((void*)memory_head_ptr + sizeof(MEM_HEAD) + sizeof(BL_HEAD) + i)) = '0';

    printf("Memory initialized, first free block %d, address %p\n", block_head_ptr->size, block_head_ptr);
}

void* memory_alloc(int size) {
    if(memory_head_ptr->linked_list == NULL) {
        printf("Memory has no free space\n");
        return NULL;
    }

    //temp pointer to traverse linked list
    BL_HEAD* temp = memory_head_ptr->linked_list;
    //fittable block
    BL_HEAD* fittable = NULL;

    //find first block with enough space
    if(temp->size >= size)
        fittable = temp;
    while(temp->size < size && temp->next != NULL) {
        temp = temp->next;
        fittable = temp;
    }

    //if cannot find any suitable block return NULL
    if(fittable == NULL || size <= 0) {
        printf("Memory with size %d cannot be allocated\n", size);
        return NULL;
    }

    //if worth dividing block to two
    int remainingSize = fittable->size - size - sizeof(BL_HEAD);
    //if so, divide block, remove from list and return part of mem
    if(remainingSize > 1) {
        int freeSpace = fittable->size - size - sizeof(BL_HEAD);
        fittable->size = size * (-1);
        //new part of divided block
        BL_HEAD* new_bl_head = (BL_HEAD*)((void*)fittable + sizeof(BL_HEAD) + size);
        new_bl_head->size = freeSpace;

        //set neighbor on the left to link at new head
        if(memory_head_ptr->linked_list == fittable) {
            memory_head_ptr->linked_list = new_bl_head;
            new_bl_head->prev = NULL;
        }
        else if(fittable->prev != NULL)
            (fittable->prev)->next = new_bl_head;

        //set neighbor at right to link at new head
        if(fittable->next != NULL)
            (fittable->next)->prev = new_bl_head;
        else
            new_bl_head->next = NULL;

        //copy links from old block to new
        new_bl_head->prev = fittable->prev;
        new_bl_head->next = fittable->next;

        fittable->next = fittable->prev = NULL;
    }
    //if we don't divide block, then just set neighbor links
    else {
        fittable->size *= (-1);
        if(memory_head_ptr->linked_list == fittable)
            memory_head_ptr->linked_list = fittable->next;
        else if(fittable->prev != NULL)
            fittable->prev->next = fittable->next;
        if(fittable->next != NULL)
            fittable->next->prev = fittable->prev;
        //fittable->next = fittable->prev = NULL;
    }

    printf("Block of size %d was allocated\n", size);
    return (void*)fittable+sizeof(BL_HEAD);
}

int not_in_linkedlist(BL_HEAD* block_head) {
    BL_HEAD* temp = memory_head_ptr->linked_list;

    if(block_head->size > 0)
        return 0;
    while(temp != NULL) {
        if(temp == block_head)
            return 0;
        temp = temp->next;
    }
    return 1;
}

int memory_check(void* ptr) {
    BL_HEAD* block_head = (void*)ptr - sizeof(BL_HEAD);
    void* lower_limit = (void*)memory_head_ptr + sizeof(MEM_HEAD);
    void* upper_limit = (void*)memory_head_ptr + memory_head_ptr->size + sizeof(MEM_HEAD);

    if((void*)ptr >= lower_limit && (void*)ptr < upper_limit && not_in_linkedlist(block_head)) {
        printf("Pointer %p is valid and not released yet\n", ptr);
        return 1;
    }
    else {
        printf("Pointer %p is NOT valid\n", ptr);
        return 0;
    }
}

void merge_block(BL_HEAD* block_to_free, BL_HEAD* temp) {
    //if block to free is on the left
    if(block_to_free < temp) {
        block_to_free->size += temp->size + sizeof(BL_HEAD);
        temp->size = 0;
        //copy links to new merged block
        block_to_free->next = temp->next;
        if(temp->prev != block_to_free)
            block_to_free->prev = temp->prev;
        //set links of temp neighbors to new merged block
        if(temp->prev != NULL)
            temp->prev->next = block_to_free;
        /*if(block_to_free->next == temp)
            temp->prev->next = NULL;*/
        if(temp->next != NULL)
            temp->next->prev = block_to_free;
    }
    //if block to free is on the right
    else {
        temp->size += block_to_free->size + sizeof(BL_HEAD);
        block_to_free->size = 0;
    }
}

int memory_free(void* valid_ptr) {
    //block_to_free reference
    BL_HEAD* block_to_free = (void*)valid_ptr - sizeof(BL_HEAD);
    int size_to_free = block_to_free->size;
    //to be sure, we check ptr if really valid
    if(!memory_check(valid_ptr)) {
        printf("Cannot free block %p with size %d\n", block_to_free, block_to_free->size);
        return 0;
    }

    //mark head of block as free
    if(block_to_free->size < 0) {
        block_to_free->size *= -1;
    }

    BL_HEAD* temp = memory_head_ptr->linked_list;
    //if linked list empty, insert block_to_free block
    if(memory_head_ptr->linked_list == NULL) {
        memory_head_ptr->linked_list = block_to_free;
        block_to_free->next = block_to_free->prev = NULL;
        printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
    }
    //if block_to_free is before first in linked list
    else if (temp > block_to_free) {
        //if they are neighbors
        if((void*)block_to_free + sizeof(BL_HEAD) + block_to_free->size == temp) {
            merge_block(block_to_free, temp);
            memory_head_ptr->linked_list = block_to_free;
            printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
        }
        //if the aren't neighbors just set new links
        else {
            block_to_free->next = temp;
            block_to_free->prev = NULL;
            memory_head_ptr->linked_list = block_to_free;
            block_to_free->next->prev = block_to_free;
            printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
        }
    }
    //if block_to_free is in the middle or the end
    else {
        //move in list and depending on which case fails we continue
        while(temp->next != NULL && block_to_free > temp) {
            temp = temp->next;
        }
        //block_to_free is after linked list
        if(temp->next == NULL && block_to_free > temp) {
            //found left neighbor of block_to_free, merge them
            if((void*)temp + sizeof(BL_HEAD) + temp->size == block_to_free) {
                merge_block(block_to_free, temp);
                printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
            }
            //if they aren't neighbors
            else {
                temp->next = block_to_free;
                block_to_free->prev = temp;
                block_to_free->next = NULL;
                printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
            }
        }
        //if block_to_free is before some free block & after another free
        else if(block_to_free < temp) {
            int merged = 0, freeSpace = block_to_free->size;
            temp = temp->prev;
            //find if there is left neighbor of block_to_free and then merge them
            if((void*)temp + sizeof(BL_HEAD) + temp->size == block_to_free) {
                merge_block(block_to_free, temp);
                printf("Block %p with size %d was released\n", valid_ptr, abs(size_to_free));
                merged++;
            }
            //find if there is right neighbor of block_to_free
            temp = temp->next;
            if((void*)temp - freeSpace - sizeof(BL_HEAD) == block_to_free) {
                //if block_to_free was already merged with left neighbor
                if(merged) {
                    merge_block(temp->prev, temp);
                }
                //if block_to_free wasn't merged with left neighbor
                else {
                    merge_block(block_to_free, temp);
                    printf("Block %p with size %d was released\n", block_to_free, abs(size_to_free));
                }
            }
        }
    }
}

void show_Ll() {
    BL_HEAD* ptr = memory_head_ptr->linked_list;
    while(ptr != NULL) {
        printf("-> %d ", ptr->size);
        ptr = ptr->next;
    }
    if(ptr == NULL)
        printf("-> NULL");
    printf("\n");
}


int main() {
    int test_size = 118, block_size = 15;
    int i, n_of_allocated = 0, n_of_deallocated = 0;
    char region[test_size];
    memory_init(region, test_size);
    show_Ll();

    char* array[test_size];
    for(i = 0; (test_size - i*block_size) > block_size ; i++) {
        array[i] = memory_alloc(block_size);
        if(array[i]) {
            memset(array[i], 0, block_size);
            n_of_allocated++;
        }
    }

    double success_rate = ((double)n_of_allocated/(double)i) * 100.0;
    printf("\nNumber of allocated %d, ideal case: %d, success rate %.2lf%%\n", n_of_allocated, i, success_rate);

    for(i = n_of_allocated-1; i >= 0; i--) {
        if(array[i])
            if(memory_free(array[i]))
               n_of_deallocated++;
    }
    if(n_of_allocated == n_of_deallocated)
        printf("Deallocation was successful\n");

    return 0;
}
