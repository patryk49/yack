#include <stdio.h>
#include <time.h>

#include "parser.h"
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
					puts("testnodes <option> <filename>\n  options:");
					puts("  -h     print help");
					puts("  -t     dont show tokens");
					puts("  -a     dont show ast nodes");
					puts("  -s     dont show statistics");
					puts("  -S     print hash set info");
					puts("  -n     show nops");
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

	if (initialize_compiler()){
		fprintf(stderr, "failed to initialize\n");
		return 2137;
	}

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
		size_t token_size = tokens.end - tokens.data;
		size_t ast_size = ast.end - ast.data;
		size_t token_count = count_tokens(tokens);
		size_t ast_count = count_ast(ast);
		printf("reading time:  %lf [s]\n", (double)read_time*0.000001);
		printf("lexing time:   %lf [s]\n", (double)tok_time*0.000001);
		printf("parsing time:  %lf [s]\n", (double)parse_time*0.000001);
		printf("token size:    %zu [nodes]\n", token_size);
		printf("ast size:      %zu [nodes]\n", ast_size);
		printf("token count:   %zu\n", token_count);
		printf("ast count:     %zu\n", ast_count);
		printf("ast per token: %lf\n", (double)ast_size / (double)token_size);
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
		AstData data = tokens.data[i].tail[0];
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		i += TokenSizes[node.type];
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_Procedure: break;
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
			printf(": ");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
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
		AstData data = ast.data[i].tail[0];
		if (!show_nops && node.type == Ast_Nop) continue;
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		i += AstNodeSizes[node.type];
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_Procedure:
			printf(": arg_count = %lu", node.count);
			if (node.flags){
				printf(",  flags = ");
				if (node.flags & AstFlag_ReturnSpec){ printf("ReturnSpec "); }
			}
			break;
		case Ast_ProcedureClass:
			printf(": arg_count = %lu", node.count);
			break;
		case Ast_StartScope:
			printf(": scope_size = %u", node.pos);
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
			printf(": ");
			for (size_t i=0; i!=node.count; i+=1){
				putchar(global_names.data[data.name_id+i]);
			}
			break;
		case Ast_EnumLiteral:
		case Ast_Character:
		case Ast_String:
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
