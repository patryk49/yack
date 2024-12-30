#pragma once

#include "utils.h"
#include "structs.h"

#define STATIC_PARAMS_MAX_SIZE 32

StaticMemory static_memory;

static void smem_init(size_t size){
	size_t text_size    = (size / sizeof(BcNode) + 1) / 2;
	size_t classes_size = (size / sizeof(Class) + 3) / 4;
	size_t names_size   = (size / sizeof(char) + 7) / 8;
	static_memory.start = malloc(size + 8);
	if (static_memory.start == NULL){
		assert(false && "memory initialization failrule");
	}
	static_memory.classes  = (Class   *)(static_memory.text    + text_size);
	static_memory.names    = (uint8_t *)(static_memory.classes + classes_size);
	static_memory.writable = (uint8_t *)(static_memory.names   + names_size);

	static_memory.text_top     = static_memory.text;
	static_memory.classes_top  = static_memory.classes;
	static_memory.names_top    = static_memory.names;
	static_memory.writable_top = static_memory.writable;
}

static bool smem_readable(uint8_t *ptr, size_t size){
	return static_memory.start <= ptr && ptr+size < static_memory.end;
}

static bool smem_writeable(uint8_t *ptr, size_t size){
	return static_memory.writable <= ptr && ptr+size < static_memory.end;
}



static uint32_t smem_alloc_class(size_t size){
	assert((uint8_t *)static_memory.classes_top + size < static_memory.names);
	uint32_t result = static_memory.classes_top - static_memory.classes;
	static_memory.classes_top += size;
	return result;
}

static uint32_t smem_push_name(const char *name, uint8_t size){
	assert(static_memory.names_top + size + 1 < (uint8_t *)static_memory.writable);
	uint32_t result = static_memory.names_top - static_memory.names + 1;
	*static_memory.names_top = size;
	memcpy(static_memory.names_top+1, name, size);
	static_memory.names_top += 1 + size;
	return result;
}





static bool class_has_pointer(Class cl){
	return cl.pointered || (cl.prefixes & PREFIX_HAS_PTR_MASK) != 0;
}




