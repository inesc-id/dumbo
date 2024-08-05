#define NUM_PAGES 62
extern __thread unsigned int local_exec_mode;
extern __thread int* transition_count_pages_read;
extern __thread int* pages_read;
extern __thread int* pages_written;
extern __thread int counter_pages_written;
extern __thread int pages_written_heap[500];
extern int writer_count[80][500];
extern __thread int counter_pages_read;
extern __thread long threadID;
