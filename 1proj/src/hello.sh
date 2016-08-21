#!/bin/bash

mpicc --prefix /usr/local/share/OpenMPI -o hello hello.c

mpirun --prefix /usr/local/share/OpenMPI -np $1 hello

rm -f hello
