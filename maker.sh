#!/bin/bash

clang src/$1.c -o bin/$1 -O2 -mavx -std=c2x\
	-Iinclude \
	-Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-variable \
	-Wno-unused-label -Wno-unused-parameter -Wno-unused-but-set-variable \
	$2 $3 $4 $5
