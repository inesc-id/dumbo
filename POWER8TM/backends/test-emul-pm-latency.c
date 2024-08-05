#include "extra_MACROS.h" 
#include <time.h>
#include <stdio.h>

#define REPETITIONS 1000000

int main() {
    for (int n = 10; n <= 100; n+=2) {
        volatile int i;
        clock_t t1 = clock();
        for (int k=0; k<REPETITIONS; k++) {
            for (i = 0; i < n; ++i)
                __asm__ volatile ("nop" ::: "memory");
        }
        clock_t t2 = clock();

    // printf("CLOCKS_PER_SEC=%d, n=%d, t2-t1=%d\n", CLOCKS_PER_SEC, n, ((int) (t2-t1)));

        printf("n=%d, %ld ns\n", n, ((t2-t1)/(CLOCKS_PER_SEC/1000000))/(REPETITIONS/1000));
    }
}


/* opções a experimentar: 
- n=10 (43ns, preprint do Izrael*)
- n=46 (eurosys'22)
- n=66 (TPP -> extrapolation to PM/CXL)
*/

/* TO DO
- exportar n como parametro do script de testes
- correr testes nas 3 opções
*/