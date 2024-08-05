#!/bin/bash

g++ -o main.out main.c -L .. -l htm_sgl -I ../include -I \
	../../arch_dep/include -mrtm -g

g++ -o test_conflicts.out test_conflicts.c -L .. -l htm_sgl -I ../include -I \
	../../arch_dep/include -mrtm -pthread -g

g++ -o test_htm_tsc_reorder.out test_htm_tsc_reorder.c -L .. -l htm_sgl -I ../include -I \
	../../arch_dep/include -mrtm -pthread -g
