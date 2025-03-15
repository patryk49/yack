#include "utils.h"
#include "ast_nodes.h"

// this must be less or equal to 16
#define MAX_PARAM_COUNT 10

// this should be less or equal to 100 // not precisely
#define MAX_NAMED_INFERS 14

#define ARRAY_MAX_SIZE (1 << 24)
#define TUPLE_MAX_SIZE 1024


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


#define PREFIX_SIZE       4u
#define MAX_PREFIXES_SIZE 24u
#define MAX_PREFIX_COUNT  6u
#define PREFIX_MASK       0xfu
#define PREFIX_TYPE_MASK  0x7u
#define PREFIX_CONST_MASK 0x8u



// CLASSES
enum ClassPrefix{
	ClassPrefix_None    = 0,
	ClassPrefix_Pointer = 1,
	ClassPrefix_Span    = 2,
	
	ClassPrefix_Const = 8 // this is a flag 
};

enum ClassTag{
	// basic classes
	Class_Void =  0,
	Class_Error, 
	Class_Infered,
	Class_Variable,
	Class_Expr,
	Class_Class,
	Class_Bytes,
	Class_Unsigned,
	Class_Integer,
	Class_Bool,
	Class_Float,
	
	// compile time only
	Class_Initlist,
	Class_Initmap,
	Class_EnumLiteral,
	
	// builtin complex classes
	Class_Array,
	Class_Tuple,
	Class_ProcPointer,
	Class_StrideSpan,
	Class_NumberRange,

	//	user defined classes
	Class_Procedure,
	Class_Struct,
	Class_Enum,
	Class_Module
};

typedef union Class{
	uint64_t id;
	struct{
		enum ClassTag tag      :  6;
		bool          infered  :  1;
		bool          evaled   :  1;
		uint64_t      prefixes : 24;
		union{
			struct{
				uint16_t basic_size;
				uint8_t  basic_alignment;
				uint8_t  basic_flags;
			};
			uint32_t idx;
			int8_t   param_id;
			uint32_t bytesize; // for bytes
			NameId   name_id;  // for enum literal
		};
	};
} Class;



typedef struct{
	uint32_t index;
	uint32_t size;
} StaticBufInfo;

typedef union Data{
	uint8_t     bytes[8];
	uint64_t    u64;
	int64_t     i64;
	uint32_t    code;
	float       f32;
	double      f64;
	Class       clas;
	void       *ptr;
	const char *text;
	NameId      name_id;
	StaticBufInfo bufinfo;
} Data;



// BASIC CLASSES
#define BASIC_CLASS(arg_tag, arg_basic_size, arg_basic_alignment) ((Class){ \
	.tag             = arg_tag, \
	.basic_size      = arg_basic_size, \
	.basic_alignment = arg_basic_alignment \
})

#define CLASS_VOID          BASIC_CLASS(Class_Void,     0, 0)
#define CLASS_ERROR         BASIC_CLASS(Class_Error,    0, 0)
#define CLASS_CLASS         BASIC_CLASS(Class_Class,    8, 3)
#define CLASS_U8            BASIC_CLASS(Class_Unsigned, 1, 0)
#define CLASS_U16           BASIC_CLASS(Class_Unsigned, 2, 1)
#define CLASS_U32           BASIC_CLASS(Class_Unsigned, 4, 2)
#define CLASS_U64           BASIC_CLASS(Class_Unsigned, 8, 3)
#define CLASS_I8            BASIC_CLASS(Class_Integer,  1, 0)
#define CLASS_I16           BASIC_CLASS(Class_Integer,  2, 1)
#define CLASS_I32           BASIC_CLASS(Class_Integer,  4, 2)
#define CLASS_I64           BASIC_CLASS(Class_Integer,  8, 3)
#define CLASS_Bool          BASIC_CLASS(Class_Bool,     1, 0)
#define CLASS_F32           BASIC_CLASS(Class_Float,    4, 2)
#define CLASS_F64           BASIC_CLASS(Class_Float,    8, 3)

#define CLASS_IPTR BASIC_CLASS(Class_Integer,  PTR_SIZE, PTR_ALIGNMENT)
#define CLASS_UPTR BASIC_CLASS(Class_Unsigned, PTR_SIZE, PTR_ALIGNMENT)

#define CLASS_ISIZE BASIC_CLASS(Class_Integer,  REG_SIZE, REG_ALIGNMENT)
#define CLASS_USIZE BASIC_CLASS(Class_Unsigned, REG_SIZE, REG_ALIGNMENT)

#define CLASS_VOID_PTR ((Class){ \
	.tag = Class_Void, \
	.prefixes = ClassPrefix_Pointer \
})

#define CLASS_EMPTY_INITLIST ((Class){ .tag = Class_Initlist })

#undef DEFINE_BASIC_CLASS



typedef union ClassInfoHeader{
	struct{
		uint32_t bytesize  : 24;
		uint8_t  alignment :  8;
	};
	uint64_t _union_field_that_exists_in_here_for_alignment_purpuses;
} ClassInfoHeader;


#define ARRAY_SIZE_TAG_MASK     0xc0000000u
#define ARRAY_SIZE_TAG_INFERED  0x40000000u
#define ARRAY_SIZE_TAG_VARIABLE 0x80000000u
#define ARRAY_SIZE_TAG_EXPR     0xc0000000u

typedef struct{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint32_t size;
	Class    arg_class;
} ArrayClassInfo;

typedef struct{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint32_t size;
	Class    classes[];
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
typedef struct{
	uint32_t       bytesize  : 24;
	uint8_t        alignment :  8;
	uint16_t       size; // this fileld exists to reuse the tuple code
	uint8_t        arg_count;
	enum ProcFlags flags : 8;
	Class          classes[];
} ProcPointerClassInfo;


typedef struct{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint16_t elem_count;
	uint16_t basic_size;
	uint64_t max_value;
	NameId   names[];
// uint??_t values[]; <- pretend it exists
} EnumClassInfo;


typedef struct{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	uint16_t size;

	uint16_t stored_field_count;

	uint16_t field_count;
	uint16_t static_field_count;

	uint16_t module_id;
	uint8_t  state;
	uint8_t  name_length;

	char     *name;

	Class    classes[];
// uint32_t offsets[];     // pretend it exists
// uint32_t field_names[]; // pretend it exists
} StructClassInfo;


typedef struct{
	uint32_t bc_index;
	uint16_t hash;
	uint16_t data_size;
	Data data[];
} InstanceInfo;


// structures that containt information abount non unique classes
typedef struct{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;

	uint8_t param_count;
	uint8_t default_count;
	uint8_t named_infers_count;
	
	uint8_t defaults_offset; // offset to defaults 
	uint8_t comptime_count; // this is not used

	uint16_t comptime_flags; // marks compile time parameters
	uint16_t infered_flags; // marks parameter types with unnamed infers

	uint16_t module_id;

	InstanceInfo *instances;
	uint16_t instance_count;
	uint16_t instance_capacity;
	
	uint32_t ast_pos;
	NameId   name_id;

	uint32_t body_index;

	Class  classes[];   // variable size field
// NameId arg_name_ids[];  // pretend it exists
// NameId infered_names[]; // pretend it exists
// Value  arg_defaults[];  // pretend it exists // defaults can be inserted from the end
} ProcedureClassInfo;






// PARSE TREE
enum AstFlags{
	AstFlag_Final = 1 << 0,

// operator flags
	AstFlag_Negate = 1 << 3,

// variable flags
	AstFlag_Constant    = 1 << 3,
	AstFlag_Initialized = 1 << 4,
	AstFlag_ClassSpec   = 1 << 5,

// procedure flags
	AstFlag_DirectName = 1 << 3,
	AstFlag_ReturnSpec = 1 << 4,
};

typedef union AstNode{
	struct{
		enum AstType type : 8;
		uint8_t flags;
		union{
			uint16_t count;
			struct{ // for procedure node
				uint8_t param_count;
				uint8_t default_and_infered_size;
			};
		};
		uint32_t pos;
	};
	Data data;
} AstNode;





// SEMENTIC ALALYSIS REALTED STRUCTS
enum ValueFlags{
	VF_Const = 0,
	VF_Big,
	VF_Dependant,
	VF_Lvalue,
	VF_Uninitialized
};


typedef struct{
	Class clas;
	uint32_t ast_start; // index of first node of expression coresponding to this value
	uint32_t flags;
	union{
		Data data;
		struct{
			uint32_t data_idx;
			uint32_t node_idx;
		};
	};
} ValueInfo;

typedef struct{
	NameId name_id;
	uint32_t value_idx;
} VariableInfo;

enum ScopeType{
	Scope_Global,
	Scope_Params,
	Scope_Procedure,
	Scope_Block,
	Scope_If,
	Scope_Else,
	Scope_For,
	Scope_While
};

typedef struct{
	enum ScopeType type : 8;
	uint16_t module_id; // 0 means the current scope
	union{
		uint16_t count;
		struct{ uint8_t a; uint8_t b; } counts;	
	};
	uint32_t proc_idx;
	uint32_t vars_size;
	union{
		uint32_t openproc_idx;
	};
} ScopeInfo;



typedef struct{
	uint32_t ibase;
	uint32_t idata;
} CtPointer64;

typedef struct{
	CtPointer64 ptr;
	uint64_t    size;
} CtSpan64;



enum DataFlags{
	DataFlag_Pointered = 0
};

typedef struct{
	uint16_t ptr_bitset[1];
	uint8_t  flags;
	uint8_t  alignment;
	uint32_t bytesize;
} DataHeader;



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
	BCF_Labeled = 1 << 1,
};



typedef union{
	struct{
		enum BcType  type  : 8;
		enum BcFlags flags : 8;
		uint16_t count;
		uint32_t inext;
	};
	struct{ // used for freelist
		uint32_t size;
		uint32_t inext;
	} freenode;
	void *ptr;
	Class clas;
	Data data;
	DataHeader data_header;
} BcNode;
	

typedef struct BcProcHeader{
	uint32_t body_size;
	uint8_t arg_count;
	uint8_t use_count;
	bool is_inline : 1;
	bool is_pure   : 1;
	bool is_const  : 1;
} BcProcHeader;

