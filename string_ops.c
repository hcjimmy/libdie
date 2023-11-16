/* String parsing functions.
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
#include "string_ops.h"
#include "lassert.h"

#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Return true if ch equals any character in chars. */
bool equals_any(const char ch, const char *chars)
{
	for(; *chars != '\0'; chars++)
		if(ch == *chars)
			return true;
	return false;
}

double strtod_noprefix(char *nptr, char **endptr)
{
	if(*nptr == '-' || *nptr == '+' || *nptr == ' ') {
		*endptr = nptr;
		return 0;
	}
	
	return strtod(nptr, endptr);
}

/* Convert section to unsigned, not accepting a modifier.
 * 
 * start: 	where the first numerical character is expected.
 * end:		points after where the last numerical character should be.
 *
 * In case of error, *invalid_char is set to true, and 1 is returned. */
unsigned str_section_to_unsigned(const char *start, const char *end, bool *invalid_char)
{
	lassert(start <= end, ASSERT_LVL_FAST);

	unsigned ret = 0;

	for(; start != end; start++) {
		if(!isdigit(*start)) {
			*invalid_char = true;
			return 1;
		}
		ret = ret*10 + (*start & 0x0F);	// Look at the ascii table in hex, you'll get it.
	}

	*invalid_char = false;
	return ret;
}

char* stringify_double(const double number, const unsigned precision, char *buf)
{
	sprintf_move(&buf, "%.*lf", precision, number);

	// Trim trailing zeros and dot(if it's all zeros).

	while(*--buf == '0')
		;
	if(*buf != '.')
		buf++;
	*buf = '\0';

	return buf;
}

void sprintf_move(char **p_buf, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	const size_t end_index = vsprintf(*p_buf, format, ap);
	va_end(ap);

	*p_buf = *p_buf + end_index;
}
