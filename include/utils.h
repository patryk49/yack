#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define SIZE(arr) (sizeof(arr)/sizeof(*arr))

#ifndef CAT
	#define CAT1(a, b) a##b
	#define CAT(a, b) CAT1(a, b)
#endif

#if 0
	#define LIKELY      [[likely]]
	#define UNLIKELY    [[unlikely]]
	#define FALLTHROUGH [[fallthrough]]
#else
	#define LIKELY
	#define UNLIKELY
	#define FALLTHROUGH
#endif


static size_t util_roundup(size_t value, size_t step){
	return (value + step - 1u) / step;
}

static void *util_alignptr(void *ptr, size_t alignment){
	return (void *)(((uintptr_t)ptr - 1u + alignment) & -alignment);
}

static uint64_t util_alignsize(uint64_t size, size_t alignment){
	return (size + alignment - 1u) & -alignment;
}



// ARITHMETIC OPERATIONS
#define UTIL_BASIC_ARITHMETIC_LIST \
	X(u8,    uint8_t) \
	X(u16,   uint16_t) \
	X(u32,   uint32_t) \
	X(u64,   uint64_t) \
	X(i8,    int8_t) \
	X(i16,   int16_t) \
	X(i32,   int32_t) \
	X(i64,   int64_t) \
	X(f32,   float) \
	X(f64,   double) \
	X(char,  char) \
	X(usize, size_t)

#define X(S, T) \
	static T util_min_##S(T n, T m){ return n<m ? n : m; }
UTIL_BASIC_ARITHMETIC_LIST
#undef X

#define X(S, T) \
	static T util_max_##S(T n, T m){ return n<m ? m : n; }
UTIL_BASIC_ARITHMETIC_LIST
#undef X

#define X(S, T) \
	static T util_clamp_##S(T n, T min, T max){ return n<min ? min : (n>max ? max : n); }
UTIL_BASIC_ARITHMETIC_LIST
#undef X

#define X(S, T) \
	static T util_ipow_##S(T base, size_t exp){ \
		T fact = (T)1; \
		if (exp == 0) return fact; \
		while (exp != 1){ \
			if (exp & 1){ fact *= base; } \
			base *= base; \
			exp >>= 1u; \
		} return base * fact; \
	}
UTIL_BASIC_ARITHMETIC_LIST
#undef X

#define X(S, T) \
	static _Bool util_contains_##S(const T *arr, size_t arr_size, T value){ \
		for (size_t i=0; i!=arr_size; i+=1) if (arr[i] == value) return 1; \
		return 0; \
	}
UTIL_BASIC_ARITHMETIC_LIST
#undef X

#undef UTIL_BASIC_ARITHMETIC_LIST



// SIGNED ARITHMETIC OPERATIONS
static int8_t  util_abs_i8 (int8_t  n){ return n<0 ? -n : n; }
static int16_t util_abs_i16(int16_t n){ return n<0 ? -n : n; }
static int32_t util_abs_i32(int32_t n){ return n<0 ? -n : n; }
static int64_t util_abs_i64(int64_t n){ return n<0 ? -n : n; }
static float   util_abs_f32(float   x){ return x<0.f ? -x : x; }
static double  util_abs_f64(double  x){ return x<0.0 ? -x : x; }

static bool util_sign_f32(float x){
	union{ float f; uint32_t n; } u; u.f = x;
	return u.n >> 31u;
}
static bool util_sign_f64(double x){
	union{ double f; uint64_t n; } u; u.f = x;
	return u.n >> 63u;
}
static uint32_t util_exponent_f32(float x){
	union{ float f; uint32_t n; } u; u.f = x;
	return ((u.n >> 23) & 0xff) - 127;
}
static uint64_t util_exponent_f64(double x){
	union{ double f; uint64_t n; } u; u.f = x;
	return ((u.n >> 52) & 0x7ff) - 1023;
}





// some algorithm's are from Hacker's Delight book
static bool util_is_power2_u32(uint32_t n){
	return (n & (n - 1)) == 0;
}

static uint32_t util_popcount_u32(uint32_t n){
    n = (n & 0x55555555u) + ((n >>  1) & 0x55555555u);
    n = (n & 0x33333333u) + ((n >>  2) & 0x33333333u);
    n = (n & 0x0f0f0f0fu) + ((n >>  4) & 0x0f0f0f0fu);
    n = (n & 0x00ff00ffu) + ((n >>  8) & 0x00ff00ffu);
    n = (n & 0x0000ffffu) + ((n >> 16) & 0x0000ffffu);
    return n;
}

static uint32_t util_leading_zeros_u32(uint32_t n){
	uint32_t res = 32;
	uint32_t m;
	m = n >> 16u; if (m != 0) { res -= 16; n = m; }
	m = n >>  8u; if (m != 0) { res -=  8; n = m; }
	m = n >>  4u; if (m != 0) { res -=  4; n = m; }
	m = n >>  2u; if (m != 0) { res -=  2; n = m; }
	m = n >>  1u; if (m != 0) return res - 2;
	return res - n;
}

static uint32_t util_trailing_zeros_u32(uint32_t n){
	n &= -n;
	uint32_t bz = n == 0;
	uint32_t b4 = ((n & 0x0000ffffu) != 0) ? 0 : 16;
	uint32_t b3 = ((n & 0x00ff00ffu) != 0) ? 0 :  8;
	uint32_t b2 = ((n & 0x0f0f0f0fu) != 0) ? 0 :  4;
	uint32_t b1 = ((n & 0x33333333u) != 0) ? 0 :  2;
	uint32_t b0 = ((n & 0x55555555u) != 0) ? 0 :  1;
	return bz + b4 + b3 + b2 + b1 + b0;
}

static uint32_t util_leading_ones_u32(uint32_t n){
	return util_leading_zeros_u32(~n);
}

static uint32_t util_trailing_ones_u32(uint32_t n){
	return util_trailing_zeros_u32(~n);
}

static uint32_t util_bitwidth_u32(uint32_t n){
	uint32_t res = 0;
	uint32_t m;
	m = n >> 16u; if (m != 0) { res += 16; n = m; }
	m = n >>  8u; if (m != 0) { res +=  8; n = m; }
	m = n >>  4u; if (m != 0) { res +=  4; n = m; }
	m = n >>  2u; if (m != 0) { res +=  2; n = m; }
	m = n >>  1u; if (m != 0) return res + 2;
	return res + n;
}

static uint32_t util_bitfloor_u32(uint32_t n){
	if (n == 0) return 0;
	return 1u << (util_bitwidth_u32(n) - 1);
}

static uint32_t util_bitceil_u32(uint32_t n){
	if (n <= 1) return 1;
	return 1u << util_bitwidth_u32(n - 1);
}

uint32_t util_digitwidth_u32(uint32_t n){
	uint32_t res = 0;
	uint32_t m;
	m = n / 100000000u; if (m != 0) { res += 8; n = m; }
	m = n / 10000u;     if (m != 0) { res += 4; n = m; }
	m = n / 100u;       if (m != 0) { res += 2; n = m; }
	m = n / 10u;        if (m != 0) return res + 2;
	return res + (n != 0);
}

static uint32_t util_hexwidth_u32(uint32_t n){
	uint32_t res = 0;
	uint32_t m;
	m = n >> 16u; if (m != 0) { res += 4; n = m; }
	m = n >>  8u; if (m != 0) { res += 2; n = m; }
	m = n >>  4u; if (m != 0) return res + 2;
	return res + (n != 0);
}



static bool util_is_power2_u64(uint64_t n){
	return (n & (n - 1)) == 0;
}

static uint64_t util_popcount_u64(uint64_t n){
    n = (n & 0x5555555555555555u) + ((n >>  1) & 0x5555555555555555u);
    n = (n & 0x3333333333333333u) + ((n >>  2) & 0x3333333333333333u);
    n = (n & 0x0f0f0f0f0f0f0f0fu) + ((n >>  4) & 0x0f0f0f0f0f0f0f0fu);
    n = (n & 0x00ff00ff00ff00ffu) + ((n >>  8) & 0x00ff00ff00ff00ffu);
    n = (n & 0x0000ffff0000ffffu) + ((n >> 16) & 0x0000ffff0000ffffu);
    n = (n & 0x00000000ffffffffu) + ((n >> 32) & 0x00000000ffffffffu);
    return n;
}

static uint64_t util_leading_zeros_u64(uint64_t n){
	uint64_t res = 64;
	uint64_t m;
	m = n >> 32; if (m != 0) { res -= 32; n = m; }
	m = n >> 16; if (m != 0) { res -= 16; n = m; }
	m = n >>  8; if (m != 0) { res -=  8; n = m; }
	m = n >>  4; if (m != 0) { res -=  4; n = m; }
	m = n >>  2; if (m != 0) { res -=  2; n = m; }
	m = n >>  1; if (m != 0) return res - 2;
	return res - n;
}

static uint64_t util_trailing_zeros_u64(uint64_t n){
	n &= -n;
	uint64_t bz = n == 0;
	uint64_t b5 = ((n & 0x00000000ffffffffu) != 0) ? 0 : 32;
	uint64_t b4 = ((n & 0x0000ffff0000ffffu) != 0) ? 0 : 16;
	uint64_t b3 = ((n & 0x00ff00ff00ff00ffu) != 0) ? 0 :  8;
	uint64_t b2 = ((n & 0x0f0f0f0f0f0f0f0fu) != 0) ? 0 :  4;
	uint64_t b1 = ((n & 0x3333333333333333u) != 0) ? 0 :  2;
	uint64_t b0 = ((n & 0x5555555555555555u) != 0) ? 0 :  1;
	return bz + b5 + b4 + b3 + b2 + b1 + b0;
}

static uint64_t util_leading_ones_u64(uint64_t n){
	return util_leading_zeros_u64(~n);
}

static uint64_t util_trailing_ones_u64(uint64_t n){
	return util_trailing_zeros_u64(~n);
}

static uint64_t util_bitwidth_u64(uint64_t n){
	uint64_t res = 0;
	uint64_t m;
	m = n >> 32u; if (m != 0) { res += 32; n = m; }
	m = n >> 16u; if (m != 0) { res += 16; n = m; }
	m = n >>  8u; if (m != 0) { res +=  8; n = m; }
	m = n >>  4u; if (m != 0) { res +=  4; n = m; }
	m = n >>  2u; if (m != 0) { res +=  2; n = m; }
	m = n >>  1u; if (m != 0) return res + 2;
	return res + n;
}

static uint64_t util_bitfloor_u64(uint64_t n){
	if (n == 0) return 0;
	return 1u << (util_bitwidth_u64(n) - 1);
}

static uint64_t util_bitceil_u64(uint64_t n){
	if (n <= 1) return 1;
	return 1u << util_bitwidth_u64(n - 1);
}



uint64_t util_digitwidth_u64(uint64_t n){
	uint64_t res = 0;
	uint64_t m;
	m = n / 10000000000000000u; if (m != 0) { res += 16; n = m; }
	m = n / 100000000u;         if (m != 0) { res +=  8; n = m; }
	m = n / 10000u;             if (m != 0) { res +=  4; n = m; }
	m = n / 100u;               if (m != 0) { res +=  2; n = m; }
	m = n / 10u;                if (m != 0) return res + 2;
	return res + (n != 0);
}

static uint64_t util_hexwidth_u64(uint64_t n){
	uint64_t res = 0;
	uint64_t m;
	m = n >> 32u; if (m != 0) { res += 8; n = m; }
	m = n >> 16u; if (m != 0) { res += 4; n = m; }
	m = n >>  8u; if (m != 0) { res += 2; n = m; }
	m = n >>  4u; if (m != 0) return res + 2;
	return res + (n != 0);
}

