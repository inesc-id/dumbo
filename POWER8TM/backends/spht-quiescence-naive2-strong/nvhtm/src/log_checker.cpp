#include "impl.h"

#include <map>

using namespace std;

/* extern*/FILE *error_fp = NULL;

struct tx_info {
  int threadId;
  uint64_t CLC;
  uint64_t CPC;
  uint64_t LPC;
};

// static map<> sorted;

// sortAllThreads --> key = LC, <threadId, positionInLog>
static int sort_N_txs(uint64_t lastCounter, int N, map<uint64_t,tx_info> &sortAllThreads)
{
  int gapsFound = 0;
  uint64_t counter;
  uint64_t target = lastCounter + 1;

  while (sortAllThreads.size() < (size_t)N) {
    int foundNone = 0;
    int testedTarget = 0;

    // must insert 1 by one as the same thread could have done 2 TXs
    for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
      int start = G_next[i].log_ptrs.write_log_start;
      if (start == G_next[i].log_ptrs.write_log_next) {
        foundNone++;
        continue;
      }
      while (!isBit63One(P_write_log[i][start]) && start != G_next[i].log_ptrs.write_log_next) {
        // passes all the writes ands goes to the TSs (assumes <addr,val>, first bit of addrs is 0)
        start = (start + 2) % gs_appInfo->info.allocLogSize; // <addr, val>
        if (start == G_next[i].log_ptrs.write_log_next) break;
      }
      uint64_t LPC = 0; // erased debug code
      uint64_t CPC = 0;
      counter = zeroBit63(P_write_log[i][start]);
      if (counter == target) {
        // 
        start = (start + 1) % gs_appInfo->info.allocLogSize; // add 1 entries <CLC>
        G_next[i].log_ptrs.write_log_start = start;
        sortAllThreads.insert(make_pair(counter, (tx_info){
          .threadId = i,
          .CLC = counter,
          .CPC = CPC,
          .LPC = LPC
        }));
        break; // go for next target
      } else {
        testedTarget++;
      }
    }
    if (foundNone == gs_appInfo->info.nbThreads) break; // no more TXs available
    target++;
    if (testedTarget == gs_appInfo->info.nbThreads) { // GAP!!!!
      gapsFound++;
    }
  }

  return gapsFound;
}

static void printf_log_state(int threadId, int CLC, map<uint64_t,tx_info> &sortAllThreads)
{
  // prints 10 TXs before and 5 after
  //fprintf(error_fp, " ################################### \n");
  //fprintf(error_fp, "[thread=%i] ERROR TX (CLC=%i): dumping logged state\n", threadId, CLC);
  //for (auto it = sortAllThreads.begin(); it != sortAllThreads.end(); ++it) {
    //fprintf(error_fp, "  [thread %i] LC=%lu LPC=%lx CPC=%lx\n", it->second.threadId,
      //it->second.CLC, it->second.LPC, it->second.CPC);
  //}
  //fprintf(error_fp, " ################################### \n");
}

// static void move_log_after(uint64_t lc)
// {
//   for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
//     uint64_t counter = 0;
//     while (counter < lc) {
//       int start = G_next[i].log_ptrs.write_log_start;
//       if (start == G_next[i].log_ptrs.write_log_next) continue; // no TXs available
//       while (!isBit63One(P_write_log[i][start]) && start != G_next[i].log_ptrs.write_log_next) {
//         // passes all the writes ands goes to the TSs (assumes <addr,val>, first bit of addrs is 0)
//         start = (start + 2) % gs_appInfo->info.allocLogSize; // <addr, val>
//         if (start == G_next[i].log_ptrs.write_log_next) break;
//       }
//       uint64_t counter = zeroBit63(P_write_log[i][(start + 1) % gs_appInfo->info.allocLogSize]);
//       if (counter < lc) {
// #ifndef NDEBUG
//         start = (start + 3) % gs_appInfo->info.allocLogSize; // add 3 entries <LPC, CPC, CLC>
// #else // production code
//         start = (start + 1) % gs_appInfo->info.allocLogSize; // add 1 entries <CLC>
// #endif
//         G_next[i].log_ptrs.write_log_start = start; // removes this TX from the LOG
//       }
//     }
//   }
// }

void state_log_checker_pcwc(void(*waitFn)(), int isAfterRun)
{
  const int N = 50; // extracts 20 transactions at a time from the log
  uint64_t lastCounter = 0;
  int donePrinting = 0, gaps;
  int numberOfPrints = 0;
  size_t numberOfEntriesProcessed = 0;

  while (1) {
    map<uint64_t,tx_info> extractedTXs;
    gaps = sort_N_txs(lastCounter, N, extractedTXs);

    if (gaps > 0) {
      //fprintf(error_fp, " gaps detected, lastCounter=%lu, N=%i \n", lastCounter, N);
    }
    if (extractedTXs.empty()) {
      break;
    }
    numberOfEntriesProcessed += extractedTXs.size();

    donePrinting = 0;
    for (auto it = extractedTXs.begin(); it != extractedTXs.end(); ++it) {
      if (it->second.CLC != lastCounter + 1 && !donePrinting) {
        printf_log_state(it->second.threadId, it->second.CLC, extractedTXs);
        donePrinting = 1;
        numberOfPrints++;
      }
      lastCounter = it->second.CLC; // could jump to the last
    }
    if (numberOfPrints > 5) {
      break; // enough info
    }
  }

  printf("processed %zu entries \n", numberOfEntriesProcessed);
  //fclose(error_fp);
}

// void state_log_checker_pcwc(void(*waitFn)(), int isAfterRun)
// {
//   // goes over the logs and look for gaps (debug)
//   uint64_t lastCounter = 0;
//   int lastTID = -1;
//   uint64_t lastLPC = 0;
//   int foundNext = 0;
//   int numberOfRetries = 0; // scan all logs up to 3 times in an attempt to find the next entry
//   int nbOfEmptyLogs = 0;
//   int nbOfFailedLookups = 0;
//   int nbOfTries = 0; // reset this when the lastCounter is incremented (exit on threshold if isAfterRun)

//   if (error_fp == NULL) error_fp = stderr;

//   while (!gs_appInfo->info.isExit) {
//     nbOfEmptyLogs = 0;
//     nbOfFailedLookups = 0;
//     if (isAfterRun && nbOfTries > 10 /* TODO: threshold */) {
//       break;
//     }
//     nbOfTries++;
//     for (int i = 0; i < gs_appInfo->info.nbThreads; ++i) {
//       int start = G_next[i].log_ptrs.write_log_start;
//       if (start == G_next[i].log_ptrs.write_log_next) {
//         nbOfEmptyLogs++;
//         continue;
//       }
//       // now it is 2 entries: <LPC, LLC>
//       while (!isBit63One(P_write_log[i][start]) && start != G_next[i].log_ptrs.write_log_next && !gs_appInfo->info.isExit) {
//         // passes all the writes ands goes to the TSs (assumes <addr,val>, first bit of addrs is 0)
//         start = (start + 2) % gs_appInfo->info.allocLogSize; // <addr, val>
//         if (!isAfterRun) {
//           while (start == G_next[i].log_ptrs.write_log_next && !gs_appInfo->info.isExit) {
//             waitFn(); // TOO FAST!
//           }
//         } else if (start == (G_next[i].log_ptrs.write_log_next + 1) % gs_appInfo->info.allocLogSize) {
//           // we are out of log and no one will produce more
//           goto ret; // TODO: check if there is still some log left...
//         }
//       }
//       uint64_t LPC = zeroBit63(P_write_log[i][start]);
//       uint64_t counter = zeroBit63(P_write_log[i][(start + 1) % gs_appInfo->info.allocLogSize]);
//       G_next[i].log_ptrs.write_log_start = start;
//       if (counter == lastCounter + 1) {
//         start = (start + 2) % gs_appInfo->info.allocLogSize; // add 2 entries <LPC, LLC>
//         if (!isAfterRun) {
//           while (start == G_next[i].log_ptrs.write_log_next && !gs_appInfo->info.isExit) {
//             waitFn(); // TOO FAST!
//           }
//         } else if (start == (G_next[i].log_ptrs.write_log_next + 1) % gs_appInfo->info.allocLogSize) {
//           // we are out of log and no one will produce more
//           goto ret; // TODO: check if there is still some log left...
//         }
//         G_next[i].log_ptrs.write_log_start = start;
//         if (LPC < lastLPC) {
//           fprintf(error_fp, "[ERROR!!!]: order inversion <LLC=%li,LPC=%lx> <LLC=%li,LPC=%lx> \n", lastCounter, lastLPC, counter, LPC);
//         }
//         lastLPC = LPC;
//         nbOfTries = 0;
//         lastCounter++;
//         lastTID = i;
//         foundNext = 1;
//       } else if (counter <= lastCounter) {
//         fprintf(error_fp, "[ERROR!!!]: counter[%i]=%li lastCounter[%i]=%li!!\n", i, counter, lastTID, lastCounter);
//         lastLPC = LPC;
//         lastTID = i;
//         nbOfFailedLookups++;
//         start = (start + 2) % gs_appInfo->info.allocLogSize; // add 2 entries <LPC, LLC>
//         if (!isAfterRun) {
//           while (start == G_next[i].log_ptrs.write_log_next && !gs_appInfo->info.isExit) {
//             waitFn(); // TOO FAST!
//           }
//         } else if (start == (G_next[i].log_ptrs.write_log_next + 1) % gs_appInfo->info.allocLogSize) {
//           // we are out of log and no one will produce more
//           goto ret; // TODO: check if there is still some log left...
//         }
//         G_next[i].log_ptrs.write_log_start = start;
//       } else {
//         nbOfFailedLookups++;
//       }
//     }
//     if (nbOfFailedLookups == gs_appInfo->info.nbThreads && nbOfEmptyLogs == 0) {
//       foundNext = 0;
//     }
//     if (nbOfEmptyLogs == gs_appInfo->info.nbThreads) {
//       // sleep
//       waitFn();
//       numberOfRetries = 0;
//       if (isAfterRun) {
//         // done analyzing the log
//         goto ret;
//       }
//     } else if (!foundNext) {
//       numberOfRetries++;
//       if (numberOfRetries > 3) {
//         fprintf(error_fp, "[ERROR!!!]: LLC %li not found!\n", lastCounter+1);
//         nbOfTries = 0;
//         lastCounter++;
//         numberOfRetries = 0;
//         waitFn();
//       }
//     }
//   }
// ret:
//   fflush(error_fp);
//   fprintf(error_fp, "leaving log checker: checked %li entries\n --- \n", lastCounter);
// }
