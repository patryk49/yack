#include "utils.h"


#define NODE_LIST \
/* KEYWORDS FOR CONTROL FLOW */ \
	X(If,       0, 0), \
	X(Else,     0, 0), \
	X(While,    0, 0), \
	X(For,      0, 0), \
	X(Break,    0, 0), \
	X(Continue, 0, 0), \
	X(Goto,     0, 0), \
	X(Return,   0, 0), \
	X(Defer,    0, 0), \
	X(With,     0, 0), \
\
/* KEYWORD OPERATIONS */ \
	X(Try,    0, 0), \
	X(Assert, 0, 0), \
\
/* DIRECTIVES */ \
	X(Import, 0, 0), \
\
/* CLOSING SYMBOLS */ \
	X(ClosePar,     10, 0), \
	X(CloseBracket, 10, 0), \
	X(CloseBrace,   10, 0), \
\
/* OPENING SYMBOLS */ \
	X(OpenPar,        140, 0), \
	X(OpenBracket,    140, 0), \
	X(OpenBrace,       30, 0), \
	X(OpenDotPar,     140, 0), \
	X(OpenDotBracket, 140, 0), \
	X(OpenDotBrace,   110, 0), \
\
/* POSTFIX OPERATORS */ \
	X(Dereference,    140, 0), \
	X(GetField,       140, 0), \
	X(Span,           140, 0), \
	X(Subscript,      140, 0), \
	X(Call,           140, 0), \
	X(GetProcedure,   140, 0), \
	X(FieldSubscript, 140, 0), \
	X(Initialize,     130, 0), \
	X(OpenScope,       30, 0), \
	X(Condition,       59, 0), \
\
/* PREFIX OPERATORS */ \
	X(Plus,          255, 120), \
	X(Minus,         255, 120), \
	X(LogicNot,      255, 120), \
	X(BitNot,        255, 120), \
	X(Pointer,       255, 120), \
	X(DoublePointer, 255, 120), \
	X(SpanClass,     255, 132), \
	X(ArrayClass,    255, 132), \
	X(Arrow,          10,  10), \
	X(DoubleArrow,   255,  51), \
\
/* BINARY ASSIGNMENTS */ \
	X(Assign,             40, 40), \
\
	X(ConcatAssign,       40, 40), \
\
	X(ModuloAssign,       40, 40), \
	X(AddAssign,          40, 40), \
	X(SubtractAssign,     40, 40), \
	X(CrossProductAssign, 40, 40), \
	X(MultiplyAssign,     40, 40), \
	X(DivideAssign,       40, 40), \
	X(PowerAssign,        40, 40), \
\
	X(BitOrAssign,        40, 40), \
	X(BitAndAssign,       40, 40), \
	X(BitXorAssign,       40, 40), \
	X(ShiftLeftAssign,    40, 40), \
	X(ShiftRightAssign,   40, 40), \
\
/* BINARY OPERATIONS */ \
	X(LogicOr,      60, 60), \
	X(LogicAnd,     62, 62), \
	X(Equal,        64, 64), \
	X(Less,         66, 66), \
	X(Greater,      66, 66), \
	X(Contains,     68, 68), \
\
	X(Range,        53, 53), \
	X(Concat,       54, 54), \
\
	X(Modulo,       70, 70), \
	X(Add,          72, 72), \
	X(Subtract,     72, 72), \
	X(CrossProduct, 76, 76), \
	X(Multiply,     78, 78), \
	X(Divide,       78, 78), \
	X(Power,        81, 80), \
\
	X(BitOr,        88, 88), \
	X(BitAnd,       90, 90), \
	X(BitXor,       92, 92), \
	X(ShiftLeft,    94, 94), \
	X(ShiftRight,   94, 94), \
\
	X(Cast,         99, 98), \
	X(Reinterpret,  99, 98), \
\
	X(Pipe,         50, 50), \
\
/* OTHER EXPRESSINS */ \
	X(Variable,       6, 36), \
	X(VariableSymbol, 6, 36), \
\
/* SEPARATORS */ \
	X(Semicolon,   5, 0), \
	X(Terminator,  5, 0), \
	X(Comma,       5, 0), \
	X(CloseScope,  5, 0), \
\
	X(StartScope, 5, 0), \
	X(EndScope,   5, 0), \
\
/* VALUES */ \
	X(Identifier,  255, 0), \
	X(EnumLiteral, 255, 0), \
	X(Unsigned,    255, 0), \
	X(Float32,     255, 0), \
	X(Float64,     255, 0), \
	X(Character,   255, 0), \
	X(String,      255, 0), \
\
/* SMALL VALUES */ \
	X(Self,    255, 0), \
	X(Infered, 255, 0), \
	X(Ignored, 255, 0), \
\
/* BIG VALUES */ \
	X(Tuple, 255, 0), \
\
/* UNDECIDED */ \
	X(Splat, 255, 0), \
	X(Pound, 255, 0), \
\
/* AFTER PARSING */ \
	X(ProcPointer, 255, 0), \
	X(Initializer, 255, 0), \
	X(Procedure,   255, 0), \
	X(Nop,         255, 0), \
	X(Error,       255, 0), \
	X(Value,       255, 0)






// define enum
#define X(name, prec_left, prec_right) Ast_##name
enum AstType{ NODE_LIST };
#undef X


// define name strings
#define X(name, prec_left, prec_right) #name
static const char *const AstTypeNames[] = { NODE_LIST };
#undef X


// define left precedence table
#define X(name, prec_left, prec_right) prec_left
static const uint8_t PrecsLeft[] = { NODE_LIST };
#undef X


// define right precedence table
#define X(name, prec_left, prec_right) prec_right
static const uint8_t PrecsRight[] = { NODE_LIST };
#undef X



// keyword strings table
static const char *KeywordNames[] = {
// KEYWORDS
	"if",       "else",      "while",       "for",
	"defer",    "return",
	"break",    "continue",  "goto",
	"assert",
	"do",
	
	"load",     "import",    "export",

	"struct",   "enum",      "bitfield",    "expr",

// DIRECTIVES
	"if",       "for",       "try",      // control statements
	"params",                            // module parameters declaration
	"asm",                               // inlineassembly
	"load",
	"assert",                            // assertion
	"at",                                // filed modifiers
	"size",     "aligning",  "const",    // data class information
	"fcount",   "fnames",    "cfnames",  // field information
	"dsalloc",  "cdsalloc",  "bssalloc", // data segment allocators
	"hot",      "cold",                  // compiler hints
	"run",      "pull",                  // expression modifiers
	"inl",      "noinl",     "callc",    // procedure flags

// BASIC CONSTANTS
	"Class",  "Null",   "True", "False",

// BASIC COMPILE TIME INFORMATION
	"class",   "isconst",
	"bytes",   "aligning",       "fcount",

// BASIC FUNCTIONS
	"abs",
	"min", "max",  "clamp",
};
