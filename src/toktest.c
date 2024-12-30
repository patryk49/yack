#include "parser.h"
#include "files.h"
//#include "eval.h"

#include <time.h>

#define PRINT_LIMIT SIZE_MAX

static size_t print_tokens(AstArray, const char *);
static size_t print_nodes(AstArray, const char *);
static void print_name_table();


int main(int argc, char **argv){
	bool display_tokens = true;
	bool display_nodes  = true;
	bool evaluate = false;
	bool show_name_table = false;
	const char *path = NULL;
	for (size_t i=1; i!=(size_t)argc; i+=1){
		if (argv[i][0] == '-'){
			if (argv[i][1]=='t' && argv[i][2]=='\0'){
				display_tokens = false;
			} else if (argv[i][1]=='a' && argv[i][2]=='\0'){
				display_nodes  = false;
			} else if (argv[i][1]=='e' && argv[i][2]=='\0'){
				evaluate = true;
			} else if (argv[i][1]=='n' && argv[i][2]=='\0'){
				show_name_table = true;
			} else if (argv[i][1]=='h' && argv[i][2]=='\0'){
				puts("toktest [arguments] [filename]");
				puts("-t    dont print tokens");
				puts("-a    dont print ast nodes");
				puts("-a    print name table");
				puts("-h    display help");
				return 0;
			} else{
				fprintf(stderr, "wrong option: %s\n", argv[i]);
				return 1;
			}
		} else{
			path = argv[i];
		}
	}

	size_t token_count;
	size_t node_count;

	time_t read_time = clock();
	String text;
	if (path != NULL){
		text = mmap_file(path);
		if (text.ptr == NULL){
			fprintf(stderr, "file \"%s\" cannot be memory mapped\n", path);
			return 1;
		}
	} else{
		text = read_file(stdin);
		if (text.ptr == NULL){
			fprintf(stderr, "cannot read the input\n");
			return 1;
		}
	}
	read_time = clock() - read_time;

	smem_init(1000000000);
	init_name_id_set(512);

	time_t tok_time = clock();
	AstArray tokens = make_tokens(text.ptr);
	size_t tokens_size = tokens.end - tokens.data;
	tok_time = clock() - tok_time;
	if (tokens.data == NULL){
		raise_error(text.ptr, tokens.error, tokens.position);
	}
	
	time_t print_time_0 = 0;
	time_t print_time_1 = 0;
	if (display_tokens){
		print_time_0 = clock();
		puts(">>> TOKENS >>>");
		token_count = print_tokens(tokens, text.ptr);
		putchar('\n');
		print_time_0 = clock() - print_time_0;
	}

	time_t parse_time = clock();
	AstArray nodes = tokens;
	size_t tokens_data_size = (tokens.end-tokens.data)*sizeof(AstNode);
	nodes.data = malloc(tokens_data_size);
	memcpy(nodes.data, tokens.data, tokens_data_size);
	nodes = parse_tokens(nodes);
	parse_time = clock() - parse_time;
	if (nodes.data == NULL){
		raise_error(text.ptr, nodes.error, nodes.position);
	}
	size_t nodes_size = nodes.end - nodes.data;

	if (display_nodes){
		print_time_1 = clock();
		puts(">>> AST_NODES >>>");
		node_count = print_nodes(nodes, text.ptr);
		putchar('\n');
		print_time_1 = clock() - print_time_1;
	}
	time_t print_time = print_time_0 + print_time_1;
	if (display_tokens)
		printf("token count : %zu\n", token_count);
	if (display_nodes)
		printf("node count  : %zu\n", node_count);
	if (display_tokens && display_nodes)
		printf("node/token count ratio : %lf\n", (double)node_count/(double)token_count);
	time_t making_ast_time = read_time + tok_time + parse_time;
	printf("tokens size: %zu Nodes\n", tokens_size);
	printf("nodes size:  %zu Nodes\n", nodes_size);
	printf("nodes/tokens size ratio : %lf\n", (double)nodes_size/(double)tokens_size);
	printf("reading time:  %lf s\n", (double)read_time/1000000.0);
	printf("lexing time:   %lf s\n", (double)tok_time/1000000.0);
	printf("parsing time:  %lf s\n", (double)parse_time/1000000.0);
	printf("making ast time:  %lf s\n", (double)making_ast_time/1000000.0);
	printf("making ast speed: %lf MB/s\n", (double)text.size/(double)making_ast_time);
	printf("printing time: %lf s\n", (double)print_time/1000000.0);

	if (show_name_table) print_name_table();

#if 0
	if (evaluate){
		//printf("Evaluation is not implemented.\n"); return 1;
		ModuleInfo m = make_module(path, 3);
		Node *iter = nodes.ptr + 1;
		putchar('\n');
		putchar('\n');
		eval_module(&m, &m.ir_temp_storage, &iter);
		printf("Global IR Procedure:\n");
		print_ir_procedure(&m.ir_temp_storage);
	}
#endif

	return 0;
}



static size_t print_tokens(AstArray nodes, const char *text){
	size_t count = 0;
	if (nodes.data == NULL) raise_error(text, nodes.error, nodes.position);
	
	size_t nodes_size = nodes.end - nodes.data;
	for (size_t n=1; n<nodes_size-1; n+=1){
		if (n >= PRINT_LIMIT) break;
		AstNode it = nodes.data[n];
		AstData data = nodes.data[n].tail[0];
		enum AstType t = it.type;
		count += 1;
		if (t >= SIZE(AstTypeNames)){
			fprintf(stderr, "WRONG TYPE : %i\n", t);
			exit(1);
		}
		if (t == Ast_Terminator) break;
		printf("%10zu %8zu %s", n-1, (size_t)it.pos, AstTypeNames[(size_t)t]);
		switch (t){
		case Ast_Variable:
			if ((it.flags & NodeFlag_Destructure) != 0){
				printf(": index: (%u), size: %u", (unsigned)it.pos, (unsigned)it.count);
				if (t == Ast_Variable && it.flags != 0){
					putchar(' ');
					putchar('|');
					if ((it.flags >> 0) & 1) printf(" Constant");
					if ((it.flags >> 1) & 1) printf(" ClassSpec");
					if ((it.flags >> 2) & 1) printf(" Initialized");
					if ((it.flags >> 3) & 1) printf(" Parameter");
					//if ((it.flags >> 4) & 1) printf(" DestructureField");
					if ((it.flags >> 5) & 1) printf(" Destructure");
				}
				break;
			}
		case Ast_Identifier:
		case Ast_GetField:
		case Ast_EnumLiteral:
			if (it.count == UINT16_MAX){
				printf(": MAX");
				break;
			}
			n += 1;
			printf(": \"");
			for (size_t i=0; i!=it.count; i+=1){
				putchar(static_memory.names[data.name_id + i]);
			}
			putchar('\"');
			if (t == Ast_Variable && it.flags != 0){
				putchar(' ');
				putchar('|');
				if ((it.flags >> 0) & 1) printf(" Constant");
				if ((it.flags >> 1) & 1) printf(" ClassSpec");
				if ((it.flags >> 2) & 1) printf(" Initialized");
				if ((it.flags >> 3) & 1) printf(" Parameter");
				//if ((it.flags >> 4) & 1) printf(" DestructureField");
				if ((it.flags >> 5) & 1) printf(" Destructure");
			}
			break;
		case Ast_String:
			n += 1;
			printf(": \"");
			for (size_t i=0; i!=data.u64; i+=1){
				putchar(text[it.pos + i + 1]);
			}
			putchar('\"');
			break;
		case Ast_Character:
			n += 1;
			char buf[8] = {0};
			utf8_write(buf, data.u64);
			printf(": \'%s\' - %u", buf, (unsigned)data.u64);
			break;
		case Ast_Unsigned:
			n += 1;
			printf(": %lu", data.u64);
			break;
		case Ast_Float32:
			n += 1;
			printf(": %f", data.f32);
			break;
		case Ast_Float64:
			n += 1;
			printf(": %f", data.f64);
			break;
		case Ast_Break:
		case Ast_Continue:
		case Ast_Goto:
			break;
		case Ast_OpenPar:
		case Ast_ProcPointer:
		case Ast_OpenBrace:
		case Ast_FieldSubscript:
		case Ast_OpenBracket:
			printf(": %u", (unsigned)it.count);
			break;
		
		default:
			goto Next;
		}
	Next:
		putchar('\n');
	}
	return count;
}



static void print_name_table(){
	puts(" index  |       hash       | name_id |     name_string      ");
	puts("--------+------------------+---------+----------------------");
	for (size_t i=0; i!=global_name_set.capacity; i+=1){
		NameEntry entry = global_name_set.data[i];
		if (entry.length == 0) continue;
		printf(" %6zu | %016lx | %7u | ", i, entry.hash, entry.name_id);
		for (size_t j=0; j!=entry.length; j+=1)
			putchar(static_memory.names[entry.name_id + j]);
		putchar('\n');
	}
	puts("--------+------------------+---------+----------------------");
}


static size_t print_nodes(AstArray nodes, const char *text){
	return 0;
}

