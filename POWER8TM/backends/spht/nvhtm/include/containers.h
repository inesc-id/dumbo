#ifndef CONTAINERS_H_GUARD
#define CONTAINERS_H_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EASY_MALLOC
#define MALLOC_FN(size) aligned_alloc(ARCH_CACHE_LINE_SIZE, (size))
#define EASY_MALLOC(var, nb) \
  if ((var = (__typeof__(var))MALLOC_FN(sizeof(__typeof__(*(var)))*(nb))) == NULL) \
    perror("malloc");
#endif /* EASY_MALLOC */

void *cls_alloc();
void cls_free(void *container);
void cls_push(void *container, void *addr);
void* cls_pop(void *container);

// swaps the memory position of the two containers
void cls_swap(void **container1, void **container2);

#ifdef __cplusplus
}
#endif

#endif /* CONTAINERS_H_GUARD */
