/*
 * memtester version 4
 *
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 */

#define __version__ "4.3.0"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "types.h"
#include "sizes.h"
#include "tests.h"

#define TEST_NARROW_WRITES

#define EXIT_FAIL_NONSTARTER    0x01
#define EXIT_FAIL_ADDRESSLINES  0x02
#define EXIT_FAIL_OTHERTEST     0x04

#define TEST_LOOPS 1

struct test tests[] = {
	{ "Random Value", test_random_value },
	{ "Compare XOR", test_xor_comparison },
	{ "Compare SUB", test_sub_comparison },
	{ "Compare MUL", test_mul_comparison },
	{ "Compare DIV", test_div_comparison },
	{ "Compare OR", test_or_comparison },
	{ "Compare AND", test_and_comparison },
	{ "Sequential Increment", test_seqinc_comparison },
	{ "Solid Bits", test_solidbits_comparison },
	{ "Block Sequential", test_blockseq_comparison },
	{ "Checkerboard", test_checkerboard_comparison },
	{ "Bit Spread", test_bitspread_comparison },
	{ "Bit Flip", test_bitflip_comparison },
	{ "Walking Ones", test_walkbits1_comparison },
	{ "Walking Zeroes", test_walkbits0_comparison },
#ifdef TEST_NARROW_WRITES
	{ "8-bit Writes", test_8bit_wide_random },
	{ "16-bit Writes", test_16bit_wide_random },
#endif
	{ NULL, NULL }
};

int memtester_main(void *ram_start, void *ram_end)
{
	uint32_t loop, i, size, count;
	ulv *bufa = NULL, *bufb = NULL;
	int32_t ret = 0;

	size = (size_t)ram_end - (size_t)ram_start;
	size &= 0xFFFFFFFFFFFFFFF0;

	count = size / sizeof(ulv) / 2;
	bufa = (ulv *)(ram_start);
	bufb = (ulv *)((size_t)ram_start + size / 2);

	printf("Start Test RAM\n");
	printf("Test BUF A: 0x%p, Size = %d Bytes\n", bufa, size / 2);
	printf("Test BUF B: 0x%p, Size = %d Bytes\n", bufb, size / 2);

	for (loop = 0; loop < TEST_LOOPS; loop++) {
		printf("Loop %u", loop);
		printf(":\n");
		printf("  %-20s: ", "Stuck Address");

		for (i = 0;; i++) {
			if (!tests[i].name) {
				break;
			}

			printf("  %-20s: ", tests[i].name);

			if (!tests[i].fp(bufa, bufb, count)) {
				ret = 0;
				printf("ok\n");
			} else {
				ret = -1;
				printf("failed\n");
			}
		}

		printf("\n");
	}

	printf("Done.\n");

	return ret;
}
