/*
 * Copyright 2014	Stephan M. Guenther <moepi@moepi.net>
 * 			Maximilian Riemensberger <riemensberger@tum.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See COPYING for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gf.h"
#include "gf16.h"

#if GF16_POLYNOMIAL == 19
#include "gf16tables19.h"
#else
#error "Invalid prime polynomial or tables not available."
#endif

static const uint8_t inverses[GF16_SIZE] = GF16_INV_TABLE;
static const uint8_t pt[GF16_SIZE][GF16_EXPONENT] = GF16_POLYNOMIAL_DIV_TABLE;
static const uint8_t alogt[2*GF16_SIZE-1] = GF16_ALOG_TABLE;
static const uint8_t logt[GF16_SIZE] = GF16_LOG_TABLE;
static const uint8_t multab[GF16_SIZE][256] = GF16_LOOKUP_TABLE;

inline uint8_t
inv16(uint8_t element)
{
	return inverses[element];
}

void
maddrc16_imul_scalar(uint8_t* region1, const uint8_t* region2,
					uint8_t constant, int length)
{
	const uint8_t *p = pt[constant];
	uint8_t r[4];

	if (constant == 0)
		return;

	if (constant == 1) {
		xorr_scalar(region1, region2, length);
		return;
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
maddrc16_imul_gpr32(uint8_t* region1, const uint8_t* region2, uint8_t constant,
								int length)
{
	const uint8_t *p = pt[constant];
	uint32_t r64[4];

	if (constant == 0)
		return;

	if (constant == 1) {
		xorr_gpr32(region1, region2, length);
		return;
	}

	for (; length > 0; region1+=4, region2+=4, length-=4) {
		r64[0] = ((*(uint32_t *)region2 & 0x11111111)>>0)*p[0];
		r64[1] = ((*(uint32_t *)region2 & 0x22222222)>>1)*p[1];
		r64[2] = ((*(uint32_t *)region2 & 0x44444444)>>2)*p[2];
		r64[3] = ((*(uint32_t *)region2 & 0x88888888)>>3)*p[3];
		*((uint32_t *)region1) ^= r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
}

void
maddrc16_imul_gpr64(uint8_t* region1, const uint8_t* region2, uint8_t constant,
								int length)
{
	const uint8_t *p = pt[constant];
	uint64_t r64[4];

	if (constant == 0)
		return;

	if (constant == 1) {
		xorr_gpr64(region1, region2, length);
		return;
	}

	for (; length > 0; region1+=8, region2+=8, length-=8) {
		r64[0] = ((*(uint64_t *)region2 & 0x1111111111111111)>>0)*p[0];
		r64[1] = ((*(uint64_t *)region2 & 0x2222222222222222)>>1)*p[1];
		r64[2] = ((*(uint64_t *)region2 & 0x4444444444444444)>>2)*p[2];
		r64[3] = ((*(uint64_t *)region2 & 0x8888888888888888)>>3)*p[3];
		*((uint64_t *)region1) ^= r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
}

void
maddrc16_flat_table(uint8_t* region1, const uint8_t* region2, uint8_t constant, 
								int length)
{
	if (constant == 0)
		return;

	if (constant == 1) {
		xorr_scalar(region1, region2, length);
		return;
	}

	for (; length; region1++, region2++, length--) {
		*region1 ^= multab[constant][*region2];
	}
}

void
maddrc16_log_table(uint8_t* region1, const uint8_t* region2, uint8_t constant, 
								int length)
{
	uint8_t l;
	uint8_t tmp,r;
	int x;

	if (constant == 0)
		return;

	if (constant == 1) {
		xorr_scalar(region1, region2, length);
		return ;
	}

	l = logt[constant];

	for (; length; region1++, region2++, length--) {
		tmp = *region2 >> 4;

		r = 0;
		if (tmp) {
			x = l + logt[tmp];
			r = alogt[x] << 4;
		}

		tmp = *region2 & 0x0f;
		if (tmp) {
			x = l + logt[tmp];
			r |= alogt[x];
		}

		*region1 ^= r;
	}
}

void
mulrc16_imul_scalar(uint8_t *region, uint8_t constant, int length)
{
	const uint8_t *p = pt[constant];
	uint8_t r[4];

	if (constant == 0) {
		memset(region, 0, length);
		return;
	}

	if (constant == 1)
		return;

	for (; length; region++, length--) {
		r[0] = ((*region & 0x11) >> 0) * p[0];
		r[1] = ((*region & 0x22) >> 1) * p[1];
		r[2] = ((*region & 0x44) >> 2) * p[2];
		r[3] = ((*region & 0x88) >> 3) * p[3];
		*region = r[0] ^ r[1] ^ r[2] ^ r[3];
	}
}

void
mulrc16_imul_gpr32(uint8_t *region, uint8_t constant, int length)
{
	const uint8_t *p = pt[constant];
	uint32_t r64[4];

	if (constant == 0) {
		memset(region, 0, length);
		return;
	}

	if (constant == 1)
		return;

	for (; length > 0; region+=4, length-=4) {
		r64[0] = ((*(uint32_t *)region & 0x11111111)>>0)*p[0];
		r64[1] = ((*(uint32_t *)region & 0x22222222)>>1)*p[1];
		r64[2] = ((*(uint32_t *)region & 0x44444444)>>2)*p[2];
		r64[3] = ((*(uint32_t *)region & 0x88888888)>>3)*p[3];
		*((uint32_t *)region) = r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
}

void
mulrc16_imul_gpr64(uint8_t *region, uint8_t constant, int length)
{
	const uint8_t *p = pt[constant];
	uint64_t r64[4];

	if (constant == 0) {
		memset(region, 0, length);
		return;
	}

	if (constant == 1)
		return;

	for (; length > 0; region+=8, length-=8) {
		r64[0] = ((*(uint64_t *)region & 0x1111111111111111)>>0)*p[0];
		r64[1] = ((*(uint64_t *)region & 0x2222222222222222)>>1)*p[1];
		r64[2] = ((*(uint64_t *)region & 0x4444444444444444)>>2)*p[2];
		r64[3] = ((*(uint64_t *)region & 0x8888888888888888)>>3)*p[3];
		*((uint64_t *)region) = r64[0] ^ r64[1] ^ r64[2] ^ r64[3];
	}
}

