#include <stdio.h>
#include <time.h>

#include "eval.h"
#include "files.h"


void print_tokens(AstArray tokens);
void print_ast(AstArray tokens);

size_t count_tokens(AstArray tokens);
size_t count_ast(AstArray ast);



// settings
bool show_tokens = true;
bool show_ast    = true;
bool show_stats  = true;
bool show_nops   = false;
bool show_sets   = false;



int main(int argc, char **argv){
	char *input = NULL;
	for (size_t i=1; i!=argc; i+=1){
		if (argv[i][0] == '-'){
			for (size_t j=1;; j+=1){
				char opt = argv[i][j];
				if (opt == '\0') break;
				switch (opt){
				case 'h':
					printf(
						"testnodes <option> <filename>\n  options:\n"
						"  -h     print help\n"
						"  -t     dont show tokens\n"
						"  -a     dont show ast nodes\n"
						"  -s     dont show statistics\n"
						"  -S     print hash set info\n"
						"  -n     show nops\n"
					);
					return 0;
				case 't': show_tokens = false; break;
				case 'a': show_ast    = false; break;
				case 's': show_stats  = false; break;
				case 'n': show_nops   = true;  break;
				case 'S': show_sets   = true;  break;
				default:
					fprintf(stderr, "unknown option: -%c\n", opt);
					return 10;
				}
			}
		} else{
			if (input != NULL){
				fprintf(stderr, "input file already specified\n");
				return 20;
			}
			input = argv[i];
		}
	}

	StringView text;
	time_t read_time = clock();
	if (input == NULL){
		text = read_file(stdin);
		if (text.data == NULL){
			fprintf(stderr, "allocation failrule\n");
			return 1;
		}
	} else{
		text = mmap_file(input);
		if (text.data == NULL){
			fprintf(stderr, "error while reading the file: \"%s\"\n", input);
			return 21;
		}
	}
	read_time = clock() - read_time;

	initialize_compiler_globals();

	time_t tok_time = clock();
	AstArray tokens = make_tokens(text.data);
	tok_time = clock() - tok_time; 
	if (tokens.data == NULL){
		raise_error(text.data, tokens.error, tokens.position);
	}

	if (show_tokens){
		puts("tokens:");
		print_tokens(tokens);
		putchar('\n');
	}

	AstArray ast = ast_array_clone(tokens);

	time_t parse_time = clock();
	ast = parse_tokens(ast);
	parse_time = clock() - parse_time;
	if (ast.data == NULL){
		raise_error(text.data, ast.error, ast.position);
	}

	if (show_ast){
		puts("ast:");
		print_ast(ast);
		putchar('\n');
	}

	if (show_stats){
		double read_time_s = (double)read_time * 0.000001;
		double tok_time_s = (double)tok_time * 0.000001;
		double parse_time_s = (double)parse_time * 0.000001;
		double making_ast_time_s = (double)(read_time_s + tok_time_s + parse_time_s);
		size_t token_size = tokens.end - tokens.data;
		size_t ast_size = ast.end - ast.data;
		size_t token_count = count_tokens(tokens);
		size_t ast_count = count_ast(ast);
		double text_size_mb = text.size * 0.000001;

		printf("token count    :%10zu\n", token_count);
		printf("ast node count :%10zu\n", ast_count);
		printf("node/token count ratio : %8.6lf\n\n", (double)ast_count/(double)token_count);
		
		printf("tokens size    :%10zu\n", token_size);
		printf("ast nodes size :%10zu\n", ast_size);
		printf("nodes/tokens size ratio : %8.6lf\n\n", (double)ast_size/(double)token_size);
		
		printf("lexing speed     :%13.2lf [tokens/s]\n", (double)token_count/tok_time_s);
		printf("parsing speed    :%13.2lf [nodes/s]\n", (double)ast_count/parse_time_s);
		printf("making ast speed :%13.2lf [nodes/s]\n\n", (double)ast_count/making_ast_time_s);
		
		printf("reading time    :%10.6lf [s]\n", read_time_s);
		printf("lexing time     :%10.6lf [s]\n", tok_time_s);
		printf("parsing time    :%10.6lf [s]\n", parse_time_s);
		printf("making ast time :%10.6lf [s]\n\n", making_ast_time_s);
		
		printf("reading speed    :%11.2lf [MB/s]\n", text_size_mb/read_time_s);
		printf("lexing speed     :%11.2lf [MB/s]\n", text_size_mb/tok_time_s);
		printf("parsing speed    :%11.2lf [MB/s]\n", text_size_mb/parse_time_s);
		printf("making ast speed :%11.2lf [MB/s]\n\n", text_size_mb/making_ast_time_s);
	}

	if (show_sets){
		printf("uniuqe names:         %zu\n", global_name_set.size);
		printf("name set capacity:    %zu\n", global_name_set.capacity);
		printf("names data size:      %zu\n", global_names.size);
		printf("names data capacity:  %zu\n", global_names.capacity);
		printf("hash colissions:      %zu\n", hash_colissions);
		printf("hash colission ratio: %lf\n", (double)hash_colissions/(double)global_name_set.size);
	}

	return 0;
}

void print_tokens(AstArray tokens){
	for (size_t i=1; i!=tokens.end-tokens.data;){
		AstNode node = tokens.data[i];
		Data data = tokens.data[i+1].data;
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		i += TokenSizes[node.type];
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_Unsigned:
			printf(": %lu", data.u64);
			break;
		case Ast_Float32:
			printf(": %f", data.f32);
			break;
		case Ast_Float64:
			printf(": %lf", data.f64);
			break;
		case Ast_Identifier:
			printf(": \"");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
			}
			printf("\"");
			break;
		case Ast_Variable:
			printf(": \"");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
			}
			printf("\" ");
			if (node.flags & AstFlag_ClassSpec  ){ printf("ClassSpec ");   }
			if (node.flags & AstFlag_Initialized){ printf("Initialized "); }
			if (node.flags & AstFlag_Constant   ){ printf("Constant");     }
			break;
		case Ast_Character:{
			char repr[8] = {0};
			utf8_write(repr, data.code);
			printf(": code = %u, repr = \"%s\"", data.code, repr);
			break;
		}
		case Ast_String:{
			printf(
				": size = %u, repr = \"%s\"", data.bufinfo.size,
				(const char *)(global_bc + data.bufinfo.index)
			);
			break;
		}
		case Ast_Equal:
		case Ast_Less:
		case Ast_Greater:
		case Ast_LogicOr:
		case Ast_LogicAnd:
		case Ast_Contains:
		case Ast_BitOr:
		case Ast_BitAnd:
			if (node.flags & AstFlag_Negate){
				printf(": Negate");
			}
			break;
		default: break;
		}
		putchar('\n');
	}
}

void print_ast(AstArray ast){
	for (size_t i=1; i!=ast.end-ast.data;){
		AstNode node = ast.data[i];
		Data data = ast.data[i+1].data;
		if (!show_nops && node.type == Ast_Nop){ i+=1; continue; }
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		i += AstNodeSizes[node.type];
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_Procedure:
			printf(
				": param_count = %u, default_and_infered_size = %u",
				(unsigned)node.param_count, (unsigned)node.default_and_infered_size
			);
			if (node.flags){
				printf(",  flags = ");
				if (node.flags & AstFlag_ReturnSpec){ printf("ReturnSpec "); }
			}
			break;
		case Ast_ProcedureClass:
			printf(": arg_count = %lu", node.count);
			break;
		case Ast_Call:
		case Ast_GetProcedure:
		case Ast_Subscript:
		case Ast_FieldSubscript:
			printf(": arg_count = %lu ", node.count);
			break;
		case Ast_StartScope:
			printf(": scope_size = %u", node.pos);
			if (node.flags & AstFlag_ReturnSpec){ printf("  ReturnSpec"); }
			break;
		case Ast_Unsigned:
			printf(": %lu", data.u64);
			break;
		case Ast_Float32:
			printf(": %f", data.f32);
			break;
		case Ast_Float64:
			printf(": %lf", data.f64);
			break;
		case Ast_Identifier:
		case Ast_GetField:
		case Ast_EnumLiteral:
		case Ast_NamedInfered:
			printf(": \"");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
			}
			printf("\"");
			break;
		case Ast_Variable:
			printf(": \"");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
			}
			printf("\" ");
			if (node.flags & AstFlag_ClassSpec  ){ printf("ClassSpec ");   }
			if (node.flags & AstFlag_Initialized){ printf("Initialized "); }
			if (node.flags & AstFlag_Constant   ){ printf("Constant");     }
			break;
		case Ast_Character:{
			char repr[8] = {0};
			utf8_write(repr, data.code);
			printf(": code = %u, repr = \"%s\"", data.code, repr);
			break;
		}
		case Ast_String:{
			printf(
				": size = %u, repr = \"%s\"", data.bufinfo.size,
				(const char *)(global_bc + data.bufinfo.index)
			);
			break;
		}
		case Ast_Equal:
		case Ast_Less:
		case Ast_Greater:
		case Ast_LogicOr:
		case Ast_LogicAnd:
		case Ast_Contains:
		case Ast_BitOr:
		case Ast_BitAnd:
			if (node.flags & AstFlag_Negate){
				printf(": Negate");
			}
			break;
		default: break;
		}
		putchar('\n');
	}
}

size_t count_tokens(AstArray tokens){
	size_t res = 0;
	for (size_t i=1; i!=tokens.end-tokens.data;){
		AstNode node = tokens.data[i];
		i += TokenSizes[node.type];
		res += 1;
		if (node.type == Ast_Terminator) break;
	}
	return res;
}

size_t count_ast(AstArray ast){
	size_t res = 0;
	for (size_t i=1; i!=ast.end-ast.data;){
		AstNode node = ast.data[i];
		i += AstNodeSizes[node.type];
		res += 1;
		if (node.type == Ast_Terminator) break;
	}
	return res;
}
