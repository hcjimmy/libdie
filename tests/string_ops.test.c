/* Tests for string module.
 * Copyright (C) 2023  hcjimmy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "string_ops.test.h"
#include "../string_ops.h"


#include <stdbool.h>
#include <stdio.h>

int test_str_section_to_unsigned(char *start, char *end,
		unsigned ex_result, bool ex_invalid_char)
{
	int failed;
	unsigned result;
	bool invalid_char;

	failed = 0;
	result = str_section_to_unsigned(start, end, &invalid_char);

	if(result != ex_result) {
		fprintf(stderr, "Error: expecting result %u, but received %u.\n",
				ex_result, result);
		failed = 1;
	}
	if(invalid_char != ex_invalid_char) {
		fprintf(stderr, "Error: expecting invalid char to be %s, but was %s",
				ex_invalid_char ? "true" : "false",
				invalid_char ? "true" : "false");
		failed = 1;
	}

	return failed;
}

int str_section_to_unsigned_tester()
{
	int fails;
	char *exp;

	exp = "hello world";
	fails = test_str_section_to_unsigned(exp, exp+4, 1, true);
	fails += test_str_section_to_unsigned(exp, exp+8, 1, true);

	exp = "32";
	fails += test_str_section_to_unsigned(exp, exp+3, 1, true);

	exp = "123a";
	fails += test_str_section_to_unsigned(exp, &exp[3], 123, false);
	fails += test_str_section_to_unsigned(exp, &exp[2], 12, false);

	exp = "05456";
	fails += test_str_section_to_unsigned(exp, &exp[5], 5456, false);

	return fails;
}

