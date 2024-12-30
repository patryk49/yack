#pragma once

#include "utils.h"
#include "ast_nodes.h"



typedef uint32_t NameId;
typedef int32_t  VarIndex;



#if   UINTPTR_MAX == UINT64_MAX
	#define PTR_SIZE      8
	#define PTR_ALIGNMENT 3
#elif UINTPTR_MAX == UINT32_MAX
	#define PTR_SIZE      4
	#define PTR_ALIGNMENT 2
#elif UINTPTR_MAX == UINT16_MAX
	#define PTR_SIZE      2
	#define PTR_ALIGNMENT 1
#elif UINTPTR_MAX == UINT8_MAX
	#define PTR_SIZE      1
	#define PTR_ALIGNMENT 0
#else
	#error "unsupported architecture"
#endif

#if   SIZE_MAX == UINT64_MAX
	#define REG_SIZE      8
	#define REG_ALIGNMENT 3
#elif SIZE_MAX == UINT32_MAX
	#define REG_SIZE      4
	#define REG_ALIGNMENT 2
#elif SIZE_MAX == UINT16_MAX
	#define REG_SIZE      2
	#define REG_ALIGNMENT 1
#elif SIZE_MAX == UINT8_MAX
	#define REG_SIZE      1
	#define REG_ALIGNMENT 0
#else
	#error "unsupported architecture"
#endif

#define SPAN_SIZE (2*PTR_SIZE)


#define PREFIX_SIZE         4u
#define PREFIX_COUNT_MAX    6u
#define PREFIX_MASK         0xfu
#define PREFIX_HAS_PTR_MASK 0x888888u





// CLASSES
enum ClassPrefix{
	ClassPrefix_None    = 0,

	// the below prefixes contain pointers
	ClassPrefix_Pointer = 8,
	ClassPrefix_Span    = 9,
};

enum ClassTag{
	// basic classes
	Class_Void =  0,
	Class_Error, 
	Class_Infered,
	Class_Bytes,
	Class_Class,
	Class_InferedTuple,
	Class_EmptyTuple,
	Class_Unsigned,
	Class_Integer,
	Class_Bool,
	Class_Float,
	
	Class_Basic,

	// complex classes
	Class_Procedure,
	Class_Struct,
	Class_Array,
	Class_Tuple,
	Class_ProcPointer,
	Class_Enum,
	Class_StrideSpan,
	Class_NumberRange,
	Class_Module,
	Class_EnumLiteral
};


typedef union Class{
	uint64_t id;
	struct{
		enum ClassTag tag       :  6;
		bool          infered   :  1;
		bool          pointered :  1;
		uint32_t      prefixes  : 24;
		union{
			struct{
				uint16_t basic_size;
				uint8_t  basic_alignment;
				uint8_t  basic_flags;
			};
			uint32_t index;
			uint32_t bytesize; // for bytes
			NameId   name_id; // for enum literal
		};
	};
} Class;



typedef struct ClassInfoArray{
	Class    *data;
	uint32_t  size;
	uint32_t  capacity;
} ClassInfoArray;



// BASIC CLASSES
#define BASIC_CLASS(arg_tag, arg_infered, arg_basic_size, arg_basic_alignment) (Class){ \
	.tag             = arg_tag, \
	.infered         = arg_infered, \
	.basic_size      = arg_basic_size, \
	.basic_alignment = arg_basic_alignment \
}

#define CLASS_VOID          BASIC_CLASS(Class_Void,         false, 0, 0)
#define CLASS_ERROR         BASIC_CLASS(Class_Error,        false, 0, 0)
#define CLASS_INFERED       BASIC_CLASS(Class_Infered,      true,  0, 0)
#define CLASS_CLASS         BASIC_CLASS(Class_Class,        false, 8, 3)
#define CLASS_INFERED_TUPLE BASIC_CLASS(Class_InferedTuple, true,  0, 0)
#define CLASS_EMPTY_TUPLE   BASIC_CLASS(Class_EmptyTuple,   false, 0, 0)
#define CLASS_U8            BASIC_CLASS(Class_Unsigned,     false, 1, 0)
#define CLASS_U16           BASIC_CLASS(Class_Unsigned,     false, 2, 1)
#define CLASS_U32           BASIC_CLASS(Class_Unsigned,     false, 4, 2)
#define CLASS_U64           BASIC_CLASS(Class_Unsigned,     false, 8, 3)
#define CLASS_I8            BASIC_CLASS(Class_Integer,      false, 1, 0)
#define CLASS_I16           BASIC_CLASS(Class_Integer,      false, 2, 1)
#define CLASS_I32           BASIC_CLASS(Class_Integer,      false, 4, 2)
#define CLASS_I64           BASIC_CLASS(Class_Integer,      false, 8, 3)
#define CLASS_Bool          BASIC_CLASS(Class_Bool,         false, 1, 0)
#define CLASS_F32           BASIC_CLASS(Class_Float,        false, 4, 2)
#define CLASS_F64           BASIC_CLASS(Class_Float,        false, 8, 3)

#define CLASS_IPTR BASIC_CLASS(Class_Integer,  false, PTR_SIZE, PTR_ALIGNMENT)
#define CLASS_UPTR BASIC_CLASS(Class_Unsigned, false, PTR_SIZE, PTR_ALIGNMENT)

#define CLASS_ISIZE BASIC_CLASS(Class_Integer,  false, REG_SIZE, REG_ALIGNMENT)
#define CLASS_USIZE BASIC_CLASS(Class_Unsigned, false, REG_SIZE, REG_ALIGNMENT)

#define CLASS_VOID_PTR (Class){ \
	.tag = Class_Void, \
	.pointered = true, \
	.prefixes = ClassPrefix_Pointer \
}

#undef DEFINE_BASIC_CLASS


typedef struct ClassInfoHeader{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
} ClassInfoHeader;

typedef struct ArrayClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint32_t size;
	Class    arg_class;
} ArrayClassInfo;


typedef struct TupleClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint16_t size;
	uint16_t padding_0;
	Class    arg_classes[];
// uint32_t offsets[]; // pretend it exists
} TupleClassInfo;

typedef struct StrideSpanClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	int64_t  stride;
	Class    arg_class;
} StrideSpanClassInfo;


enum ProcFlags{
	ProcFlag_C = 0,
};

// has the same arrangement as TupleClassInfo to reuse some code
typedef struct ProcPointerClassInfo{
	uint32_t       bytesize  : 24;
	uint8_t        alignment :  8;
	uint16_t       size; // this fileld exists to reuse the tuple code
	uint8_t        arg_count;
	enum ProcFlags flags : 8;
	Class          arg_classes[];
} ProcPointerClassInfo;


// has the same arrangement as TupleClassInfo to reuse some code
typedef struct EnumClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint32_t elem_count;
	Class    base_class;
	NameId names[];
	// <- values are stored here
} EnumClassInfo;


typedef struct StructClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint16_t size;
// uint32_t offsets[]; // pretend it exists

	uint16_t stored_field_count;

	uint16_t field_count;
	uint16_t static_field_count;

	uint16_t module_id;
	uint8_t  state;
	uint8_t  name_length;

	char     *name;

	Class    arg_classes[];
// uint32_t offsets[];     // pretend it exists
// uint32_t field_names[]; // pretend it exists
} StructClassInfo;


typedef struct InstanceInfo{
	uint64_t hash;
	//struct IrProc *ir_proc;
	Class arg_classes[];
// IrNode *comptime_args[]; // pretend it exists
} InstanceInfo;


// structures that containt information abount non unique classes
typedef struct ProcedureClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint8_t  arg_count;
	uint8_t  default_count;
	uint8_t  state;

	uint16_t comptime_flags;

	uint8_t  infered_count;
	uint16_t module_id;

	// For each implementation of this procedure .istances stores:
	// Value struct that contains:
	// - return class
	// - coresponding ir_procedure or returned value in case when it's constant
	// - hash of ClassNode's and IrData's following the Value struct,
	//   that is stored at .position field
	// - and maybe something else
	// infered classes
	// IrNode's of constant arguments
	uint16_t instance_count;
	uint16_t instance_capacity;
	InstanceInfo *instances;
	
	uint32_t body_index;
	
	Class classes[]; // variable size field
// NameId  name_ids[]; // pretend it exists
// BcNode  defaults[]; // pretend it exists
} ProcedureClassInfo;


typedef struct DefaultsInfo{
	Class    *cls;
	uint32_t *bcs;
} DefaultsInfo;





// BYTECODE
enum BcType{
	// compile time values
	BC_Data    = 0, // needs class
	BC_Static  = 1, // needs class

	BC_GhostData, // needs class
	BC_RawData,

	BC_Zeros, // needs class

	// operations on variables
	BC_Push,  // needs class
	BC_Alloc, // needs class
	
	// operations on memory
	BC_MemLoad,  // needs class

// below operations does not require class information
	BC_MemStore,
	
	BC_MemCopy,
	BC_MemSet,
	BC_MemComp,

	BC_Load,
	BC_LoadField,
	BC_LoadIndex,

	BC_Store,
	BC_StoreField,
	BC_StoreIndex,
	
	BC_Reassign,
	BC_Pop,

	BC_Address,
	
	BC_Pack,
	BC_Merge, // simmilar to pack, but only pack at splitpoints
	
	// unary operations
	BC_Bitcast,
	BC_ZeroExtend,
	BC_SignExtend,
	BC_ConvertFloat,
	
	// reassigning operations used for implicit converions
	BC_ZeroExtendVariable,
	BC_SignExtendVariable,
};


enum BcFlags{
	BCF_Labeled = 1 << 0,
};



typedef struct SpanData{
	uint8_t  *ptr;
	uintptr_t size;
} SpanData;

typedef struct BcFmaInfo{
	uint32_t offset;
	uint32_t stride;
} BcFmaInfo;

typedef struct BcStoreInfo{
	uint32_t bytesize;
	uint32_t alignment;
} BcStoreInfo;

typedef struct BcNodeExtendInfo{
	uint8_t from;
	uint8_t to;
} BcNodeExtendInfo;

typedef struct BcVarInfo{
	uint32_t index;
	uint32_t bytesize;
} BcVarInfo;

typedef union BcData{
	uint64_t    u64;
	void       *ptr;
	float       f32;
	double      f64;
	uint8_t     bytes[8];
	Class       cl;
	uint8_t    *address;
	const char *error;
} BcData;

typedef union BcInfo{
	Class cl;
	uint8_t bytes[8];
	BcData data;
	uint8_t *address;
	size_t size;
	uint8_t *data_ptr;
	struct BcInfo_Pack{
		uint32_t count;
		uint32_t discard_count;
	} pack;
	BcVarInfo   var;
	BcFmaInfo   fma;
	BcStoreInfo store;
	struct BcInfo_Merge{
		uint16_t split_count;
		uint16_t splits[3];
	} merge;
} BcInfo;


typedef struct BcNode{
	enum BcType  type  : 8;
	enum BcFlags flags : 8;
	uint16_t     debug_info;
	uint32_t     size;
	BcInfo tail[];
} BcNode;


typedef struct BcProcHeader{
	uint32_t body_size;
	uint8_t arg_count;
	uint8_t use_count;
	bool is_inline : 1;
	bool is_pure   : 1;
	bool is_const  : 1;
} BcProcHeader;


typedef struct BcArray{
	BcNode  *data;
	uint32_t size;
	uint32_t capacity;
} BcArray;


enum NodeFlags{
	NodeFlag_Terminal         = 1 << 0,
	// flags for variables
	NodeFlag_Constant         = 1 << 1,
	NodeFlag_ClassSpec        = 1 << 2,
	NodeFlag_Initialized      = 1 << 3,
	NodeFlag_Parameter        = 1 << 4,
	NodeFlag_DestructureField = 1 << 5,
	NodeFlag_Destructure      = 1 << 6,
	
	// flags for control flow statements
	NodeFlag_Comptime         = 1 << 1,
	NodeFlag_HasIndex         = 1 << 2,
	NodeFlag_Hot              = 1 << 3,
	NodeFlag_Cold             = 1 << 4,

	// flags for operations
	NodeFlag_Negate           = 1 << 1,

	NodeFlag_Modulo           = 1 << 2,
	NodeFlag_Saturate         = 1 << 3,
	NodeFlag_FullOperation    = 1 << 4,
	
	// flags for other things
	NodeFlag_HasReturnClass   = 1 << 7,
	NodeFlag_Broadcasted      = 1 << 8,
//	NodeFlag_Aliasing         = 1 << 7,
//	NodeFlag_AfterColon       = 1 << 7
};


typedef struct StaticMemory{
	union{
		uint8_t *start;
		BcNode  *text;
	};
	Class   *classes;
	uint8_t *names;
	uint8_t *writable;
	uint8_t *end;

	BcNode  *text_top;
	Class   *classes_top;
	uint8_t *names_top;
	uint8_t *writable_top;
} StaticMemory;





// PARSE TREE
enum AstFlags{
	AstFlag_Negate      = 1 << 0,
	AstFlag_Constant    = 1 << 1,
	AstFlag_Initialized = 1 << 2,
	AstFlag_ClassSpec   = 1 << 3,
	AstFlag_ReturnSpec  = 1 << 4,
};

typedef union AstData{
	uint8_t  bytes[8];
	uint64_t u64;
	float    f32;
	double   f64;
	void    *ptr;
	const char *text;
	NameId name_id;
} AstData;

typedef struct AstNode{
	enum AstType type : 8;
	uint8_t  flags;
	uint16_t count;
	uint32_t pos;
	AstData tail[];
} AstNode;




