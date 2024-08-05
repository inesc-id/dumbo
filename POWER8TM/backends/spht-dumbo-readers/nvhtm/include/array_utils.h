// WARN: no guards on this include, define TYPE before

#ifndef TYPE
#define TYPE int
#endif

#ifndef CAT
#define CAT(X,Y) X##_##Y
#define TEMPLATE(X,Y) CAT(X,Y)
#endif

#define TYPED_NAME(x) TEMPLATE(x, TYPE)

// dirty way of doing templates in C
// static inline template<class T>

int i;
static inline int TYPED_NAME(sortedArray)(TYPE *arr, int size, TYPE elem)
{
  int where = size;
  if (size == 0) {
    arr[0] = elem;
    return where;
  }
  for (i = 0; i < size; ++i) {
    if (arr[i] > elem) {
      int j;
      for (j = size; j >= i; --j) {
        arr[j] = arr[j-1];
      }
      arr[i] = elem;
      where = i;
      return where;
    }
  }
  arr[size] = elem; // is the last element
  return where;
}

int j;
static inline int TYPED_NAME(insArrayPos)(TYPE *arr, int size, TYPE elem, int pos)
{
  for (j = size; j >= pos; --j) {
    arr[j] = arr[j-1];
  }
  arr[pos] = elem;
  return pos;
}

#undef TYPE
#undef TYPED_NAME
