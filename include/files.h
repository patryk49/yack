#pragma once

#include "utils.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#ifndef FILES_EXPECTED_LENGTH
	#define FILES_EXPECTED_LENGTH 64
#endif


typedef struct{
	char *data;
	size_t size;
} StringView;


StringView mmap_file(const char *path){
	StringView res = {};
	int fd = open(path, O_RDONLY);
	if (fd == -1) return res;

	struct stat s;
	int status = fstat(fd, &s);
	if (status != 0 || !S_ISREG(s.st_mode)) return res;
	
	res.data = (char *)mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	res.size = s.st_size;
	return res;
}



StringView read_file(FILE *input){
	size_t capacity = FILES_EXPECTED_LENGTH;
	StringView res = {malloc(capacity), 0};
	if (res.data == NULL) return res;
	
	for (;;){
		int c = getc(input);
		UNLIKELY if (c == EOF) break;

		if (res.size == capacity-1){
			char *new_data = realloc(res.data, res.size*2);
			if (new_data == NULL){
				free(res.data);
				res.data = new_data;
				break;
			}
			res.data = new_data;
			capacity = res.size*2;
		}

		res.data[res.size] = (char)c;
		res.size += 1;
	}

	res.data[res.size] = '\0';
	return res;
}
