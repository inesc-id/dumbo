#include "containers.h"

#include <unordered_set>
#include <cstdint>

#define MEMORY_ALLOCATION 1048576
#define CACHE_LINE_BITS   6 // log2(64)

void *cls_alloc()
{
  std::unordered_set<uint64_t> *res;
  res = new std::unordered_set<uint64_t>(MEMORY_ALLOCATION);
  return res;
}

void cls_free(void *container)
{
  delete (std::unordered_set<uint64_t> *)container;
}

void cls_push(void *container, void *addr)
{
  uint64_t adr = (uint64_t)addr >> CACHE_LINE_BITS;
  ((std::unordered_set<uint64_t> *)container)->insert(adr);
}

void* cls_pop(void *container)
{
  void *addr = nullptr;
  auto cnt = (std::unordered_set<uint64_t> *)container;
  auto it = cnt->begin();
  if (it != cnt->end()) {
    addr = (void*)(*it << CACHE_LINE_BITS);

    // TODO: make sure inserts and erases are not concurrent!
    cnt->erase(it);
  }
  return addr;
}

void cls_swap(void **container1, void **container2)
{
  void *tmp = *container1;
  *container1 = *container2;
  *container2 = tmp;
}
