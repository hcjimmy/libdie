/* String parsing functions - header.
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
#pragma once

#include <stdbool.h>
#include <string.h>

/* Return true if ch equals any character in chars. */
bool equals_any(const char ch, const char *chars);

/* Return pointer to the next occurance of any character in chars within str.
 * Currently implemented as a macro. */
char* get_next_in_chars(char* str, const char* chars);
// Implementation:
#define get_next_in_chars(str, chars) ((str) + strcspn((str), (chars)))

/* Return pointer to the next char in str that is not in chars.
 * Currently implemented as a macro. */
char* get_next_non_pchars(char* str, const char* chars);
// Implementation:
#define get_next_non_pchars(str, chars) ((str) + strspn((str), (chars)))

/* Wrapper of strtod that does not accept initial '+', '-' or spaces. */
double strtod_noprefix(char *nptr, char **endptr);

/* Convert string to unsigned, not accepting prefix.
 * On failure (invalid char) *invalid_char is set to true and 1 is returned. */
unsigned str_section_to_unsigned(const char *start, const char *end, bool *invalid_char);

/* Convert double to string with the trailing 0s removed.
 *
 * number	The number to convert.
 * precision	Maximum number of digits after the dot.
 *			SHOULD NOT BE 0!
 * buf		The buffer the number is converted into.
 * 		The length of the buffer must be at least the result of
 * 			snprintf(NULL, 0, "%.<precision>lf", number) + 1.
 * 		Or (calculating manually):
 *			If -1 < number < 1:	 1 + precision + 2 (+1 if negative).
 *			Else: 	log10(|number|) + precision + 3 (+1 if negative).
 */
char* stringify_double(const double number, const unsigned precision, char *buf);

/* Similar to sprintf, only recieve a pointer to pointer instead of pointer.
 * After the call, *p_buf is moved to the last char written ('\0'). */
void sprintf_move(char **p_buf, const char *format, ...);

#define STRINGER_INNER(str) #str 
#define STRINGER(str) STRINGER_INNER(str)

