/*
 * This is a library providing arithmetic functions on GF(2^1) and GF(2^8).
 * Copyright (C) 2013  Alexander Kurtz <alexander@kurtz.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stddef.h>

#include "gf16.h"

static pthread_once_t _init_done_ = PTHREAD_ONCE_INIT;
static uint8_t inverses[GF16_SIZE];
static uint8_t mul[GF16_SIZE][GF16_SIZE];
static uint8_t pt[16][4];

uint8_t 
ffadd16(const uint8_t summand1, const uint8_t summand2)
{
	return summand1 ^ summand2;
}

uint8_t
ffdiv16(uint8_t dividend, const uint8_t divisor)
{
	ffmul16_region_c(&dividend, inverses[divisor], 1);
	return dividend;
}

uint8_t
ffmul16(uint8_t factor1, const uint8_t factor2)
{
	uint8_t r[4], *p;

	if (factor2 == 0)
		return 0;

	if (factor2 == 1)
		return factor1;
	
	p = pt[factor2];

	r[0] = (factor1 & 1) ? p[0] : 0;
	r[1] = (factor1 & 2) ? p[1] : 0;
	r[2] = (factor1 & 4) ? p[2] : 0;
	r[3] = (factor1 & 8) ? p[3] : 0;
	factor1 = r[0] ^ r[1] ^ r[2] ^ r[3];

	return factor1;
}

void
ffadd16_region(uint8_t* region1, const uint8_t* region2, const int length)
{
	ffxor_region(region1, region2, length);
}

void
ffdiv16_region_c(uint8_t* region, const uint8_t constant, const int length)
{
	ffmul16_region_c(region, inverses[constant], length);
}

void
ffmadd16_region_c_slow(uint8_t* region1, const uint8_t* region2, 
					const uint8_t constant, int length)
{
	uint8_t r[4], *p;
	
	if (constant == 0)
		return;

	if (constant == 1) {
		ffxor_region(region1, region2, length);
		return;
	}

	p = pt[constant];
	
	for (; length; region1++, region2++, length--) {
		r[0] = ((*region2 & 0x11) >> 0) * p[0];
		r[1] = ((*region2 & 0x22) >> 1) * p[1];
		r[2] = ((*region2 & 0x44) >> 2) * p[2];
		r[3] = ((*region2 & 0x88) >> 3) * p[3];
		*region1 ^= r[0] ^ r[1] ^ r[2] ^ r[3];
	}
}

void
ffmadd16_region_c(uint8_t* region1, const uint8_t* region2, 
					const uint8_t constant, int length)
{
	uint64_t r64[4];
	uint8_t r[4], *p;

	if (constant == 0)
		return;

	if (constant == 1) {
		ffxor_region(region1, region2, length);
		return;
	}
	
	p = pt[constant];

#if __GNUC_PREREQ(4,7)
#if defined __SSE4_1__
	register __m128i in1, in2, out, table1, table2, mask1, mask2, l, h;
	table1 = _mm_loadu_si128((void *)mul[constant]);
	table2 = _mm_slli_epi64(table1, 4);
	mask1 = _mm_set1_epi8(0x0f);
	mask2 = _mm_set1_epi8(0xf0);

	for (; length & 0xffffffff0; region1+=16, region2+=16, length-=16) {
		in2 = _mm_load_si128((void *)region2);
		in1 = _mm_load_si128((void *)region1);
		l = _mm_and_si128(in2, mask1);
		l = _mm_shuffle_epi8(table1, l);
		h = _mm_and_si128(in2, mask2);
		h = _mm_srli_epi64(h, 4);
		h = _mm_shuffle_epi8(table2, h);
		out = _mm_xor_si128(h,l);
		out = _mm_xor_si128(out, in1);
		_mm_store_si128((void *)region1, out);
	}
#elif defined __SSE2__
	register __m128i reg1, reg2, ri[4], sp[4], mi[4];
	mi[0] = _mm_set1_epi8(0x11);
	mi[1] = _mm_set1_epi8(0x22);
	mi[2] = _mm_set1_epi8(0x44);
	mi[3] = _mm_set1_epi8(0x88);
	sp[0] = _mm_set1_epi16(p[0]);
	sp[1] = _mm_set1_epi16(p[1]);
	sp[2] = _mm_set1_epi16(p[2]);
	sp[3] = _mm_set1_epi16(p[3]);

	for (; length & 0xfffffff0; region1+=16, region2+=16, length-=16) {
		reg2 = _mm_load_si128((void *)region2);
		reg1 = _mm_load_si128((void *)region1);
		ri[0] = _mm_and_si128(reg2, mi[0]);
		ri[1] = _mm_and_si128(reg2, mi[1]);
		ri[2] = _mm_and_si128(reg2, mi[2]);
		ri[3] = _mm_and_si128(reg2, mi[3]);
		ri[1] = _mm_srli_epi16(ri[1], 1);
		ri[2] = _mm_srli_epi16(ri[2], 2);
		ri[3] = _mm_srli_epi16(ri[3], 3);
		ri[0] = _mm_mullo_epi16(ri[0], sp[0]);
		ri[1] = _mm_mullo_epi16(ri[1], sp[1]);
		ri[2] = _mm_mullo_epi16(ri[2], sp[2]);
		ri[3] = _mm_mullo_epi16(ri[3], sp[3]);
		ri[0] = _mm_xor_si128(ri[0], ri[1]);
		ri[2] = _mm_xor_si128(ri[2], ri[3]);
		ri[0] = _mm_xor_si128(ri[0], ri[2]);
		ri[0] = _mm_xor_si128(ri[0], reg1);
		_mm_store_si128((void *)region1, ri[0]);
	}
#endif
#endif
	
	for (; length & 0xffffffff8; region1+=8, region2+=8, length-=8) {
		r64[0] = ((*(uint64_t *)region2 & 0x1111111111111111)>>0)*p[0];
		r64[1] = ((*(uint64_t *)region2 & 0x2222222222222222)>>1)*p[1];
		r64[2] = ((*(uint64_t *)region2 & 0x4444444444444444)>>2)*p[2];
		r64[3] = ((*(uint64_t *)region2 & 0x8888888888888888)>>3)*p[3];
		*((uint64_t *)region1) ^= r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
	
	for (; length; region1++, region2++, length--) {
		r[0] = ((*region2 & 0x11) >> 0) * p[0];
		r[1] = ((*region2 & 0x22) >> 1) * p[1];
		r[2] = ((*region2 & 0x44) >> 2) * p[2];
		r[3] = ((*region2 & 0x88) >> 3) * p[3];
		*region1 ^= r[0] ^ r[1] ^ r[2] ^ r[3];
	}
}

void
ffmul16_region_c_slow(uint8_t *region, const uint8_t constant, int length)
{
	uint8_t r[4], *p;
	
	if (constant == 0) {
		memset(region, 0, length);
		return;
	}

	if (constant == 1)
		return;

	p = pt[constant];
	
	for (; length; region++, length--) {
		r[0] = ((*region & 0x11) >> 0) * p[0];
		r[1] = ((*region & 0x22) >> 1) * p[1];
		r[2] = ((*region & 0x44) >> 2) * p[2];
		r[3] = ((*region & 0x88) >> 3) * p[3];
		*region = r[0] ^ r[1] ^ r[2] ^ r[3];
	}
}

void
ffmul16_region_c(uint8_t *region, const uint8_t constant, int length)
{
	uint8_t r[4], *p;
	uint64_t r64[4];

	if (constant == 0) {
		memset(region, 0, length);
		return;
	}

	if (constant == 1)
		return;

	p = pt[constant];

#if __GNUC_PREREQ(4,7)
#if defined __SSE4_1__
	register __m128i in, out, t1, t2, m1, m2, l, h;
	t1 = _mm_loadu_si128((void *)mul[constant]);
	t2 = _mm_slli_epi64(t1, 4);
	m1 = _mm_set1_epi8(0x0f);
	m2 = _mm_set1_epi8(0xf0);

	for (; length & 0xfffffff0; region+=16, length-=16) {
		in = _mm_load_si128((void *)region);
		l = _mm_and_si128(in, m1);
		l = _mm_shuffle_epi8(t1, l);
		h = _mm_and_si128(in, m2);
		h = _mm_srli_epi64(h, 4);
		h = _mm_shuffle_epi8(t2, h);
		out = _mm_xor_si128(h,l);
		_mm_store_si128((void *)region, out);
	}
#elif defined __SSE2__
	register __m128i reg, ri[4], sp[4], mi[4];
	mi[0] = _mm_set1_epi8(0x11);
	mi[1] = _mm_set1_epi8(0x22);
	mi[2] = _mm_set1_epi8(0x44);
	mi[3] = _mm_set1_epi8(0x88);
	sp[0] = _mm_set1_epi16(p[0]);
	sp[1] = _mm_set1_epi16(p[1]);
	sp[2] = _mm_set1_epi16(p[2]);
	sp[3] = _mm_set1_epi16(p[3]);

	for (; length & 0xfffffff0; region+=16, length-=16) {
		reg = _mm_load_si128((void *)region);
		ri[0] = _mm_and_si128(reg, mi[0]);
		ri[1] = _mm_and_si128(reg, mi[1]);
		ri[2] = _mm_and_si128(reg, mi[2]);
		ri[3] = _mm_and_si128(reg, mi[3]);
		ri[1] = _mm_srli_epi16(ri[1], 1);
		ri[2] = _mm_srli_epi16(ri[2], 2);
		ri[3] = _mm_srli_epi16(ri[3], 3);
		ri[0] = _mm_mullo_epi16(ri[0], sp[0]);
		ri[1] = _mm_mullo_epi16(ri[1], sp[1]);
		ri[2] = _mm_mullo_epi16(ri[2], sp[2]);
		ri[3] = _mm_mullo_epi16(ri[3], sp[3]);
		ri[0] = _mm_xor_si128(ri[0], ri[1]);
		ri[2] = _mm_xor_si128(ri[2], ri[3]);
		ri[0] = _mm_xor_si128(ri[0], ri[2]);
		_mm_store_si128((void *)region, ri[0]);
	}
#endif
#endif

	for (; length & 0xffffffff8; region+=8, length-=8) {
		r64[0] = ((*(uint64_t *)region & 0x1111111111111111)>>0)*p[0];
		r64[1] = ((*(uint64_t *)region & 0x2222222222222222)>>1)*p[1];
		r64[2] = ((*(uint64_t *)region & 0x4444444444444444)>>2)*p[2];
		r64[3] = ((*(uint64_t *)region & 0x8888888888888888)>>3)*p[3];
		*((uint64_t *)region) = r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
	
	for (; length; region++, length--) {
		r[0] = ((*region & 0x11) >> 0) * p[0];
		r[1] = ((*region & 0x22) >> 1) * p[1];
		r[2] = ((*region & 0x44) >> 2) * p[2];
		r[3] = ((*region & 0x88) >> 3) * p[3];
		*region = r[0] ^ r[1] ^ r[2] ^ r[3];
	}
}

static void
init()
{
	int i, j;
	
	for (i=0; i<16; i++) {	
		pt[i][0] = i;
		for (j=1; j<8; j++) {
			pt[i][j] = (pt[i][j-1] << 1) & 0x0f;
			if (pt[i][j-1] & 0x08)
				pt[i][j] ^= (GF16_PRIMITIVE_POLYNOMIAL & 0x0f);
		}
	}

	for(i = 0; i < 16; i++){
		for(j = 0; j < 16; j++){
			mul[i][j] = ffmul16(i,j);
			if(mul[i][j] == 1){
				inverses[i] = j;
			}
		}
	}
	
}

void
gf16_init()
{
	pthread_once(&_init_done_, init);
}
