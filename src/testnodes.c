#include <stdio.h>
#include <time.h>

#include "parser.h"
#include "files.h"


void print_tokens(AstArray tokens);
void print_ast(AstArray tokens);

size_t count_tokens(AstArray tokens);
size_t count_ast(AstArray ast);

int main(int argc, char **argv){
	bool show_tokens = true;
	bool show_ast = true;
	bool show_stats = true;
	char *input = NULL;
	for (size_t i=1; i!=argc; i+=1){
		if (argv[i][0] == '-'){
			char opt = argv[i][1];
			switch (opt){
			case 'h':
				printf("testnodes <option> <filename>\n  options:\n");
				printf("  -h     print help");
				printf("  -t     dont show tokens");
				printf("  -a     dont show ast nodes");
				printf("  -s     dont show statistics");
				break;
			case 't': show_tokens = false; break;
			case 'a': show_ast = false; break;
			case 's': show_stats = false; break;
			default:
				fprintf(stderr, "unknown option: -%c\n", opt);
				return 10;
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
		printf("reading time:  %.2lf [s]\n", (double)read_time*0.000001);
		printf("lexing time:   %.2lf [s]\n", (double)tok_time*0.000001);
		printf("parsing time:  %.2lf [s]\n", (double)parse_time*0.000001);
		printf("token size:    %zu [nodes]\n", token_size);
		printf("ast size:      %zu [nodes]\n", ast_size);
		printf("token count:   %zu\n", token_count);
		printf("ast count:     %zu\n", ast_count);
		printf("ast per token: %.2lf\n", (double)ast_size / (double)token_size);
	}

	return 0;
}

void print_tokens(AstArray tokens){
	for (size_t i=1; i!=tokens.end-tokens.data; i+=1){
		AstNode node = tokens.data[i];
		AstData data = tokens.data[i].tail[0];
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_DoubleArrow: i+=1; break;
		case Ast_Unsigned: i+=1;
			printf(": %lu", data.u64);
			break;
		default: break;
		}
		putchar('\n');
	}
}

void print_ast(AstArray ast){
	for (size_t i=1; i!=ast.end-ast.data; i+=1){
		AstNode node = ast.data[i];
		AstData data = ast.data[i].tail[0];
		printf("%5zu%5zu  %s", i, node.pos, AstTypeNames[node.type]);
		switch (node.type){
		case Ast_Terminator: return;
		case Ast_Procedure:
			printf(": arg_count = %lu", node.count);
			if (node.flags){
				printf(",  flags = ");
				if (node.flags & AstFlag_ReturnSpec){ printf("ReturnSpec "); }
			}
			break;
		case Ast_Arrow:
			printf(": arg_count = %lu", node.count);
			break;
		case Ast_StartScope:
			printf(": scope_size = %u", node.pos);
			break;
		case Ast_Unsigned: i+=1;
			printf(": %lu", data.u64);
			break;
		case Ast_Identifier:
		case Ast_EnumLiteral:
		case Ast_Float32:
		case Ast_Float64:
		case Ast_Character:
		case Ast_String:
			i += 1;
		default: break;
		}
		putchar('\n');
	}
}

size_t count_tokens(AstArray tokens){
	return 0;
}

size_t count_ast(AstArray ast){
	return 0;
}
