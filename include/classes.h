#pragma once

#include "utils.h"
#include "structs.h"



#define ARRAY_SIZE_MAX (UINT32_MAX - 1)
#define TUPLE_SIZE_MAX (400)
#define ARG_COUNT_MAX TUPLE_SIZE_MAX


// GLOBAL BYTECODE BUFFER
#define BC_BUFFER_CAPACITY (1 << 27)
BcNode *global_bc;
size_t global_bc_size = 0;

// initialize bytecode linked list
BcNode global_bc_head;
BcNode *global_bc_last = &global_bc_head;




// IDENTIFIER STUFF
static uint64_t name_hash(const char *str, size_t length){
	// djb2 hash
	uint64_t hash = 5381;
	for (size_t i=0; i!=length; i+=1){
		hash = ((hash << 5u) + hash) + (uint64_t)str[i];
	}
	return hash;
}

struct NameEntry{
	uint64_t hash;
	NameId   name_id : 24;
	size_t   length  : 8;
	uint32_t data_index;
};

struct GlobalNameSet{
	struct NameEntry *data;
	size_t size     : 32;
	size_t capacity : 32;
};

struct GlobalNameData{
	uint8_t *data;
	size_t   size     : 32;
	size_t   capacity : 32;
};


static struct GlobalNameSet  global_name_set;
static struct GlobalNameData global_names;

static size_t hash_colissions = 0; 

static NameId get_name_id(const char *str, uint8_t length){
	assert(util_is_power2_u32(global_name_set.capacity));
	assert(length != 0 && length <= 255);

	struct GlobalNameSet  name_set = global_name_set;
	struct GlobalNameData names    = global_names;

	uint64_t hash = name_hash(str, length);
	size_t index_mask = name_set.capacity - 1;
	size_t index = hash & index_mask;
	for (size_t i=0;; i+=1){
		struct NameEntry entry = name_set.data[index];
		if (entry.length == 0) break; // name not found
		if (entry.hash == hash && entry.length == length){
			if (memcmp(names.data + entry.name_id, str, length) == 0){
				return entry.name_id;
			}
			hash_colissions += 1;
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry's name data
	NameId result = names.size + 1;
	size_t new_names_size = names.size + length + 1;
	if (new_names_size > names.capacity){
		size_t   new_names_capacity = 2*names.capacity;
		uint8_t *new_names_data = malloc(new_names_capacity);
		assert(new_names_data != NULL && "name allocation failrule");
		memcpy(new_names_data, names.data, names.size);
		free(names.data);
		names.data     = new_names_data;
		names.capacity = new_names_capacity;
		global_names = names;
	}
	names.data[names.size] = length;
	memcpy(names.data+names.size+1, str, length);
	global_names.size = new_names_size;
	
	// add new entry to set
	name_set.data[index] = (struct NameEntry){
		.hash    = hash,
		.name_id = result,
		.length  = length
	};
	name_set.size += 1;
	global_name_set.size = name_set.size;
	
	UNLIKELY if (4*name_set.size >= 3*name_set.capacity){
		// resize hash table
		size_t new_hs_capacity = 2*name_set.capacity;
		struct NameEntry *new_hs_data = malloc(new_hs_capacity*sizeof(struct NameEntry));
		if (new_hs_data == NULL){
			assert(false && "name allocation failrule");
		}
		memset(new_hs_data, 0, new_hs_capacity*sizeof(struct NameEntry));
		// reindex old hash table
		size_t elem_index_mask = new_hs_capacity - 1;
		for (size_t i=0; i!=name_set.capacity; i+=1){
			struct NameEntry old_set_entry = name_set.data[i];
			if (old_set_entry.name_id != 0){
				size_t elem_index = old_set_entry.hash & elem_index_mask;
				for (size_t i=0;; i+=1){
					struct NameEntry entry = new_hs_data[elem_index];
					if (entry.length == 0) break; // add elem to new table
					elem_index = (elem_index + i + 1) & elem_index_mask;
				}
				new_hs_data[elem_index] = old_set_entry;
			}
		}
		free(name_set.data);
		global_name_set.data     = new_hs_data;
		global_name_set.capacity = new_hs_capacity;
	}
	return result;
}




// CLASS INFO FUNCTIONS 
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

static bool class_has_pointer(Class cl){
	return cl.pointered || (cl.prefixes & PREFIX_HAS_PTR_MASK) != 0;
}





// CLASS INFO DATA
struct ClassInfoArray{
	ClassInfoHeader *data;
	size_t size     : 32;
	size_t capacity : 32;
	struct ClassInfoArray *next;
};

struct ClassInfoArray global_classes;

uint32_t global_classes_alloc(size_t size){
	size_t alloc_size = (size + sizeof(ClassInfoHeader) - 1) / sizeof(ClassInfoHeader);
	size_t new_size = global_classes.size + alloc_size;
	if (new_size > global_classes.capacity){
		assert(false && "class info array is out of memory");
	}
	size_t res = global_classes.size;
	global_classes.size = new_size;
	return res;
}

static uint32_t get_bytesize(Class cl){
	if (class_is_pointer(cl)) return PTR_SIZE;
	if (class_is_span(cl)) return 2*PTR_SIZE;
	if (cl.tag <= Class_Float) return cl.basic_size;
	return global_classes.data[cl.idx].bytesize;
}

static uint8_t get_alignment(Class cl){
	if (class_is_pointer(cl) | class_is_span(cl)) return PTR_SIZE;
	if (cl.tag <= Class_Float) return cl.basic_alignment;
	return global_classes.data[cl.idx].alignment;
}





// ARRAY HASH TABLE
static ArrayClassInfo *array_class_info(uint32_t index){
	return (ArrayClassInfo *)(global_classes.data + index);
}

static uint64_t array_class_hash(Class cl, size_t size){
	return class_hash(cl) ^ (size << 3);
}

struct ArrayClassEntry{
	uint64_t hash;
	Class clas; // id = 0 means empty slot
};

struct ArrayClassSet{
	struct ArrayClassEntry *data;
	size_t size     : 32;
	size_t capacity : 32;
};

struct ArrayClassSet global_array_set;

static Class get_array_class(Class cl, size_t size){
	assert(util_is_power2_u32(global_array_set.capacity));

	struct ArrayClassSet array_set = global_array_set;

	uint64_t hash = array_class_hash(cl, size);
	size_t index_mask = array_set.capacity - 1;
	size_t index = hash & index_mask;
	for (size_t i=0;; i+=1){
		struct ArrayClassEntry entry = array_set.data[index];
		if (entry.clas.id == 0) break; // name not found
		if (entry.hash == hash){
			const ArrayClassInfo *info = array_class_info(entry.clas.idx);
			if (size == info->size && cl.id == info->arg_class.id) return entry.clas;
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry's data
	Class res = {
		.tag = Class_Array,
		.idx = global_classes_alloc(sizeof(ArrayClassInfo))
	};
	ArrayClassInfo *res_info = array_class_info(res.idx);
	
	*res_info = (ArrayClassInfo){
		.bytesize = size*get_bytesize(cl),
		.alignment = get_alignment(cl),
		.size = size,
		.arg_class = cl
	};

	// add new entry to set
	array_set.data[index] = (struct ArrayClassEntry){ .hash = hash, .clas = res };
	array_set.size += 1;
	global_array_set.size = array_set.size;
	
	UNLIKELY if (4*array_set.size >= 3*array_set.capacity){
		// resize hash table
		size_t new_hs_capacity = 2*array_set.capacity;
		size_t entry_bytesize = new_hs_capacity*sizeof(struct ArrayClassEntry);
		struct ArrayClassEntry *new_hs_data = malloc(entry_bytesize);
		if (new_hs_data == NULL){
			assert(false && "name allocation failrule");
		}
		memset(new_hs_data, 0, entry_bytesize);
		// reindex old hash table
		size_t elem_index_mask = new_hs_capacity - 1;
		for (size_t i=0; i!=array_set.capacity; i+=1){
			struct ArrayClassEntry old_set_entry = array_set.data[i];
			if (old_set_entry.clas.id != 0){
				size_t elem_index = old_set_entry.hash & elem_index_mask;
				for (size_t i=0;; i+=1){
					struct ArrayClassEntry entry = new_hs_data[elem_index];
					if (entry.clas.id == 0) break; // add elem to new table
					elem_index = (elem_index + i + 1) & elem_index_mask;
				}
				new_hs_data[elem_index] = old_set_entry;
			}
		}
		free(array_set.data);
		global_array_set.data     = new_hs_data;
		global_array_set.capacity = new_hs_capacity;
	}
	return res;
}





// TUPLE HASH TABLE
static TupleClassInfo *tuple_class_info(uint32_t index){
	return (TupleClassInfo *)(global_classes.data + index);
}

static uint64_t tuple_class_hash(const Class *cls, size_t cls_size){
	if (cls_size == 0) return 13421;
	return class_hash(cls[0]) ^ (cls_size << 3);
}

static uint64_t tuple_class_equals(
	const Class *cls, size_t cls_size, const TupleClassInfo *info
){
	if (cls_size == info->size) return false;
	for (size_t i=0; i!=cls_size; i+=1){
		if (cls[i].id != info->arg_classes[i].id) return false;
	}
	return true;
}

struct TupleClassEntry{
	uint64_t hash;
	Class clas; // id = 0 means empty slot
};

struct TupleClassSet{
	struct TupleClassEntry *data;
	size_t size     : 32;
	size_t capacity : 32;
};

struct TupleClassSet global_tuple_set;

static Class get_tuple_class(const Class *cls, size_t cls_size){
	assert(util_is_power2_u32(global_tuple_set.capacity));

	struct TupleClassSet tuple_set = global_tuple_set;

	uint64_t hash = tuple_class_hash(cls, cls_size);
	size_t index_mask = tuple_set.capacity - 1;
	size_t index = hash & index_mask;
	for (size_t i=0;; i+=1){
		struct TupleClassEntry entry = tuple_set.data[index];
		if (entry.clas.id == 0) break; // name not found
		if (entry.hash == hash){
			const TupleClassInfo *info = tuple_class_info(entry.clas.idx);
			if (tuple_class_equals(cls, cls_size, info)) return entry.clas;
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry's data
	Class res = {
		.tag = Class_Tuple,
		.idx = global_classes_alloc(sizeof(TupleClassInfo) + cls_size*sizeof(Class))
	};
	TupleClassInfo *res_info = tuple_class_info(res.idx);
	
	Class *res_classes = res_info->arg_classes;
	uint32_t *res_offsets = (uint32_t *)(res_info->arg_classes + cls_size);

	uint32_t bytesize = 0; 
	uint8_t max_alignment = 0;
	for (size_t i=0; i!=cls_size; i+=1){
		res_classes[i] = cls[i];
		res.infered |= cls[i].infered;
		res.pointered |= cls[i].pointered;
		if (!res.infered){
			uint8_t arg_alignment = get_alignment(cls[i]);
			bytesize = util_alignsize(bytesize, 1<<arg_alignment);
			res_offsets[i] = bytesize;
			if (arg_alignment > max_alignment){ max_alignment = arg_alignment; }
			bytesize += get_bytesize(cls[i]);
		}
	}
	*res_info = (TupleClassInfo){
		.bytesize = util_alignsize(bytesize, 1<<max_alignment),
		.alignment = max_alignment,
		.size = cls_size
	};

	// add new entry to set
	tuple_set.data[index] = (struct TupleClassEntry){ .hash = hash, .clas = res };
	tuple_set.size += 1;
	global_tuple_set.size = tuple_set.size;
	
	UNLIKELY if (4*tuple_set.size >= 3*tuple_set.capacity){
		// resize hash table
		size_t new_hs_capacity = 2*tuple_set.capacity;
		size_t entry_bytesize = new_hs_capacity*sizeof(struct TupleClassEntry);
		struct TupleClassEntry *new_hs_data = malloc(entry_bytesize);
		if (new_hs_data == NULL){
			assert(false && "name allocation failrule");
		}
		memset(new_hs_data, 0, entry_bytesize);
		// reindex old hash table
		size_t elem_index_mask = new_hs_capacity - 1;
		for (size_t i=0; i!=tuple_set.capacity; i+=1){
			struct TupleClassEntry old_set_entry = tuple_set.data[i];
			if (old_set_entry.clas.id != 0){
				size_t elem_index = old_set_entry.hash & elem_index_mask;
				for (size_t i=0;; i+=1){
					struct TupleClassEntry entry = new_hs_data[elem_index];
					if (entry.clas.id == 0) break; // add elem to new table
					elem_index = (elem_index + i + 1) & elem_index_mask;
				}
				new_hs_data[elem_index] = old_set_entry;
			}
		}
		free(tuple_set.data);
		global_tuple_set.data     = new_hs_data;
		global_tuple_set.capacity = new_hs_capacity;
	}
	return res;
}



// PROCEDURE HALPER PROCEDURES
static ProcedureClassInfo *procedure_class_info(uint32_t index){
	return (ProcedureClassInfo *)(global_classes.data + index);
}



// INITIALIZING GLOBAL VARIABLES
static void initialize_compiler_globals(void){
	// global bytecode buffer
	global_bc = malloc(BC_BUFFER_CAPACITY*sizeof(BcNode));
	assert(global_bc != NULL);

	// class info data
	global_classes.capacity = (1 << 24); // for now just make it big
	global_classes.size = 0;
	global_classes.data = malloc(global_classes.capacity*sizeof(ClassInfoHeader));
	assert(global_classes.data != NULL);

	// name set
	global_name_set.capacity = 256;
	global_name_set.size = 0;
	size_t name_set_bytes = global_name_set.capacity*sizeof(struct NameEntry);
	global_name_set.data = malloc(name_set_bytes);
	assert(global_name_set.data != NULL);
	memset(global_name_set.data, 0, name_set_bytes);
	
	// name data 
	global_names.capacity = global_name_set.capacity*(1+8);
	global_names.size = 0;
	global_names.data = malloc(global_names.capacity);
	assert(global_names.data != NULL);

	// array set
	global_array_set.capacity = 64;
	global_array_set.size = 0;
	size_t array_set_bytes = global_array_set.capacity*sizeof(struct ArrayClassEntry);
	global_array_set.data = malloc(array_set_bytes);
	assert(global_array_set.data != NULL);
	memset(global_array_set.data, 0, array_set_bytes);

	// tuple set
	global_tuple_set.capacity = 64;
	global_tuple_set.size = 0;
	size_t tuple_set_bytes = global_tuple_set.capacity*sizeof(struct TupleClassEntry);
	global_tuple_set.data = malloc(tuple_set_bytes);
	assert(global_tuple_set.data != NULL);
	memset(global_tuple_set.data, 0, tuple_set_bytes);

	// keywords & directires
	init_keyword_names();
}


static const char *infer_argument_class(Class source, Class *target_ptr, Data *infers){
	Class target = *target_ptr;
	assert(source.tag == Class_Initlist || !source.infered);
	assert(target.infered);

	switch (target.tag){
	case Class_Infered:
		//infers[target.infro.var_index].clas = source;
		//source = infers[target.infro.var_index].clas;
		break;
	case Class_Array:{
		if (source.tag != Class_Array)
			return "argument's class doesn\'t match the target";
		ArrayClassInfo *target_info = array_class_info(target.idx);
		ArrayClassInfo *source_info = array_class_info(source.idx);
		Class target_arg = target_info->arg_class;
		if (target_info->size < 0){
			infers[1-target_info->size].u64 = source_info->size;
		}
		if (target_arg.infered){
			Class source_arg = source_info->arg_class;
			const char *err = infer_argument_class(source_arg, &target_arg, infers);
			if (err != NULL) return err;
			source = get_array_class(target_arg, source_info->size);
		}
		break;
	}

	default: break;
	}

	source.prefixes = target.prefixes;
	*target_ptr = source;
	return NULL;	
}


static const char *match_argument(ValueInfo *arg, Class target, bool ctime, Data *infers){
	Class source = arg->clas;
	if (ctime && !(arg->flags & VF_Const))
		return "provided argument is not known at compile time";
	
	if (target.infered){
		const char *err = infer_argument_class(source, &target, infers);
		if (err != NULL) return err;
	}
	arg->clas = target;
}




