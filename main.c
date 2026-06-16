#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    char stub[32];
};
typedef union header header_t;

pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
header_t *head = NULL;
header_t *tail = NULL;

header_t *get_free_block(size_t size) {
    header_t *cur = head;
    while (cur) {
        if (cur->s.size >= size && cur->s.is_free) {
            return cur;
        }
        cur = cur->s.next;
    }
    return NULL;
}

void *mmalloc(size_t size) {

    if (size == 0) {
        return NULL;
    }

    size = (size + 15) & ~((size_t)15); // align up
    pthread_mutex_lock(&global_malloc_lock);

    // 嘗試尋找已經分配且可用的 block
    header_t *header = get_free_block(size);
    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header + 1);
    }

    // 分配新 header 與 block
    size_t total_size = sizeof(header_t) + size;
    void *block = sbrk(total_size);
    if (block == (void *)(-1)) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (head == NULL) {
        head = header;
    }
    if (tail != NULL) {
        tail->s.next = header;
    }
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
}

void mfree(void *block) {
    if (block == NULL) {
        return;
    }

    pthread_mutex_lock(&global_malloc_lock);

    header_t *header = (header_t *)block - 1;
    void *brk = sbrk(0);

    if ((char *)block + header->s.size != brk) {
        header->s.is_free = 1;
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }

    // section 1 start
    header_t **cur = &head;
    header_t *new_tail = NULL;
    while (*cur != NULL && *cur != tail) {
        new_tail = *cur;
        cur = &(*cur)->s.next;
    }
    *cur = NULL;
    tail = new_tail;
    // section 1 end

    sbrk(-sizeof(header_t) - header->s.size);
    pthread_mutex_unlock(&global_malloc_lock);
}
// section 2 start
// Note: section 2 is equivalent to section 1

// if (head == tail) {
//     head = NULL;
//     tail = NULL;
// } else {
//     // delete last node
//     header_t *cur = head;
//     while (cur) {
//         if (cur->s.next == tail) {
//             cur->s.next = NULL;
//             tail = cur;
//         }
//         cur = cur->s.next;
//     }
// }

// section 2 end

void *mcalloc(size_t num, size_t nsize) {
    if (num == 0 || nsize == 0) {
        return NULL;
    }

    size_t size = num * nsize;

    if (nsize != size / num) {
        // check multiplication overflow
        return NULL;
    }

    void *block = mmalloc(size);
    if (block == NULL) {
        return NULL;
    }
    memset(block, 0, size);
    return block;
}

void *mrealloc(void *block, size_t size) {
    if (size == 0) {
        mfree(block);
        return NULL;
    }

    if (block == NULL) {
        return mmalloc(size);
    }

    header_t *header = (header_t *)block - 1;
    if (header->s.size >= size) {
        return block;
    }

    void *res = mmalloc(size);
    if (res != NULL) {
        memcpy(res, block, header->s.size);
        mfree(block);
    }
    return res;
}

#define SIZE 1024

int main(void) {
    int *ptr = mmalloc(SIZE * sizeof(int));
    assert(ptr != NULL);

    for (size_t i = 0; i < SIZE; i++) {
        ptr[i] = i;
    }

    size_t new_size = 2 * SIZE;
    int *tmp = mrealloc(ptr, new_size * sizeof(int));
    assert(tmp != NULL);

    ptr = tmp;

    for (size_t i = 1024; i < new_size; i++) {
        ptr[i] = i;
    }

    for (size_t i = 0; i < new_size; i++) {
        assert(ptr[i] == i);
    }

    mfree(ptr);
    ptr = NULL;

    ptr = mcalloc(SIZE, sizeof(int));
    for (size_t i = 0; i < SIZE; i++) {
        assert(ptr[i] == 0);
    }
    mfree(ptr);
    ptr = NULL;

    return 0;
}
