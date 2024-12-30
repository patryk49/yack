#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>


static size_t utf8_leading_ones_u8(uint8_t n){
	n = ~n;
	uint32_t res = 8;
	uint32_t m;
	m = n >>  4u; if (m != 0) { res -=  4; n = m; }
	m = n >>  2u; if (m != 0) { res -=  2; n = m; }
	m = n >>  1u; if (m != 0) return res - 2;
	return res - n;
}

static size_t utf8_codesize(uint32_t code){
	if (code <= 0x7fu)     return 1;
	if (code <= 0x7ffu)    return 2;
	if (code <= 0xffffu)   return 3;
	if (code <= 0x10ffffu) return 4;
	return SIZE_MAX;
}



static size_t utf8_write(uint8_t *out_iter, uint32_t code){
	if (code <= 0x7fu){
		out_iter[0] = code;
		return 1;
	}
	if (code <= 0x7ffu){
		out_iter[0] = ((code >> 6) & 0x1fu) | 0xc0u;
		out_iter[1] = ((code >> 0) & 0x3fu) | 0x80u;
		return 2;
	}
	if (code <= 0xffffu){
		out_iter[0] = ((code >> 12) & 0x0fu) | 0xe0u;
		out_iter[1] = ((code >>  6) & 0x3fu) | 0x80u;
		out_iter[2] = ((code >>  0) & 0x3fu) | 0x80u;
		return 3;
	}
	if (code <= 0x10ffffu){
		out_iter[0] = ((code >> 18) & 0x07u) | 0xf0u;
		out_iter[1] = ((code >> 12) & 0x3fu) | 0x80u;
		out_iter[2] = ((code >>  6) & 0x3fu) | 0x80u;
		out_iter[3] = ((code >>  0) & 0x3fu) | 0x80u;
		return 4;
	}
	return SIZE_MAX;
}



static uint32_t utf8_read(const uint8_t *it){
	uint32_t res = (uint8_t)it[0];
	if (res <= 0x7fu) return res;

	size_t n = utf8_leading_ones_u8((uint8_t)res);
	if (n == 1 || n > 4) return UINT32_MAX;
	res &= 0xffu >> n;

	const uint8_t *end = it + n - 1;
	do{
		it += 1;
		res <<= 6;
		if (((uint8_t)*it >> 6) != 2) return UINT32_MAX;
		res |= *it & 0x7fu;
	} while (it != end);
	
	return res;
}

static uint32_t utf8_read_unchecked(const uint8_t *it){
	uint32_t res = (uint8_t)it[0];
	if (res <= 0x7fu) return res;

	size_t n = utf8_leading_ones_u8((uint8_t)res);
	res &= 0xffu >> n;

	const uint8_t *end = it + n - 1;
	do{
		it += 1;
		res <<= 6;
		res |= *it & 0x7fu;
	} while (it != end);
	
	return res;
}



static uint32_t utf8_parse(const uint8_t **iter){
	const uint8_t *it = *iter;
	uint32_t res = (uint8_t)it[0];

	if (res <= 0x7fu){
		*iter = it + 1;
		return res;
	}

	size_t n = utf8_leading_ones_u8((uint8_t)res);
	if (n == 1 || n > 4) return UINT32_MAX;
	res &= 0xffu >> n;

	const uint8_t *end = it + n - 1;
	do{
		it += 1;
		res <<= 6;
		if (((uint8_t)*it >> 6) != 2) return UINT32_MAX;
		res |= *it & 0x7fu;
	} while (it != end);
	
	*iter = it + 1;
	return res;
}

static uint32_t utf8_parse_unchecked(const uint8_t **iter){
	const uint8_t *it = *iter;
	uint32_t res = (uint8_t)it[0];

	if (res <= 0x7fu){
		*iter = it + 1;
		return res;
	}

	size_t n = utf8_leading_ones_u8((uint8_t)res);
	res &= 0xffu >> n;

	const uint8_t *end = it + n - 1;
	do{
		it += 1;
		res <<= 6;
		res |= *it & 0x7fu;
	} while (it != end);
	
	*iter = it + 1;
	return res;
}



static size_t utf8_sizecheck(const uint8_t *it){
	if ((uint8_t)it[0] <= 0x7fu) return 1;

	size_t n = utf8_leading_ones_u8((uint8_t)*it);
	if (n == 1 || n > 4) return 0;

	const uint8_t *end = it + n;
	while (it != end){
		it += 1;
		if (((uint8_t)*it >> 6) != 2) return 0;
	}
	return n;
}
