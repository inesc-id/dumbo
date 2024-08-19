# DUMBO: Making durable read-only transactions fly on hardware transactional memory

Paper under submission by João P. Barreto, Daniel Castro, Paolo Romano, and Alexandro Baldassin.

In this repository you can find the implementations of DUMBO, SPHT, SI-HTM, Pisces, and others.
We develop a Persistent TM on IBM's POWER ISA (tested on POWER9). The starting point was the POWER8 HTM implementation with, Rollback-only transactions and suspend-resume, from SI-HTM. We currently emulate Persistent Memory (PM) having in account what is known from Compute Express Link (CXL) and Intel Optane DC PM.

For detailed instructions, check the README file inside the POWER8TM folder (https://github.com/inesc-id/dumbo/blob/main/POWER8TM/README.md).
