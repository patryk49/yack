#pragma once

#include "utils.h"
#include "structs.h"

#include "bytecode.h"


#define ARRAY_SIZE_MAX (UINT32_MAX - 1)
#define TUPLE_SIZE_MAX (400)
#define ARG_COUNT_MAX TUPLE_SIZE_MAX



// IDENTIFIER STUFF
static uint64_t name_hash(const char *str, size_t length){
	// djb2 hash
	uint64_t hash = 5381;
	for (size_t i=0; i!=length; i+=1){
		hash = ((hash << 5u) + hash) + (uint64_t)*str;
	}
	return hash;
}


typedef struct NameEntry{
	uint64_t hash;
	NameId   name_id;
	uint8_t  length;
} NameEntry;


typedef struct GlobalNameSet{
	NameEntry *data;
	uint32_t   size;
	uint32_t   capacity;
} GlobalNameSet;


typedef struct GlobalNameData{
	char    *data;
	uint32_t size;
	uint32_t capacity;
} GlobalNameData;


static GlobalNameSet global_name_set;


static NameId get_name_id(const char *str, uint8_t length){
	assert(util_is_power2_u32(global_name_set.capacity));

	GlobalNameSet name_set = global_name_set;

	uint64_t hash = name_hash(str, length);
	size_t index_mask = name_set.capacity - 1;
	size_t index = hash & index_mask;
	for (size_t i=0;; i+=1){
		NameEntry entry = name_set.data[index];
		if (entry.length == 0) break; // name not found
		if (entry.hash == hash && entry.length == length){
			if (memcmp(static_memory.names + entry.name_id, str, length) == 0){
				return entry.name_id;
			}
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry
	NameId result = smem_push_name(str, length);
	global_name_set.data[index] = (NameEntry){
		.hash    = hash,
		.name_id = result,
		.length  = length
	};
	global_name_set.size += 1;
	
	UNLIKELY if (3*global_name_set.size >= 4*global_name_set.capacity){
		// resize hash table
		GlobalNameSet new_set;
		new_set.size = global_name_set.size;
		new_set.capacity = 2*global_name_set.capacity;
		new_set.data = malloc(new_set.capacity*sizeof(NameEntry));
		if (new_set.data == NULL){
			assert(false && "name allocation failrule");
		}
		memset(new_set.data, 0, new_set.capacity*sizeof(NameEntry));
		// reindex old hash table
		size_t elem_index_mask = new_set.capacity - 1;
		for (size_t i=0; i!=global_name_set.capacity; i+=1){
			NameEntry old_set_entry = global_name_set.data[i];
			if (old_set_entry.name_id != 0){
				size_t elem_index = old_set_entry.hash & elem_index_mask;
				for (size_t i=0;; i+=1){
					NameEntry entry = new_set.data[elem_index];
					if (entry.length == 0) break; // add elem to new table
					elem_index = (elem_index + i + 1) & elem_index_mask;
				}
				new_set.data[elem_index] = old_set_entry;
			}
		}
		free(global_name_set.data);
		global_name_set = new_set;
	}
	return result;
}


static void init_name_id_set(size_t capacity){
	assert(capacity >= 16);
	assert(util_is_power2_u32(capacity));

	global_name_set.size = 0;
	global_name_set.capacity = capacity;
	global_name_set.data = malloc(capacity*sizeof(NameEntry));
	assert(global_name_set.data != NULL && "name allocation failrule");
	memset(global_name_set.data, 0, capacity*sizeof(NameEntry));
}





// GLOBAL CLASS TABLES
static uint64_t class_hash(Class cl){
	return cl.id ^ (cl.id << 9u);
}


static bool class_is_pointer(Class cl){
	return (cl.prefixes & PREFIX_MASK) == ClassPrefix_Pointer;
}

static bool class_is_span(Class cl){
	return (cl.prefixes & PREFIX_MASK) == ClassPrefix_Span;
}

static Class class_add_prefix(Class cl, enum ClassPrefix prefix){
	cl.prefixes <<= PREFIX_SIZE;
	cl.prefixes |= prefix;
	return cl;
}

static Class class_remove_prefix(Class cl){
	cl.prefixes >>= PREFIX_SIZE;
	return cl;
}

