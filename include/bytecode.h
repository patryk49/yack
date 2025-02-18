#include "utils.h"
#include "parser.h"


static uint32_t bc_alloc_data(Class clas, void *data, size_t data_size){
	uint32_t index = global_bc_size;
	BcNode *top = global_bc + index;
	*top = (BcNode){ .type = BC_Data };
	(top+1)->clas = clas;
	memcpy(top+2, data, data_size);
	global_bc_size = index + 2 + (data_size + sizeof(BcNode) - 1)/sizeof(BcNode);
	return index;
}

