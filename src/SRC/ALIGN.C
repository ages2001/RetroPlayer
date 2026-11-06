/* Aligned C allocation for codecs; all storage belongs to CRTDLL. */
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

typedef struct {
    void *allocation;
    size_t size;
    size_t alignment;
} RP_ALIGNED;

void *rp_aligned_malloc(size_t size, size_t alignment)
{
    void *allocation;
    uintptr_t address;
    size_t effective, overhead;
    RP_ALIGNED *header;
    if (!size || !alignment || (alignment & (alignment - 1))) {
        errno = EINVAL; return NULL;
    }
    effective = alignment < sizeof(void *) ? sizeof(void *) : alignment;
    if (effective - 1 > SIZE_MAX - sizeof(*header)) { errno = ENOMEM; return NULL; }
    overhead = sizeof(*header) + effective - 1;
    if (size > SIZE_MAX - overhead) { errno = ENOMEM; return NULL; }
    allocation = malloc(size + overhead);
    if (!allocation) { errno = ENOMEM; return NULL; }
    address = ((uintptr_t)allocation + sizeof(*header) + effective - 1) & ~(effective - 1);
    header = (RP_ALIGNED *)address - 1;
    header->allocation = allocation;
    header->size = size;
    header->alignment = alignment;
    return (void *)address;
}

void rp_aligned_free(void *pointer)
{
    if (pointer) free(((RP_ALIGNED *)pointer - 1)->allocation);
}

void *rp_aligned_realloc(void *pointer, size_t size, size_t alignment)
{
    RP_ALIGNED *header;
    unsigned char *result, *previous = pointer;
    size_t copy, i;
    if (!pointer) return rp_aligned_malloc(size, alignment);
    header = (RP_ALIGNED *)pointer - 1;
    if (alignment != header->alignment) { errno = EINVAL; return NULL; }
    if (!size) { rp_aligned_free(pointer); return NULL; }
    result = rp_aligned_malloc(size, alignment);
    if (!result) return NULL;
    copy = size < header->size ? size : header->size;
    for (i = 0; i < copy; ++i) result[i] = previous[i];
    rp_aligned_free(pointer);
    return result;
}
