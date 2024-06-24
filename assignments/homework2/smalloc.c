#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h> // mmap 헤더 추가
#include "smalloc.h"

smheader_ptr smlist = NULL; // 초기화 변경

#define HEADER_SIZE sizeof(smheader)
#define page_size getpagesize()

// smalloc 함수 개선
void *smalloc(size_t s)
{

    smheader_ptr itr = smlist;
    smheader_ptr prev = NULL;
    for (itr = smlist; itr != NULL; prev = itr, itr = itr->next)
    {
        if (itr->used == 0 && itr->size >= s)
        {
            // Found a free block with enough space
            // smalloc(1000) 처음할 때 동작 생각해보기->smalloc(1000) 두번째 할 때 왜 새로 페이지 할당?
            if (itr->size >= s + HEADER_SIZE)
            {
                // Split the block
                smheader_ptr new_block = (smheader_ptr)((char *)itr + HEADER_SIZE + s);
                new_block->size = itr->size - s - HEADER_SIZE;
                new_block->used = 0;
                new_block->next = itr->next; // 새로운 블럭의 next pointer를 현재의 pointer로 바꿈
                itr->size = s;
                itr->next = new_block;
            }
            itr->used = 1;
            return ((void *)itr) + sizeof(smheader);
        }
    }

    // No suitable block found, allocate new memory

    size_t alloc_size = (s + HEADER_SIZE + page_size - 1) / page_size * page_size; // Round up to the nearest multiple of page size
    smheader_ptr new_block = (smheader_ptr)mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_block == MAP_FAILED)
    {
        perror("Failed to mmap");
        exit(EXIT_FAILURE);
    }

    new_block->size = alloc_size - HEADER_SIZE;
    new_block->used = 0;
    new_block->next = NULL;

    // Append new_block to the end of the list
    if (prev != NULL)
        prev->next = new_block;
    else
        smlist = new_block; // Update smlist if it's the first block
    smalloc(s);
    return ((void *)new_block) + sizeof(smheader);
}

void *smalloc_mode(size_t s, smmode m)
{
    if (smlist == NULL)
    {
        return smalloc(s);
    }

    smheader *itr, *selected_block = NULL;

    // 메모리 할당 모드에 따라 다른 로직 수행
    switch (m)
    {
    case bestfit:
        // Best-fit 할당 모드의 구현
        {
            size_t min_remaining = SIZE_MAX; // 가용한 블록 중에서 가장 작은 남은 공간을 저장할 변수
            for (itr = smlist; itr != NULL; itr = itr->next)
            {
                if (!itr->used && itr->size >= s)
                {
                    size_t remaining_space = itr->size - s;
                    if (remaining_space < min_remaining)
                    {
                        selected_block = itr;
                        min_remaining = remaining_space;
                    }
                }
            }
        }
        break;

    case worstfit:
        // Worst-fit 할당 모드의 구현
        {
            size_t max_remaining = 0; // 가용한 블록 중에서 가장 큰 남은 공간을 저장할 변수
            for (itr = smlist; itr != NULL; itr = itr->next)
            {
                if (!itr->used && itr->size >= s)
                {
                    size_t remaining_space = itr->size - s;
                    if (remaining_space > max_remaining)
                    {
                        selected_block = itr;
                        max_remaining = remaining_space;
                    }
                }
            }
        }
        break;

    case firstfit:
        // First-fit 할당 모드의 구현
        for (itr = smlist; itr != NULL; itr = itr->next)
        {
            if (!itr->used && itr->size >= s)
            {
                selected_block = itr;
                break; // 첫 번째로 발견된 블록을 사용
            }
        }
        break;
    }

    // 선택된 블록이 있다면 할당하고 반환
    if (selected_block != NULL)
    {
        selected_block->used = 1;
        return ((void *)selected_block) + sizeof(smheader);
    }

    // 선택된 블록이 없는 경우 새로운 메모리 할당
    return smalloc(s);
}

void sfree(void *p)
{
    if (p == NULL)
    {
        fprintf(stderr, "Attempt to free a null pointer.\n");
        return;
    }
    smheader_ptr header = (smheader_ptr)p - 1;
    if (header->used == 0)
    {
        fprintf(stderr, "Double free detected.\n");
        return;
    }

    // 로깅을 추가하여 어떤 주소가 해제되는지 확인
    printf("Freeing memory at %p, size %zu\n", p, header->size);

    header->used = 0; // 메모리 해제 상태로 설정
}

// srealloc 함수 개선
void *srealloc(void *p, size_t s)
{
    if (p == NULL)
    {
        fprintf(stderr, "Attempt to reallocate a null pointer.\n");
        return NULL;
    }

    smheader_ptr header = (smheader_ptr)p - 1;
    size_t current_size = header->size;

    if (s <= current_size)
    {
        // 요청된 크기가 현재 크기보다 작거나 같으면
        // 메모리 블록을 재사용하고 남은 공간을 적절히 처리합니다.
        size_t remaining_size = current_size - s;
        if (remaining_size >= HEADER_SIZE)
        {
            // 재사용된 블록의 크기를 조정하고 남은 공간을 새로운 블록으로 분할합니다.
            header->size = s;
            smheader_ptr remaining_block = (smheader_ptr)((char *)header + HEADER_SIZE + s);
            remaining_block->size = remaining_size - HEADER_SIZE;
            remaining_block->used = 0;
            remaining_block->next = header->next;
            header->next = remaining_block;
        }
        smcoalesce();
        return p;
    }
    else
    {
        // 요청된 크기가 현재 크기보다 크면
        // 새로운 메모리 블록을 할당하고 기존 데이터를 복사합니다.
        void *new_block = smalloc(s);
        if (new_block == NULL)
        {
            fprintf(stderr, "Failed to reallocate memory.\n");
            return NULL;
        }
        memcpy(new_block, p, current_size);
        sfree(p);
        smcoalesce();
        return new_block;
    }
}

void smcoalesce()
{
    smheader_ptr itr = smlist;
    while (itr != NULL && itr->next != NULL)
    {
        if (!itr->used && !itr->next->used)
        {
            uintptr_t current_block = (uintptr_t)itr;
            uintptr_t next_block = (uintptr_t)itr->next;
            if ((current_block / page_size) == (next_block / page_size))
            {
                itr->size += sizeof(struct _smheader) + itr->next->size;
                itr->next = itr->next->next;
            }
            else
            {
                itr = itr->next;
            }
        }
        else
        {
            itr = itr->next;
        }
    }
}
void smdump()
{
    smheader_ptr itr;

    printf("==================== used memory slots ====================\n");
    int i = 0;
    for (itr = smlist; itr != 0x0; itr = itr->next)
    {
        if (itr->used == 0)
            continue;

        printf("%3d:%p:%8d:", i, ((void *)itr) + sizeof(smheader), (int)itr->size);

        int j;
        char *s = ((char *)itr) + sizeof(smheader);
        for (j = 0; j < (itr->size >= 8 ? 8 : itr->size); j++)
        {
            printf("%02x ", s[j]);
        }
        printf("\n");
        i++;
    }
    printf("\n");

    printf("==================== unused memory slots ====================\n");
    i = 0;
    for (itr = smlist; itr != 0x0; itr = itr->next, i++)
    {
        if (itr->used == 1)
            continue;

        printf("%3d:%p:%8d:", i, ((void *)itr) + sizeof(smheader), (int)itr->size);

        int j;
        char *s = ((char *)itr) + sizeof(smheader);
        for (j = 0; j < (itr->size >= 8 ? 8 : itr->size); j++)
        {
            printf("%02x ", s[j]);
        }
        printf("\n");
        i++;
    }
    printf("\n");
}