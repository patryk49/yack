#!/bin/bash

gcc src/$1.c -o bin/$1 -ggdb \
	-Iinclude \
	-Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-variable \
	-Wno-unused-label -Wno-unused-parameter -Wno-unused-but-set-variable \
	-Wno-switch \
	-w \
	$2 $3 $4 $5
