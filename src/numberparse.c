#include "parser.h"

int main(int argc, char **argv){
	if (argc != 2) return 1;

	const char *src = argv[1];

	AstData res_data;
	enum AstType res_type = parse_number(&res_data, &src);	

	puts(argv[1]);
	for (size_t i=0; i!=src-argv[1]; i+=1){ putchar('^'); }
	putchar('\n');
	
	switch (res_type){
		case Ast_Unsigned: printf("result : U64 = %lu\n", res_data.u64); break;
		case Ast_Float32:  printf("result : F32 = %f\n",  res_data.f32); break;
		case Ast_Float64:  printf("result : F64 = %lf\n", res_data.f64); break;
		default: return 2;
	}

	return 0;
}
