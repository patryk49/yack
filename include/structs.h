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

#define MAX_INFERED_COUNT 16



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
	Class_Initlist,
	Class_Bytes,
	Class_Class,
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



typedef struct{
	uint8_t var_index;
	bool is_reference    : 1;
	bool is_field_access : 1;
	uint16_t field_index;
} InferedInfo;


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
			uint32_t    idx;
			uint32_t    bytesize; // for bytes
			NameId      name_id;  // for enum literal
			InferedInfo infro;
		};
	};
} Class;



// BASIC CLASSES
#define BASIC_CLASS(arg_tag, arg_infered, arg_basic_size, arg_basic_alignment) (Class){ \
	.tag             = arg_tag, \
	.infered         = arg_infered, \
	.basic_size      = arg_basic_size, \
	.basic_alignment = arg_basic_alignment \
}

#define CLASS_VOID          BASIC_CLASS(Class_Void,         false, 0, 0)
#define CLASS_ERROR         BASIC_CLASS(Class_Error,        false, 0, 0)
#define CLASS_CLASS         BASIC_CLASS(Class_Class,        false, 8, 3)
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



typedef union ClassInfoHeader{
	struct{
		uint32_t bytesize  : 24;
		uint8_t  alignment :  8;
	};
	uint64_t _union_field_that_exists_in_here_for_alignment_purpuses;
} ClassInfoHeader;

typedef struct ArrayClassInfo{
	uint32_t bytesize  : 24;
	uint8_t  alignment :  8;
	int32_t  size; // negative means infered size
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
	bool     has_single_implementation;

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
	
	Class   arg_classes[]; // variable size field
// NameId  arg_name_ids[]; // pretend it exists
// BcNode  defaults[]; // pretend it exists
} ProcedureClassInfo;


typedef struct DefaultsInfo{
	Class    *cls;
	uint32_t *bcs;
} DefaultsInfo;





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

typedef struct{
	uint32_t index;
	uint32_t size;
} StaticBufInfo;

typedef union Data{
	uint8_t  bytes[8];
	uint64_t u64;
	uint32_t code;
	float    f32;
	double   f64;
	Class    clas;
	void    *ptr;
	const char *text;
	NameId name_id;
	StaticBufInfo bufinfo;
} Data;

typedef union AstNode{
	struct{
		enum AstType type : 8;
		uint8_t  flags;
		uint16_t count;
		uint32_t pos;
	};
	Data data;
} AstNode;





// SEMENTIC ALALYSIS REALTED STRUCTS
enum ValueFlags{
	VF_Const  = 0,
	VF_Big,
	VF_Dependant,
};


typedef struct{
	Class clas;
	uint32_t ast_index;
	uint32_t flags;
	union{
		Data data;
		struct{
			uint32_t data_idx;
			uint32_t node_idx;
		};
	};
} ValueInfo;


enum ScopeType{
	Scope_Global,
	Scope_Proc,
	Scope_Block,
	Scope_If,
	Scope_Else,
	Scope_For,
	Scope_While
};

typedef struct{
	enum ScopeType type : 8;
	uint32_t count : 24;
	uint32_t bc_start;
	uint32_t value_start;
	uint32_t variable_start;
} ScopeInfo;

typedef struct{
	uint32_t name_id;
	uint32_t bc_pos;
} VariableInfo;





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
} BcNode;


typedef struct BcProcHeader{
	uint32_t body_size;
	uint8_t arg_count;
	uint8_t use_count;
	bool is_inline : 1;
	bool is_pure   : 1;
	bool is_const  : 1;
} BcProcHeader;

