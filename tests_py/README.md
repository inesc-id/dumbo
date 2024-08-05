# Plots for PSI experiments

Each experiment is in a python script like the one in
```tests_py/test_H1_H2.py```
check it for more details

## Hypervisor thread pinning

According to the output of:
```/sys/devices/system/cpu/cpuX/topology/thread_siblings_list```
(changing X with the thread ID) threads on the same core have
contiguous IDs, i.e., threads 0-3 are in core 0.

--> we have at maximum 16 threads (without SMT) and 2 sockets
