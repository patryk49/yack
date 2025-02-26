#include "utils.h"
#include "parser.h"

_Static_assert(sizeof(BcNode) == 8, "must be true");



static uint32_t static_data_alloc(void *data, size_t data_size, uint8_t alignment){
	uint32_t index = global_bc_size;
	Node *top = global_bc + index;
	*top = (DataHeader){ .bytesize = data_size, .alignment = alignment };
	memcpy(top+1, data, data_size);
	global_bc_size = index + 1 + (data_size + sizeof(BcNode) - 1)/sizeof(BcNode);
	return index;
}


static uint32_t alloc_data_alloc_pb(
	void *data, size_t data_size, uint8_t alignment,
	uint16_t *ptr_bitset // this is reversed
){
	uint32_t index = global_bc_size;
	Node *top = global_bc + index;
	*top = (DataHeader){
		.bytesize = data_size, .alignment = alignment, .flags = DataFlag_Pointered
	};
	memcpy(top+1, data, data_size);
	size_t ptr_cap = data_size / PTR_SIZE;
	size_t ptr_bufs = (ptr_cap - 1 + 3) / 4;
	global_bc_size = index + ptr_bufs + 1 + (data_size + sizeof(BcNode) - 1)/sizeof(BcNode);
	for (size_t i=0; i!=ptr_bufs; i+=1){ top->ptr_bitset[-(int)i] = ptr_bitset[-(int)i]; }	
	return index;
}
