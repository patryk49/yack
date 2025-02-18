#include "utils.h"
#include "unicode.h"
#include "files.h"

#include <stdio.h>
#include <stdlib.h>

#include "classes.h"


static bool is_valid_name_char(char);
static bool is_valid_first_name_char(char);
static bool is_number(char);
static bool is_whitespace(char);

static uint32_t parse_character(const char **);

static enum AstType parse_number(void *data_res, const char **src_it);

static void print_codeline(const char *text, size_t position);

static void raise_error(const char *text, const char *msg, uint32_t pos);





// AST ARRAY DATA STRUCTURE
typedef struct AstArray{
	AstNode *data;
	union{
		struct{
			AstNode *end;
			AstNode *maxptr;
		};
		struct{
			const char *error;
			uint32_t position;
		};
	};
} AstArray;


static AstArray ast_array_new(size_t capacity){
	assert(capacity >= 32);
	AstArray arr;
	arr.data = malloc(capacity*sizeof(AstNode));
	if (arr.data == NULL){
		assert(false && "node array allocation failrule");
	}
	arr.end = arr.data;
	arr.maxptr = arr.data + capacity;
	return arr;
}

static AstArray ast_array_clone(AstArray ast){
	if (ast.data == NULL) return ast;
	AstArray res = {0};
	size_t size = ast.end - ast.data;
	res.data = malloc(size*sizeof(AstNode));
	if (res.data == NULL) return res;
	memcpy(res.data, ast.data, size*sizeof(AstNode));
	res.end = res.data + size;
	res.maxptr = res.data + size;
	return res;
}

static void ast_array_grow(AstArray *arr){
	size_t new_capacity = 2*(arr->maxptr - arr->data);
	size_t arr_size = arr->end - arr->data;
	arr->data = realloc(arr->data, new_capacity*sizeof(AstNode));
	if (arr->data == NULL){
		assert(false && "node array allocation failrule");
	}
	arr->end = arr->data + arr_size;
	arr->maxptr = arr->data + new_capacity;
}

static AstNode *ast_array_push(AstArray *arr, AstNode node){
	if (arr->end == arr->maxptr) ast_array_grow(arr);
	AstNode *res = arr->end;
	arr->end += 1;
	*res = node;
	return res;	
}

static AstNode *ast_array_push2(AstArray *arr, AstNode node, Data data){
	if (arr->end+2 > arr->maxptr) ast_array_grow(arr);
	AstNode *res = arr->end;
	arr->end += 2;
	*res = node;
	(res+1)->data = data;
	return res;	
}



static AstArray make_tokens(const char *input){
	const char *text_begin = input;
	AstArray res = ast_array_new(4096);
	ast_array_push(&res, (AstNode){ .type = Ast_Terminator });

#define RETURN_ERROR(arg_error, arg_position) { \
	res.error    = arg_error; \
	res.position = arg_position; \
	goto ReturnError; } \

	uint8_t scope_types[64];
	uint32_t scope_idxs[SIZE(scope_types)];
	size_t scope_count = 0;

#define PUSH_SCOPE(type) \
	if (scope_count == SIZE(scope_types)){ \
		RETURN_ERROR("too many nested scopes", position); \
	} else{ \
		scope_types[scope_count] = type; \
		scope_idxs[scope_count] = res.end - res.data; \
		scope_count += 1; \
	}
	size_t position = 0;

	AstNode *prev_token = res.data;

	AstNode curr;
	Data curr_data;

	for (;;){
		const char *prev_input = input;
		curr = (AstNode){ .pos = position };

		switch (*input){
		case '=': input += 1;
			if (*input == '='){
				input += 1;
				curr.type = Ast_Equal;
				goto AddToken;
			}
			if (*input == '>'){
				input += 1;
				if (prev_token->type != Ast_EndScope || scope_types[scope_count] != Ast_OpenPar)
					RETURN_ERROR("expected closing parenthesis before => symbol", position);
				res.data[scope_idxs[scope_count]].type = Ast_OpenProcedure;
				prev_token->pos = position;
				curr.type = Ast_Nop;
				memcpy(&curr_data, &curr, sizeof(AstNode));
				goto AddTokenWithData;
			}
			if (!is_whitespace(*(input-2))){
				enum AstType prev_type = prev_token->type;
				if (Ast_Concat <= prev_type && prev_type <= Ast_ShiftRight){
					prev_token->type = prev_type + (Ast_AddAssign - Ast_Add);
					goto SkipToken;
				}
			}
			curr.type = Ast_Assign;
			goto AddToken;

		case '+': input += 1;
			curr.type = Ast_Add;
			goto AddToken;

		case '-': input += 1;
			if (*input == '>'){
				input += 1;
				if (prev_token->type != Ast_EndScope || scope_types[scope_count] != Ast_OpenPar)
					RETURN_ERROR("expected closing parenthesis before -> symbol", position);
				res.data[scope_idxs[scope_count]].type = Ast_OpenProcedureClass;
				goto SkipToken;
			}
			curr.type = Ast_Subtract;
			goto AddToken;

		case '*': input += 1;
			if (*input == '*'){
				input += 1;
				curr.type = Ast_Power;
			} else{
				curr.type = Ast_Multiply;
			}
			goto AddToken;

		case '/': input += 1;
			if (*input == '/'){
				input += 1;
				while (*input!='\0' && *input!='\n') input += 1;
				goto SkipToken;
			}
			if (*input == '*'){
				size_t depth = 1;
				for (;;){
					input += 1;
					UNLIKELY if (*input == '\0')
						RETURN_ERROR("unfinished comment", position);
					
					if (*input=='*' && *(input+1)=='/'){
						input += 1;
						depth -= 1;
						if (depth == 0){
							input += 1;
							goto SkipToken;
						}
					} else if (input[0]=='/' && input[1]=='*'){
						input += 1;
						depth += 1;
					}
				}
			}
			curr.type = Ast_Divide;
			goto AddToken;

		case '%': input += 1;
			curr.type = Ast_Modulo;
			goto AddToken;

		case '|': input += 1;
			if (*input == '|'){
				input += 1;
				curr.type = Ast_LogicOr;
			} else if (*input == '>'){
				curr.type = Ast_Pipe;
				input += 1;
			} else{
				curr.type = Ast_BitOr;
			}
			goto AddToken;

		case '&': input += 1;
			if (*input == '&'){
				input += 1;
				curr.type = Ast_LogicAnd;
			} else{
				curr.type = Ast_BitAnd;
			}
			goto AddToken;

		case '~': input += 1;
			if (*input == '%' && *(input+1) == '~'){
				input += 2;
				curr.type = Ast_Reinterpret;
			} else{
				curr.type = Ast_Cast;
			}
			goto AddToken;

		case '<': input += 1;
			if (*input == '='){
				input += 1;
				curr.type  = Ast_Greater;
				curr.flags = AstFlag_Negate;
			} else if (*input == '>'){
				input += 1;
				curr.type = Ast_Concat;
			} else if (*input == '<'){
				input += 1;
				curr.type = Ast_ShiftLeft;
			} else{
				curr.type = Ast_Less;
			}
			goto AddToken;

		case '>': input += 1;
			if (*input == '='){
				input += 1;
				curr.type = Ast_Less;
				curr.flags = AstFlag_Negate;
			} else if (*input == '>'){
				input += 1;
				curr.type = Ast_ShiftRight;
			} else if (*input == '<'){
				input += 1;
				curr.type = Ast_CrossProduct;
			} else{
				curr.type = Ast_Greater;
			}
			goto AddToken;

		case '!': input += 1;
			curr.flags = AstFlag_Negate;
			if (*input == '='){
				input += 1;
				curr.type  = Ast_Equal;
			} else if (*input == '|'){
				input += 1;
				curr.type = Ast_LogicOr;
			} else if (*input == '&'){
				input += 1;
				curr.type = Ast_LogicAnd;
			} else if (*input == '@' && *(input+1) == '='){
				input += 2;
				curr.type = Ast_Contains;
			} else{
				curr.type = Ast_LogicNot;
			}
			goto AddToken;

		case '(': input += 1;
			PUSH_SCOPE(Ast_OpenPar);
			curr.type = Ast_OpenPar;
			if (prev_token->type == Ast_Identifier){ curr.flags = AstFlag_DirectName; }
			goto AddToken;

		case ')':{ input += 1;
			if (scope_count == 0)
				RETURN_ERROR("too many closing parenthesis", position);
			scope_count -= 1;
			enum AstType t = scope_types[scope_count];
			if (t != Ast_OpenPar && t != Ast_GetProcedure)
				RETURN_ERROR("mismatched parenthesis", position);
			curr.type = Ast_EndScope;
			goto AddToken;
		}
		
		case '{': input += 1;
			PUSH_SCOPE(Ast_OpenBrace);
			curr.type = Ast_OpenBrace;
			goto AddToken;
		
		case '}':{ input += 1;
			if (scope_count == 0)
				RETURN_ERROR("too many closing braces", position);
			scope_count -= 1;
			enum AstType t = scope_types[scope_count];
			if (t != Ast_OpenBrace && t != Ast_FieldSubscript)
				RETURN_ERROR("mismatched braces", position);
			curr.type = Ast_EndScope;
			goto AddToken;
		}
		
		case '[': input += 1;
			PUSH_SCOPE(Ast_Subscript);
			curr.type = Ast_Subscript;
			goto AddToken;
		
		case ']':{ input += 1;
			if (scope_count == 0)
				RETURN_ERROR("too many closing brackets", position);
			scope_count -= 1;
			enum AstType t = scope_types[scope_count];
			if (t != Ast_Subscript && t != Ast_FieldSubscript)
				RETURN_ERROR("mismatched brackets", position);
			curr.type = Ast_EndScope;
			goto AddToken;
		}

		case '^': input += 1;
			if (*input == '|'){
				curr.type  = Ast_BitOr;
				curr.flags = AstFlag_Negate;
				input += 1;
			} else if (*input == '&'){
				curr.flags = AstFlag_Negate;
				curr.type  = Ast_BitAnd;
				input += 1;
			} else{
				curr.type = Ast_BitXor;
			}
			goto AddToken;

		case '@': input += 1;
			if (*input == '='){
				curr.type = Ast_Contains;
				input += 1;
			} else{
				curr.type = Ast_Self;
			}
			goto AddToken;

		case '#': input += 1;
			curr.type = Ast_Pound;
			goto AddToken;

		case '$': input += 1;
			if (is_valid_first_name_char(*input)){
				size_t size = 1;
				while (is_valid_name_char(input[size])) size += 1;
				curr.type = Ast_NamedInfered;
				curr.count = size;
				curr_data.name_id = get_name_id(input, size);
				input += size;
				goto AddTokenWithData;
			}
			curr.type = Ast_Infered;
			goto AddToken;

		case ':':{
			input += 1;
			curr.type = Ast_Variable;
			if (*input == ':'){
				curr.flags = AstFlag_Constant | AstFlag_Initialized;
				input += 1;
			} else if (*input == '='){
				curr.flags = AstFlag_Initialized;
				input += 1;
			} else{
				curr.flags = AstFlag_ClassSpec;
			}

			if (prev_token->type == Ast_Identifier){
				curr.count = prev_token->count;
				*prev_token  = curr;
				goto SkipToken;
			}

			//if (res.ptr[iprev].type != Ast_CloseBrace)
			//	RETURN_ERROR("invalid variable syntax", position);
			
			RETURN_ERROR("invalid variable syntax", position);

			goto AddToken;
		}
		
		case ',': input += 1;
			curr.type = Ast_Comma;
			goto AddToken;

		case '.': input += 1;
			if (*input == '('){
				input += 1;
				PUSH_SCOPE(Ast_GetProcedure);
				curr.type = Ast_GetProcedure;
				goto AddToken;
			}
			if (*input == '['){
				input += 1;
				PUSH_SCOPE(Ast_FieldSubscript);
				curr.type = Ast_FieldSubscript;
				goto AddToken;
			}
			if (*input == '{'){
				input += 1;
				PUSH_SCOPE(Ast_Initialize);
				curr.type = Ast_Initialize;
				goto AddToken;
			}
			if (*input == '.'){
				input += 1;
				if (*input == '.'){
					input += 1;
					curr.type = Ast_Splat;
				} else{
					curr.type = Ast_Range;
				}
				goto AddToken;
			}
			if (is_number(*input)){
				input -= 1;
				curr.type = parse_number(&curr_data, &input);
				if (curr.type == Ast_Error)
					RETURN_ERROR("invalid floating point literal", position);
				goto AddTokenWithData;
			}
			if (!is_valid_first_name_char(*input))
				RETURN_ERROR("invalid token starting with dot", position);
			/* ADD GET FIELD TOKEN */ {
				size_t size = 1;
				while (is_valid_name_char(input[size])) size += 1;
				curr.type = Ast_GetField;
				curr.count = size;
				curr_data.name_id = get_name_id(input, size);
				input += size;
			}
			goto AddTokenWithData;

		case ';':
			input += 1;
			if (prev_token->type == Ast_Semicolon) goto SkipToken;
			curr.type = Ast_Semicolon;
			goto AddToken;
		
		case '\'':{
			input += 1;
			if (*input == '\''){
				curr.type = Ast_Dereference;
				goto AddToken;
			}

			const char *old_input = input;
			uint32_t c = parse_character(&input);

			if (c == UINT32_MAX || *input != '\''){
				input = old_input;
				curr.type = Ast_Dereference;
				goto AddToken;
			}
			input += 1;

			curr.type = Ast_Character;
			curr_data.code = c;
			goto AddTokenWithData;
		}
		
		case '\"':{
			input += 1;
			size_t data_size = 0;
			BcNode *dest_node = global_bc + global_bc_size;
			uint8_t *dest_data = (uint8_t *)(dest_node + 2);
			while (*input != '\"'){
				if (*input == '\0')
					RETURN_ERROR("end of file inside of string literal", input-text_begin);
				uint32_t c = parse_character(&input);
				if (c == UINT32_MAX)
					RETURN_ERROR("invalid character code", input-text_begin);
				size_t code_size = utf8_write(dest_data, c);
				dest_data += code_size;
				data_size += code_size;
			}
			*dest_data = '\0';
			input += 1;
			*dest_node = (BcNode){ .type = BC_Data, };
			(dest_node+1)->clas = (Class){ .tag = Class_Bytes, .bytesize = data_size };

			curr.type = Ast_String;
			curr_data.bufinfo.index = global_bc_size + 2;
			curr_data.bufinfo.size  = data_size;
			
			global_bc_size += 2 + (data_size + 2 + sizeof(BcNode) - 1)/sizeof(BcNode);
			goto AddTokenWithData;
		}

		case ' ':
		case '\t':
			while (input+=1, *input==' ' || *input=='\t');
			goto SkipToken;

		case '\n':{
			input += 1;
			enum AstType prev_type = prev_token->type;
			if (
				scope_count != 0 || (Ast_OpenPar <= prev_type && prev_type <= Ast_Semicolon)
			) goto SkipToken;
			curr.type = Ast_Semicolon;
			goto AddToken;
		}
		
		case '\0':
			curr.type = Ast_Terminator;
			curr.pos = position;
			ast_array_push(&res, curr);
			return res;

		case '_':
			if (!is_valid_name_char(*(input+1))){
				input += 1;
				curr.type = Ast_Ignored;
				goto AddToken;
			}
			FALLTHROUGH;

		default:{
			if (is_valid_first_name_char(*input)){
				size_t size = 1;
				while (is_valid_name_char(input[size])) size += 1;

				if (2 <= size && size <= 8){
					uint64_t text = 0;
					memcpy(&text, input, size);
					for (size_t i=AST_KW_START; i<=AST_KW_END; i+=1){
						if (text == KeywordNamesU64[i-AST_KW_START]){
							curr.type = (enum AstType)i;
							input += size;
							goto AddToken;
						}
					}
				}
				curr.type = Ast_Identifier;
				curr.count = size;
				curr_data.name_id = get_name_id(input, size);
				input += size;
				goto AddTokenWithData;
			}
			if (is_number(*input)){
				curr.type = parse_number(&curr_data, &input);
				if (curr.type == Ast_Error)
					RETURN_ERROR("invalid number literal", position);
				goto AddTokenWithData;
			}
			RETURN_ERROR("unrecognized token", position);
		}

	AddTokenWithData:
		prev_token = ast_array_push2(&res, curr, curr_data);
		goto SkipToken;
	AddToken:
		prev_token = ast_array_push(&res, curr);
	SkipToken:
		position += input - prev_input;
	}}
#undef PUSH_SCOPE 
#undef RETURN_ERROR
ReturnError:
	free(res.data);
	res.data = NULL;
	return res;
}





static AstArray parse_tokens(AstArray tokens){
	AstNode opers[512];
	size_t opers_size = 1;
	
	opers[0] = (AstNode){ .type = Ast_StartScope, .flags = 0xff };

	AstNode *it = tokens.data + 1;
	AstNode *res_it = it;

#define RETURN_ERROR(arg_error, arg_position) { \
	tokens = (AstArray){ .data=NULL, .error=arg_error, .position=arg_position }; \
	goto ReturnError; \
}

#define CHECK_OPER_STACK_OVERFLOW(arg_position) if (opers_size == SIZE(opers)){ \
	tokens.error    = "operator stack overflow"; \
	tokens.position = arg_position; \
	goto ReturnError; \
}

ExpectValue:{
		AstNode curr = *it;
		it += 1;
		
		switch (curr.type){
		case Ast_Terminator:
			goto EndOfFile;

		case Ast_EndScope:
			it -= 1;
			goto ExpectOperator;
		
		case Ast_Infered:
			goto ExpectOperator;

		case Ast_Unsigned:
		case Ast_Float32:
		case Ast_Float64:
		case Ast_Character:
		case Ast_String:
		SimpleLiteral:
			*res_it = curr;
			(res_it+1)->data = *(Data *)it;
			it += 1;
			res_it += 2;
			goto ExpectOperator;
		
		case Ast_Identifier:
			if (opers[opers_size-1].type == Ast_OpenProcedure){ curr.type = Ast_Variable; }
			goto SimpleLiteral;
		
		case Ast_NamedInfered:{
			if (opers[opers_size-1].type != Ast_OpenProcedure)
				RETURN_ERROR("named infered value was used in inapropriate context", curr.pos);
			uint8_t default_and_infered_size = opers[opers_size-1].flags;
			if (default_and_infered_size == 255) // prevent overflow 
				RETURN_ERROR("definitely too many infered values", curr.pos);
			opers[opers_size-1].flags = default_and_infered_size + 1;
			goto SimpleLiteral;
		}
		
		case Ast_GetField:
			curr.type = Ast_EnumLiteral;
			goto SimpleLiteral;

		case Ast_Ignored:
			*res_it = curr;
			res_it += 1;
			goto ExpectOperator;

		case Ast_BitNot:
		case Ast_LogicNot:
		case Ast_Vectorize:
		case Ast_Splat:
		case Ast_Return:
		SimplePrefixOperator:
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;

		case Ast_Add:
			curr.type = Ast_Plus;
			goto SimplePrefixOperator;
		case Ast_Subtract:
			curr.type = Ast_Minus;
			goto SimplePrefixOperator;
		case Ast_Multiply:
			curr.type = Ast_Pointer;
			goto SimplePrefixOperator;
		case Ast_Power:
			curr.type = Ast_DoublePointer;
			goto SimplePrefixOperator;
		case Ast_BitXor:
			curr.type = Ast_BitNot;
			goto SimplePrefixOperator;

		case Ast_Variable:{
			enum AstType t = opers[opers_size-1].type;
			if (t!=Ast_OpenProcedure && t!=Ast_With && t!=Ast_StartScope)
				RETURN_ERROR("variable cannot be defined in this context", curr.pos);
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			memcpy(opers+opers_size, it, sizeof(Data));
			opers_size += 1;
			it += 1;
			goto SimplePrefixOperator;
		}

		case Ast_OpenBrace:
			if (it->type == Ast_EndScope){
				it += 1;
				curr.type = Ast_Initializer;
				*res_it = curr; res_it += 1;
				goto ExpectOperator;
			}
			curr.count = 1;
			goto SimplePrefixOperator;

		case Ast_OpenPar:{
			if (it->type == Ast_EndScope)
				RETURN_ERROR("missing expression inside of parenthesis", curr.pos);
			curr.count = 0;
			goto SimplePrefixOperator;
		}

		case Ast_OpenProcedureClass:{
			if (it->type == Ast_EndScope){
				it += 1;
				curr.type = Ast_ProcedureClass;
				curr.count = 0;
				goto SimplePrefixOperator;
			}
			curr.count = 1;
			goto SimplePrefixOperator;
		}

		case Ast_OpenProcedure:{
			AstNode procnode = curr;
			procnode.type = Ast_Procedure;
			*res_it = procnode; res_it += 1;
			if (it->type == Ast_EndScope){
				AstNode open_node = { .type = Ast_StartScope, .pos = it->pos };
				it += 3;
				AstNode open_oper = { .type = Ast_Procedure, .pos = res_it - tokens.data };
				opers[opers_size] = open_oper; opers_size += 1;
				*res_it = open_node; res_it += 1;
				goto ExpectValue;
			}
			curr.pos = res_it - tokens.data - 1;
			curr.count = 1;
			goto SimplePrefixOperator;
		}

		case Ast_With:
			*res_it = (AstNode){ .type = Ast_OpenBlock, .pos = curr.pos };
			res_it += 1;
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = curr;
			opers_size += 1;
			goto ExpectValue;

		default:
			RETURN_ERROR("expected value", curr.pos);
		}
	}


	ExpectOperator:{
		AstNode curr = *it;
		it += 1;

		if (PrecsLeft[curr.type] == UINT8_MAX)
			RETURN_ERROR("expected operator", curr.pos);

		for (;;){
			AstNode head = opers[opers_size-1];
			if (PrecsRight[head.type] < PrecsLeft[curr.type]) break;
			opers_size -= 1;
			if (head.type == Ast_Procedure){
				AstNode *startnode = tokens.data + head.pos;
				*(res_it + 0) = (AstNode){ .type = Ast_Return, .pos=startnode->pos };
				*(res_it + 1) = (AstNode){ .type = Ast_EndScope, .pos=startnode->pos };
				res_it += 2;
				startnode->pos = (res_it - tokens.data) - head.pos;
				continue;
			}
			if (head.type == Ast_With){
				head = (AstNode){ .type = Ast_EndScope, .pos = curr.pos };
			}
			*res_it = head;
			res_it += 1;
			if (head.type == Ast_Variable){
				opers_size -= 1;
				*res_it = opers[opers_size];
				res_it += 1;
			}
		}

		if (Ast_Assign <= curr.type && curr.type <= Ast_Reinterpret){
			if (curr.type == Ast_Assign){
				AstNode varnode = opers[opers_size-1];
				if (
					varnode.type == Ast_Variable &&
					(varnode.flags & (AstFlag_ClassSpec|AstFlag_Constant))
				){
					if (opers[opers_size-3].type == Ast_OpenProcedure){
						*(res_it+0) = varnode;
						*(res_it+1) = opers[opers_size-2];
						res_it += 2;
						curr.type = Ast_DefaultParam;
						uint8_t default_and_infered_size = opers[opers_size-3].flags;
						if (default_and_infered_size == 255) // prevent overflow 
							RETURN_ERROR("definitely too many default values", curr.pos);
						_Static_assert(sizeof(ValueInfo)/sizeof(NameId) == 6);
						opers[opers_size-3].flags = default_and_infered_size + 6;
						opers[opers_size-2] = curr;
						opers_size -= 1;
					} else{
						varnode.flags |= (AstFlag_ClassSpec|AstFlag_Initialized);
						opers[opers_size-1] = varnode;
					}
					goto ExpectValue;
				}
			}
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;
		}

		switch (curr.type){
		case Ast_Terminator:
			goto EndOfFile;

		case Ast_Semicolon:
			goto ExpectValue;

		case Ast_Comma:
			opers[opers_size-1].count += 1;
			goto ExpectValue;

		case Ast_OpenBrace:{
			AstNode head = opers[opers_size-1];
			switch (head.type){
			case Ast_Procedure:{
				AstNode *startnode = tokens.data + head.pos;
				startnode->flags |= AstFlag_ReturnSpec;
				*res_it = (AstNode){ .type = Ast_ReturnClass, .pos = startnode->pos };
				res_it += 1;
				opers[opers_size-1].type = Ast_StartScope;
				goto ExpectValue;
			}
			default:
				RETURN_ERROR("opening brace has nothing to open", curr.pos);
			}
		}

		case Ast_Dereference:
		SimplePostfixOperator:
			*res_it = curr;
			res_it += 1;
			goto ExpectOperator;

		case Ast_GetField:
			*(res_it+0) = curr;
			*(res_it+1) = *it;
			it += 1;
			res_it += 2;
			goto ExpectOperator;

		case Ast_OpenPar:
			curr.type = Ast_Call;
		case Ast_GetProcedure:
			if (it->type == Ast_EndScope){
				it += 1;
				curr.count = 0;
				goto SimplePostfixOperator;
			}
		SimplePostfixScope:
			curr.count = 1;
			CHECK_OPER_STACK_OVERFLOW(it->pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;

		case Ast_Subscript:
			if (it->type == Ast_EndScope){
				it += 1;
				curr.type = Ast_Span;
				goto SimplePostfixOperator;
			}
			goto SimplePostfixScope;

		case Ast_FieldSubscript:
			if (it->type == Ast_EndScope)
				RETURN_ERROR("missing argument for field subscript", curr.pos);
			goto SimplePostfixScope;

		case Ast_EndScope:
			goto HandleEndOfScope;

		default:
			RETURN_ERROR("parser error: unhandled token", curr.pos);
		}
	}


	HandleEndOfScope:{
		opers_size -= 1;
		AstNode sc = opers[opers_size];

		switch (sc.type){
		case Ast_OpenPar:
			if (sc.count != 0)
				RETURN_ERROR("parenthesis contain too many expressions", sc.pos);
			goto ExpectOperator;
		
		case Ast_Call:
		case Ast_GetProcedure:
			*res_it = sc; res_it += 1;
			goto ExpectOperator;

		case Ast_Subscript:
			*res_it = sc; res_it += 1;
			goto ExpectOperator;

		case Ast_OpenProcedureClass:
			sc.type = Ast_ProcedureClass;
			opers[opers_size] = sc; opers_size += 1;
			goto ExpectValue;
		
		case Ast_FieldSubscript:
			if (sc.count != 1)
				RETURN_ERROR("field subscript takes a single argument", sc.pos);
			*res_it = sc; res_it += 1;
			goto ExpectOperator;

		case Ast_OpenProcedure:{
			AstNode *procnode = tokens.data + sc.pos;
			if (sc.count > MAX_PARAM_COUNT)
				RETURN_ERROR("procedure has too many parameters", procnode->pos);
			procnode->param_count = sc.count;
			procnode->default_and_infered_size = sc.flags;
			AstNode open_node = { .type = Ast_StartScope, .pos = (it-1)->pos };
			it += 2;
			AstNode open_oper = { .type = Ast_Procedure, .pos = res_it - tokens.data };
			opers[opers_size] = open_oper; opers_size += 1;
			*res_it = open_node; res_it += 1;
			goto ExpectValue;
		}

		case Ast_StartScope:{
			AstNode *argnode = tokens.data + sc.pos;
			*res_it = (AstNode){ .type = Ast_EndScope, .pos = (it-1)->pos };
			res_it += 1;
			argnode->pos = (res_it - tokens.data) - sc.pos;
			goto ExpectOperator;
		}
			
		default: 
			RETURN_ERROR("parser error: unhandled scope type", sc.pos);
		}
	}
	
EndOfFile:
	if (opers[opers_size-1].type != Ast_StartScope || opers[opers_size-1].flags != 0xff){
		RETURN_ERROR("unexpected end of file", (it-1)->pos-1);
	}
	*res_it = (AstNode){ .type = Ast_Terminator };
	tokens.end = res_it;
ReturnError:
	return tokens;
#undef RETURN_ERROR
}





static bool is_valid_name_char(char c){
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';
}

static bool is_valid_first_name_char(char c){
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}

static bool is_number(char c){
	return (uint8_t)(c-'0') < 10;
}

static bool is_whitespace(char c){
	return c==' ' || c=='\n' || c=='\t' || c=='\v';
}



static uint32_t parse_character(const char **iter){
	const char *it = *iter;
	uint32_t c;

ParseCharacter:{
		c = *it;
		if (c != '\\') return utf8_parse((const uint8_t **)iter);
	
		int utf_base = 10;
	
		it += 1;
		c = *it;
		switch (c){
		case '\'':
		case '\"':
		case '\\':
			break;
		case '@': case '#': case '$': case '%': case '^': case '&': case '*':
		case '(': case ')': case '[': case ']': case '{': case '}':
			c = '\\';
			it -= 1;
			break;
		case 'x':
			utf_base = 16;
			goto ParseUtf8;
		case 'o':
			utf_base = 8;
			goto ParseUtf8;
		ParseUtf8:
			it += 1;
			FALLTHROUGH;
		case '0' ... '9':{
			const char *old_iter = it;
			c = strtol(old_iter, (char **)&it, utf_base);
			if (old_iter == it) return UINT32_MAX;
			it -= 1;
			break;
		}
		case 'a': c = '\a';   break;
		case 'b': c = '\b';   break;
		case 'e': c = '\033'; break;
		case 'f': c = '\f';   break;
		case 'r': c = '\r';   break;
		case 'n': c = '\n';   break;
		case 't': c = '\t';   break;
		case 'v': c = '\v';   break;
		case '\n':
		case '\v':
		case '_':
			it += 1;
			goto ParseCharacter;
		default:
			return UINT32_MAX;
		}
	}

	*iter = it + 1;
	return c;
}



static uint64_t parse_number_dec(const char **src_it){
	const char *src = *src_it;
	size_t res = 0;

	while (is_number(*src)){
		res = res*10 + (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static uint64_t parse_number_bin(const char **src_it){
	const char *src = *src_it;
	size_t res = 0;

	while (*src=='0' || *src=='1'){
		res = (res << 1) | (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static uint64_t parse_number_oct(const char **src_it){
	const char *src = *src_it;
	size_t res = 0;

	while (is_number(*src) && *src<'8'){
		res = (res << 3) | (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static uint64_t parse_number_hex(const char **src_it){
	const char *src = *src_it;
	size_t res = 0;

	for (;;){
		char c = *src;
		if (is_number(c)){
			res = (res << 4) | (c - '0');
		} else{
			if (c <= 'Z') c += 'a'-'A';
			if ('a' > c || c > 'f') break;
			res = (res << 4) | (10 + c - 'a');
		}
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static uint64_t parse_number_roman(const char **src_it){
	const char *src = *src_it;
	size_t res = 0;

	size_t last_size = 0;
	size_t last_count = 0;

	static const uint16_t Values[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
	static const char FirstChars[] = "MCDCCXLXXIVII";
	static const char LastChars[] = "MDCLXV";
	for (size_t i=0; i!=SIZE(Values); i+=1){
		const char *next = src;
		while (next+=1, *next== '_');
		if (i & 1){
			while (*src == FirstChars[i] && *next == LastChars[i/2]){
				res += Values[i];
				while (next+=1, *next== '_');
				src = next;
				while (next+=1, *next== '_');
			}
		} else{
			while (*src == FirstChars[i]){
				res += Values[i];
				src = next;
				while (next+=1, *next== '_');
			}
		}
	}

	*src_it = src;
	return res;
}

static enum AstType parse_number(void *res_data, const char **src_it){
	const char *src = *src_it;

	uint64_t res_u64 = 0;
	enum AstType res_type = Ast_Unsigned;

	if (*src == '0'){
		src += 1;
		switch (*src){
		case 'b': src += 1; res_u64 = parse_number_bin(&src); goto Return;
		case 'o': src += 1; res_u64 = parse_number_oct(&src); goto Return;
		case 'x': src += 1; res_u64 = parse_number_hex(&src); goto Return;
		case 'r': src += 1; res_u64 = parse_number_roman(&src); goto Return;
		default: break;
		}
	}
	
	res_u64 = parse_number_dec(&src);
	
	if (*src == '.'){
		src += 1;
		double res_f64 = (double)res_u64; // decimal part
		const char *fraction_str = src;
		uint64_t fraction_num = parse_number_dec(&src);
		if (fraction_num != 0){
			size_t fraction_width = 0;
			for (; fraction_str!=src; fraction_str+=1){ fraction_width += *fraction_str != '_'; }
			double fraction_den = util_ipow_f64(10.0, fraction_width);
			res_f64 += (double)fraction_num / fraction_den;
		}
		if (*src == 'f'){
			src += 1;
			res_type = Ast_Float32;
			float res_f32 = (float)res_f64;
			memcpy(&res_u64, &res_f32, sizeof(float));
		} else{
			res_type = Ast_Float64;
			memcpy(&res_u64, &res_f64, sizeof(double));
		}
	} else if (*src == 'f'){
		src += 1;
		res_type = Ast_Float32;
		float res_f32 = (float)res_u64;
		memcpy(&res_u64, &res_f32, sizeof(float));
	}
	
Return:
	*src_it = src;
	memcpy(res_data, &res_u64, sizeof(uint64_t));
	return res_type;
}




// DEBUG INFORMATION HELPERS
static void print_codeline(const char *text, size_t position){
	size_t row = 0;
	size_t col = 0;
	size_t row_position_prev = 0;
	size_t row_position = 0;

	for (size_t i=0; i!=position; ++i){
		col += 1;
		if (text[i]=='\n' || text[i]=='\v'){
			row_position_prev = row_position;
			row_position += col;
			row += 1;
			col = 0;
		}
	}
	fprintf(stderr, " -> row: %lu, column: %lu\n>\n", row, col);

	if (row != 0){
		putchar('>');
		putchar(' ');
		putchar(' ');
		for (size_t i=row_position_prev;; ++i){
			char c = text[i];
			if (c=='\0' || c=='\n' || c=='\v') break;
			putchar(c);
		}
		putchar('\n');
	}

	putchar('>');
	putchar(' ');
	putchar(' ');
	for (size_t i=row_position;; ++i){
		char c = text[i];
		if (c=='\0' || c=='\n' || c=='\v') break;
		putchar(c);
	}
	putchar('\n');

	putchar('>');
	putchar(' ');
	putchar(' ');
	for (size_t i=0; i!=col; ++i){
		putchar(text[row_position+i]=='\t' ? '\t' : ' ');
	}
	putchar('^');
	putchar('\n');
	putchar('\n');
}


static void raise_error(const char *text, const char *msg, uint32_t pos){
	fprintf(stderr, "error: \"%s\"", msg);
	print_codeline(text, pos);
	exit(1);
}



