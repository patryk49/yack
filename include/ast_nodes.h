#include "utils.h"


#define NODE_LIST \
/* KEYWORDS FOR CONTROL FLOW */ \
	X(If,       255,  0, 1, 1), \
	X(Else,     255,  0, 1, 1), \
	X(While,    255,  0, 1, 1), \
	X(For,      255,  0, 1, 1), \
	X(Break,    255,  0, 1, 1), \
	X(Continue, 255,  0, 1, 1), \
	X(Goto,     255,  0, 1, 1), \
	X(Return,   255, 16, 1, 1), \
	X(Defer,    255,  0, 1, 1), \
	X(With,     255, 12, 1, 1), \
\
/* KEYWORD OPERATIONS */ \
	X(Try,    0, 0, 1, 1), \
	X(Assert, 0, 0, 1, 1), \
\
/* DIRECTIVES */ \
	X(Import, 0, 0, 1, 1), \
\
/* POSTFIX OPERATORS */ \
	X(Dereference,    140, 255, 1, 1), \
	X(GetField,       140, 255, 2, 2), \
	X(Span,           140, 255, 1, 1), \
	X(Call,           140,   0, 1, 1), \
	X(GetProcedure,   140,   0, 1, 1), \
	X(Subscript,      140,   0, 1, 1), \
	X(FieldSubscript, 140,   0, 1, 1), \
	X(Initialize,     130,   0, 1, 1), \
	X(OpenScope,       30,   0, 1, 1), \
\
/* PREFIX OPERATORS */ \
	X(Plus,           255, 120, 1, 1), \
	X(Minus,          255, 120, 1, 1), \
	X(LogicNot,       255, 120, 1, 1), \
	X(BitNot,         255, 120, 1, 1), \
	X(Pointer,        255, 120, 1, 1), \
	X(DoublePointer,  255, 120, 1, 1), \
	X(Vectorize,      255, 120, 1, 1), \
	X(SpanClass,      255, 132, 1, 1), \
	X(ArrayClass,     255, 132, 1, 1), \
	X(ProcedureClass, 255, 132, 0, 1), \
	X(Procedure,      255,  20, 0, 1), \
\
/* CLOSING SYMBOLS */ \
	X(EndScope,   10, 0, 1, 1), \
\
/* OPENING SYMBOLS */ \
	X(OpenPar,            140, 0, 1, 1), \
	X(OpenProcedure,      140, 0, 1, 1), \
	X(OpenProcedureClass, 140, 0, 1, 1), \
	X(OpenBrace,           30, 0, 1, 1), \
	X(OpenBlock,          255, 0, 1, 1), \
\
/* BINARY ASSIGNMENTS */ \
	X(Assign,             40, 40, 1, 1), \
\
	X(ConcatAssign,       40, 40, 1, 1), \
\
	X(ModuloAssign,       40, 40, 1, 1), \
	X(AddAssign,          40, 40, 1, 1), \
	X(SubtractAssign,     40, 40, 1, 1), \
	X(CrossProductAssign, 40, 40, 1, 1), \
	X(MultiplyAssign,     40, 40, 1, 1), \
	X(DivideAssign,       40, 40, 1, 1), \
	X(PowerAssign,        40, 40, 1, 1), \
\
	X(BitOrAssign,        40, 40, 1, 1), \
	X(BitAndAssign,       40, 40, 1, 1), \
	X(BitXorAssign,       40, 40, 1, 1), \
	X(ShiftLeftAssign,    40, 40, 1, 1), \
	X(ShiftRightAssign,   40, 40, 1, 1), \
\
/* BINARY OPERATIONS */ \
	X(LogicOr,      60, 60, 1, 1), \
	X(LogicAnd,     62, 62, 1, 1), \
	X(Equal,        64, 64, 1, 1), \
	X(Less,         66, 66, 1, 1), \
	X(Greater,      66, 66, 1, 1), \
	X(Contains,     68, 68, 1, 1), \
\
	X(Range,        53, 53, 1, 1), \
	X(Concat,       54, 54, 1, 1), \
\
	X(Modulo,       70, 70, 1, 1), \
	X(Add,          72, 72, 1, 1), \
	X(Subtract,     72, 72, 1, 1), \
	X(CrossProduct, 76, 76, 1, 1), \
	X(Multiply,     78, 78, 1, 1), \
	X(Divide,       78, 78, 1, 1), \
	X(Power,        81, 80, 1, 1), \
\
	X(BitOr,        88, 88, 1, 1), \
	X(BitAnd,       90, 90, 1, 1), \
	X(BitXor,       92, 92, 1, 1), \
	X(ShiftLeft,    94, 94, 1, 1), \
	X(ShiftRight,   94, 94, 1, 1), \
\
	X(Cast,         99, 98, 1, 1), \
	X(Reinterpret,  99, 98, 1, 1), \
\
	X(Pipe,         50, 50, 1, 1), \
\
/* OTHER EXPRESSINS */ \
	X(Variable,       11, 36, 2, 2), \
\
/* SEPARATORS */ \
	X(Semicolon,     10, 0, 1, 1), \
	X(Terminator,    10, 0, 1, 1), \
	X(Comma,         18, 0, 1, 1), \
	X(DefaultParam, 255, 19, 0, 1), \
\
	X(StartScope, 10, 0, 1, 1), \
\
/* VALUES */ \
	X(Identifier,  255, 0, 2, 2), \
	X(EnumLiteral, 255, 0, 2, 2), \
	X(Unsigned,    255, 0, 2, 2), \
	X(Float32,     255, 0, 2, 2), \
	X(Float64,     255, 0, 2, 2), \
	X(Character,   255, 0, 2, 2), \
	X(String,      255, 0, 2, 2), \
\
/* SMALL VALUES */ \
	X(Self,    255, 0, 1, 1), \
	X(Infered, 255, 0, 1, 1), \
	X(Ignored, 255, 0, 1, 1), \
\
/* BIG VALUES */ \
	X(Tuple, 255, 0, 1, 1), \
\
/* UNDECIDED */ \
	X(Splat,     255,   0, 1, 1), \
	X(Pound,     255,   0, 1, 1), \
\
/* AFTER PARSING */ \
	X(ProcPointer, 255, 255, 0, 1), \
	X(Initializer, 255, 255, 0, 1), \
	X(Nop,         255, 255, 1, 1), \
	X(Error,       255, 255, 0, 1), \
	X(Value,       255, 255, 0, 1), \
	X(ReturnClass, 255, 255, 0, 1)






// define enum
#define X(name, prec_left, prec_right, ts, ns) Ast_##name
enum AstType{ NODE_LIST };
#undef X


// define name strings
#define X(name, prec_left, prec_right, ts, ns) #name
static const char *const AstTypeNames[] = { NODE_LIST };
#undef X


// define left precedence table
#define X(name, prec_left, prec_right, ts, ns) prec_left
static const uint8_t PrecsLeft[] = { NODE_LIST };
#undef X


// define right precedence table
#define X(name, prec_left, prec_right, ts, ns) prec_right
static const uint8_t PrecsRight[] = { NODE_LIST };
#undef X


// define token size table
#define X(name, prec_left, prec_right, ts, ns) ts 
static const uint8_t TokenSizes[] = { NODE_LIST };
#undef X

// define ast node size table
#define X(name, prec_left, prec_right, ts, ns) ns 
static const uint8_t AstNodeSizes[] = { NODE_LIST };
#undef X



// keywords
#define AST_KW_START Ast_If
#define AST_KW_END   Ast_Import

static uint64_t KeywordNamesU64[AST_KW_END-AST_KW_START+1];

static void init_keyword_names(void){
	for (size_t i=AST_KW_START; i<=AST_KW_END; i+=1){
		uint64_t kw = AstTypeNames[i][0] + ('a'-'A');
		for (size_t j=1; AstTypeNames[i][j]!='\0'; j+=1){
			kw |= (uint64_t)AstTypeNames[i][j] << (j*8);
		}
		KeywordNamesU64[i-AST_KW_START] = kw;
	}
}
