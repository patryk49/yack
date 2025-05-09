#pragma once

#include "utils.h"
#include "structs.h"


_Static_assert(sizeof(Class) == sizeof(BcNode));


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



static StructClassInfo *struct_class_info(uint32_t index){
	return (StructClassInfo *)(global_classes.data + index);
}

static EnumClassInfo *enum_class_info(uint32_t index){
	return (EnumClassInfo *)(global_classes.data + index);
}





// ARRAY HASH TABLE
static ArrayClassInfo *array_class_info(uint32_t index){
	return (ArrayClassInfo *)(global_classes.data + index);
}

static uint64_t array_class_hash(Class cl, uint32_t size){
	return class_hash(cl) ^ (size * 9);
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

static Class get_array_class(Class cl, uint32_t size){
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
			if (size == info->size && cl.id == info->arg_class.id){
				return entry.clas;
			}
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry's data
	uint32_t stag = size & ARRAY_SIZE_TAG_MASK;
	Class res = {
		.tag = Class_Array,
		.infered = cl.infered || stag==ARRAY_SIZE_TAG_INFERED,
		.evaled  = cl.evaled || stag==ARRAY_SIZE_TAG_VARIABLE || stag==ARRAY_SIZE_TAG_EXPR,
		.idx = global_classes_alloc(sizeof(ArrayClassInfo))
	};
	ArrayClassInfo *res_info = array_class_info(res.idx);
	
	*res_info = (ArrayClassInfo){
		.size      = size,
		.arg_class = cl
	};
	if (!res.infered && !res.evaled){
		res_info->bytesize  = size * get_bytesize(cl);
		res_info->alignment = get_alignment(cl);
	}

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
		if (cls[i].id != info->classes[i].id) return false;
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
	
	Class *res_classes = res_info->classes;
	uint32_t *res_offsets = (uint32_t *)(res_info->classes + cls_size);

	uint32_t bytesize = 0; 
	uint8_t max_alignment = 0;
	for (size_t i=0; i!=cls_size; i+=1){
		res_classes[i] = cls[i];
		res.infered |= cls[i].infered;
		res.evaled  |= cls[i].evaled;
		if (!res.infered && !res.evaled){
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





static const char *match_classes(
	Class source, Class *restrict target_ptr, ValueInfo *restrict infers
){
	Class target = *target_ptr;
	assert(!target.evaled);
	
	if (source.id == target.id) return NULL;

	size_t prefixes_size = 0;
	for (;;){
		uint32_t target_prefix = target.prefixes >> prefixes_size;
		if (target_prefix == 0) break;
		if ((source.prefixes ^ target_prefix) & PREFIX_TYPE_MASK)
			return "class prefix mismatch";
		if ((source.prefixes & ~target_prefix) & PREFIX_CONST_MASK)
			return "cannot pass const to a non const argument";
		source.prefixes >>= PREFIX_SIZE;
		prefixes_size += PREFIX_SIZE;
		uint64_t source_prefix_type = source.prefixes & PREFIX_TYPE_MASK;
		if (source.tag == Class_Void && source_prefix_type == ClassPrefix_Pointer){
			if (target.infered){
				if ((target.prefixes >> prefixes_size) != 0 || target.tag != Class_Infered)
					return "cannot infer argument from void pointer";
				break; // otherwise infer pointer to Void
			}
			return NULL;
		}
	}

	switch (target.tag){
	case Class_Infered:{
		if (source.tag == Class_Initlist || source.tag == Class_EnumLiteral)
			return "class cannot be infered from initlist or enum literal";
		if (target.param_id != 0){
			infers[target.param_id] = (ValueInfo){
				.clas = CLASS_CLASS, .flags = VF_Const, .data.clas = source
			};
		}
		source.prefixes = (source.prefixes << prefixes_size) | target.prefixes;
		*target_ptr = source;
		return NULL;
	}
	case Class_Void:{
		uint32_t last_prefix = target.prefixes >> (prefixes_size - PREFIX_SIZE);
		if ((last_prefix & PREFIX_TYPE_MASK) == ClassPrefix_Pointer) return NULL;
		break;
	}
	case Class_Array:{
		if (source.tag != Class_Array) break;
		if (source.prefixes != 0)
			return "class prefix mismatch";
		const ArrayClassInfo *target_info = array_class_info(target.idx);
		const ArrayClassInfo *source_info = array_class_info(target.idx);
	
		uint32_t target_size = target_info->size;
		Class target_arg = target_info->arg_class;
		const char *err = match_classes(source_info->arg_class, &target_arg, infers);	
		if (err != NULL) return err;
		if (target_size & ARRAY_SIZE_TAG_INFERED){
			target_size = source_info->size;
		} else if (target_size != source_info->size)
			return "array size mismatch";
		if (target_size != source_info->size || target_arg.id != source_info->arg_class.id){
			source = get_array_class(target_arg, target_size);
		}
		source.prefixes = target.prefixes;
		*target_ptr = source;
		return NULL;
	}
	case Class_Tuple:{
		if (source.tag != Class_Tuple) break;
		if (source.prefixes != 0)
			return "class prefix mismatch";
		const TupleClassInfo *target_info = tuple_class_info(target.idx);
		const TupleClassInfo *source_info = tuple_class_info(target.idx);
		uint32_t size = target_info->size;
		if (size == UINT32_MAX) goto ReturnTuple;
		Class *args = NULL;
		size_t saved_bc_size = global_bc_size;
		for (size_t i=0; i!=size; i+=1){
			Class arg = target_info->classes[i];
			const char *err = match_classes(source_info->classes[i], &arg, infers);	
			if (err != NULL){
				if (args != NULL){ global_bc_size = saved_bc_size; }
				return err;	
			}
			if (arg.id != source_info->classes[i].id){
				if (args == NULL){
					Class *args = (Class *)(global_bc + saved_bc_size);
					global_bc_size = saved_bc_size + size;
					memcpy(args, source_info->classes, size*sizeof(Class));
				}
				args[i] = arg;
			}
		}
		if (args != NULL){
			source = get_tuple_class(args, size);
			global_bc_size = saved_bc_size;
		}
	ReturnTuple:
		source.prefixes = target.prefixes;
		*target_ptr = source;
		return NULL;
	}
	default: break;
	}

	return "class mismatch";
}



static const char *eval_class(Class *restrict target_ptr, ValueInfo *restrict infers){
	Class target = *target_ptr;
	assert(target.evaled);
	
	uint64_t prefixes = target.prefixes;
	switch (target.tag){
	case Class_Variable:{
		if (target.param_id < 0){
			target = infers[-(1+target.param_id)].clas;
		} else{
			if (infers[target.param_id].clas.id != CLASS_CLASS.id)
				return "infered variable is not a class";
			target = infers[target.param_id].data.clas;
		}
		goto RestorePrefixes;
	}
	case Class_Expr:{
		// TODO: evaluete expression and assign result to target 
		assert(false && "infered variable expressions are not implemented");
		goto RestorePrefixes;
	}
	case Class_Array:{
		const ArrayClassInfo *info = array_class_info(target.idx);
		uint32_t size_value = info->size;
		uint32_t size_tag = size_value & ARRAY_SIZE_TAG_MASK;
		if ((size_tag == ARRAY_SIZE_TAG_VARIABLE) || (size_tag == ARRAY_SIZE_TAG_EXPR)){
			ValueInfo value;
			if (size_tag == ARRAY_SIZE_TAG_VARIABLE){
				int8_t var_index = (int8_t)(size_value & 0xff);
				if (var_index < 0)
					return "argument's class cannot be used to specify array's size";
				value = infers[var_index];
			} else{ // ARRAY_SIZE_TAG_EXPR case
				// TODO: evaluete expression and assign result to value
				assert(false && "infered variable expressions are not implemented");
			}
			if (value.clas.tag == Class_Integer){
				if (value.data.u64 & (1ul << (value.clas.basic_size*8ul-1ul)))
					return "array size cannot be negative";
			} else if (
				value.clas.tag != Class_Unsigned &&
				value.clas.tag != Class_Bool &&
				value.clas.tag != Class_Enum
			){
				return "arrays size must be an unsigned integer";
			}
			if (value.data.u64 > ARRAY_MAX_SIZE)
				return "arrays size cannot be that big";
			size_value = value.data.u64;
		}
		Class arg_class = info->arg_class;	
		if (arg_class.evaled){
			const char *err = eval_class(&arg_class, infers);
			if (err != NULL) return err;
		}
		target = get_array_class(arg_class, size_value);	
		goto Return;
	}
	case Class_Tuple:{
		const TupleClassInfo *info = tuple_class_info(target.idx);
		size_t saved_bc_size = global_bc_size;
		Class *arg_classes = (Class *)(global_bc + saved_bc_size);
		global_bc_size = saved_bc_size + info->size;
		memcpy(arg_classes, info->classes, info->size*sizeof(Class));
		for (size_t i=0; i!=info->size; i+=1){
			const char *err = eval_class(arg_classes+i, infers);
			if (err != NULL){ global_bc_size = saved_bc_size; return err; }
		}
		target = get_tuple_class(arg_classes, info->size);	
		global_bc_size = saved_bc_size;
		goto Return;
	}
	default: *(int *)0 = 0;
	}

	RestorePrefixes:{
		size_t prefixes_size = 0;
		while ((prefixes >> prefixes_size) != 0){ prefixes_size += PREFIX_SIZE; }
		prefixes |= (uint64_t)target.prefixes << prefixes_size;
		if (prefixes & ((1u << MAX_PREFIXES_SIZE)-1u))
			return "substituted class has too many prefixes";
	}

Return:
	target.evaled = false;
	target.prefixes = prefixes;
	*target_ptr = target;
	return NULL;
}


static const char *match_argument(
	ValueInfo *restrict arg, Class target, ValueInfo *restrict infers
){
	Class source = arg->clas;
	
	if (target.evaled){
		const char *err = eval_class(&target, infers);
		if (err != NULL) return err;
		arg->clas = target;
	}

	if (source.id == target.id) return NULL;

	if (source.id == CLASS_EMPTY_INITLIST.id && !target.infered){
		size_t bytesize = get_bytesize(target);
		if (bytesize <= sizeof(Data)){
			arg->data.u64 = 0;
		} else{
			arg->flags |= VF_Big;
			// TODO: assing index to zeroed data 
		}
		return NULL;
	}

	size_t prefixes_size = 0;
	for (;;){
		uint32_t target_prefix = target.prefixes >> prefixes_size;
		if (target_prefix == 0) break;
		if ((source.prefixes ^ target_prefix) & PREFIX_TYPE_MASK)
			return "class prefix mismatch";
		if ((source.prefixes & ~target_prefix) & PREFIX_CONST_MASK)
			return "cannot pass const to a non const argument";
		source.prefixes >>= PREFIX_SIZE;
		prefixes_size += PREFIX_SIZE;
		uint64_t source_prefix_type = source.prefixes & PREFIX_TYPE_MASK;
		if (source.tag == Class_Void && source_prefix_type == ClassPrefix_Pointer){
			if (target.infered){
				if ((target.prefixes >> prefixes_size) != 0 || target.tag != Class_Infered)
					return "cannot infer argument from void pointer";
				break; // otherwise infer pointer to Void
			}
			return NULL;
		}
	}

	arg->clas = target;

	switch (target.tag){
	case Class_Infered:{
		if (source.tag == Class_Initlist || source.tag == Class_EnumLiteral)
			return "class cannot be infered from initlist or enum literal";
		if (target.param_id != 0){
			infers[target.param_id] = (ValueInfo){
				.clas = CLASS_CLASS, .flags = VF_Const, .data.clas = source
			};
		}
		source.prefixes = (source.prefixes << prefixes_size) | target.prefixes;
		arg->clas = source;
		return NULL;
	}

	case Class_Void:{
		uint32_t last_prefix = target.prefixes >> (prefixes_size - PREFIX_SIZE);
		if ((last_prefix & PREFIX_TYPE_MASK) == ClassPrefix_Pointer) return NULL;
		break;
	}

	case Class_Unsigned:{
		if (source.prefixes != 0)
			return "class prefix mismatch";
		if (source.tag == Class_Unsigned){
			if (arg->flags & VF_Const){
				if (source.basic_size > target.basic_size){
					assert(target.basic_size < 8);
					uint64_t max_unsigned_target = (1ul << target.basic_size*8ul) - 1ul;
					if (arg->data.u64 > max_unsigned_target)
						return "value cannot be represented by smaller unsigned integer";
				}
			} else{
				if (source.basic_size > target.basic_size)
					return "assigning bigger unsigned integer to smaller one is not permitted";
				// TODO: insert zero extend node
			}
			return NULL;
		}
		if (source.tag == Class_Integer && (arg->flags & VF_Const)){
			uint64_t max_signed_target = (1ul << (target.basic_size*8ul-1ul)) - 1ul;
			if (arg->data.u64 > max_signed_target)
				return "value cannot be represented by targeted unsigned integer";
			return NULL;
		}
		if (source.tag == Class_Bool){
			if (!(arg->flags & VF_Const)){
				// TODO: insert zero extend node
			}
			return NULL;
		}
		if (source.tag == Class_Enum){
			uint64_t max_unsigned_target = UINT64_MAX >> (64u - target.basic_size*8u);
			if (arg->flags & VF_Const){
				if (arg->data.u64 > max_unsigned_target)
					return "enum's value is too big to be represented by the targeted unsigned";
			} else{
				const EnumClassInfo *source_info = enum_class_info(source.idx);
				if (source_info->max_value > max_unsigned_target)
					return "enum's values cannot be represented by the targeted unsigned";
				// TODO: insert zero extend node
			}
			return NULL;
		}
		break;
	}

	case Class_Integer:{
		if (source.prefixes != 0)
			return "class prefix mismatch";
		if (source.tag == Class_Integer){
			if (arg->flags & VF_Const){
				uint64_t source_signbit = 1ul << (source.basic_size*8ul-1ul);
				if (source.basic_size > target.basic_size){
					uint64_t max_target = (1ul << (target.basic_size*8ul-1ul)) - 1ul;
					uint64_t value = arg->data.u64;
					bool neg = value & source_signbit;
					if ((neg ? (~value & (source_signbit-1ul)) : value) > max_target)
						return "value cannot be represented by smaller integer";
				} else{
					arg->data.u64 |= -(arg->data.u64 & source_signbit);
				}
			} else{
				if (source.basic_size > target.basic_size)
					return "assigning bigger unsigned integer to smaller one is not permitted";
				// TODO: insert sign extend node
			}
			return NULL;
		}
		if (source.tag == Class_Unsigned){
			if (arg->flags & VF_Const){
				uint64_t max_signed_target = (1ul << (target.basic_size*8ul-1ul)) - 1ul;
				if (arg->data.u64 > max_signed_target)
					return "value cannot be represented by targeted integer";
			} else{
				if (source.basic_size > target.basic_size)
					return "assigning bigger unsigned integer to smaller one is not permitted";
				// TODO: insert zero extend node
			}
			return NULL;
		}
		if (source.tag == Class_Bool){
			if (!(arg->flags & VF_Const)){
				// TODO: insert zero extend node
			}
			return NULL;
		}
		if (source.tag == Class_Enum){
			uint64_t max_signed_target = (1ul << (target.basic_size*8ul-1ul)) - 1ul;
			if (arg->flags & VF_Const){
				if (arg->data.u64 > max_signed_target)
					return "enum's value is too big to be represented by the targeted integer";
			} else{
				const EnumClassInfo *source_info = enum_class_info(source.idx);
				if (source_info->max_value > max_signed_target)
					return "enum's values cannot be represented by the targeted integer";
				// TODO: insert zero extend node
			}
			return NULL;
		}
		break;
	}

	case Class_Enum:{
		if (source.prefixes != 0)
			return "class prefix mismatch";
		if (target.tag != Class_EnumLiteral) break;
		const EnumClassInfo *target_info = enum_class_info(target.idx);
		size_t elem_count = target_info->elem_count;
		size_t bytesize = target_info->basic_size;
		assert(bytesize <= 8);
		for (size_t i=0; i!=elem_count; i+=1){
			if (target_info->names[i] == source.name_id){
				arg->data.u64 = 0;
				const uint8_t *values = (const uint8_t *)(target_info->names + elem_count);
				memcpy(&arg->data.u64, values + i*bytesize, bytesize);
				return NULL;
			}
		}
		return "enum literal cannot represent any of targeted enum's values";
	}

	case Class_Array:{
		const ArrayClassInfo *target_info = array_class_info(target.idx);
		uint32_t target_size;
		Class target_arg = target_info->arg_class;
		Class source_arg;
		uint32_t source_size;
		if (source.prefixes != 0){
			if (!class_is_span(source)) return "class prefix mismatch";
			source.prefixes >>= PREFIX_SIZE;
			if (!(arg->flags & VF_Const))
				return "non compile time span cannot be assigned to array";
			source_arg = source;
			CtSpan64 span_data;
			memcpy(&span_data, global_bc + arg->data_idx, sizeof(CtSpan64));
			source_size = span_data.size;
			if (source_size > ARRAY_MAX_SIZE)
				return "span's size is too bit to be represented by array class";
			// comptime data case: just refer to data indexed by span
			// runtime data case: comptime would refer to static data, so insert
			//   load static variable node
		}
		if (source.tag == Class_Array){
			const ArrayClassInfo *source_info = array_class_info(target.idx);
			source_arg  = source_info->arg_class;
			source_size = source_info->size;
		}
		if (source.tag == Class_Initlist){
			
			break;
		}
		const char *err = match_classes(source_arg, &target_arg, infers);	
		if (err != NULL) return err;
		if (target_size & ARRAY_SIZE_TAG_INFERED){
			target_size = source_size;
		} else if (target_size != source_size){
			return "array size mismatch";
		}
		if (source.tag != Class_Array || target_arg.id != source_arg.id){
			source = get_array_class(target_arg, target_size);
		}
		source.prefixes = target.prefixes;
		arg->clas = source;
		return NULL;
	}

	case Class_Tuple:{
		if (source.tag != Class_Tuple) break;
		if (source.prefixes != 0)
			return "class prefix mismatch";
		const TupleClassInfo *target_info = tuple_class_info(target.idx);
		const TupleClassInfo *source_info = tuple_class_info(target.idx);
		uint32_t size = target_info->size;
		if (size == UINT32_MAX) goto ReturnTuple;
		Class *args = NULL;
		size_t saved_bc_size = global_bc_size;
		for (size_t i=0; i!=size; i+=1){
			Class arg = target_info->classes[i];
			const char *err = match_classes(source_info->classes[i], &arg, infers);	
			if (err != NULL){
				if (args != NULL){ global_bc_size = saved_bc_size; }
				return err;	
			}
			if (arg.id != source_info->classes[i].id){
				if (args == NULL){
					Class *args = (Class *)(global_bc + saved_bc_size);
					global_bc_size = saved_bc_size + size;
					memcpy(args, source_info->classes, size*sizeof(Class));
				}
				args[i] = arg;
			}
		}
		if (args != NULL){
			source = get_tuple_class(args, size);
			global_bc_size = saved_bc_size;
		}
	ReturnTuple:
		source.prefixes = target.prefixes;
		//*target_ptr = source;
		return NULL;
	}

	default: break;
	}

	return "class mismatch";
}




