Multicore Matrix Operations
===========================

Performs matrix multiplications by distributing load amongst an arbitrary number of processor cores. 

Dramatically increases computational power by reducing a matrix multiplication into several smaller matrix multiplications.
Each smaller problem is evaluated in parallel, and then all are merged into a result on the host machine. 
