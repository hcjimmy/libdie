/* libdie - a dice-calculation library - main header.
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

/* Calculate dice expression (like "d20+2" or "2d4+5/d10^d3") into a number after rolling the dice along
 * with an optional calculation string. */

#pragma once

#include "list/list.h"
#include "list_defs.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>


/* Contain the type of error and any data associated with it. */
typedef struct Dierror {
	enum dierror_type {
		invalid_operator,

		invalid_num,
		missing_num,

		invalid_reps,
		zero_reps,
		invalid_sides,
		non_existant_sides,
		zero_sides,

		unclosed_parenthesis,
		invalid_parenthesis,
		empty_expression,

		end_of_list		// (not an error but marks the end of the list).
	} type;

	const char *invalid_section_start;	// Should be NULL if no section should be printed.
	const char *invalid_section_end;	// Should point at the char after the last invalid character.
} Dierror;

struct Die {
	unsigned repetitions;
	int sides;	// >= 1
};

// Struct to contain either a number, die, or operation.
struct NumSection {
	enum { type_num, type_die, type_op } type;
	union {
		double num;
		struct Die die;
		void *operation;
	} data;
};

// Define lists
named_list_def_proto(struct Dierror, Dierror)
named_list_def_proto(struct NumSection, NumSection)

/* Contain a list of binary operations:
 *
 * numbers are the numbers/dice/Op to be operated.
 * While operators are a list of operators between them.
 *
 * The operators should be of the same, or decreasing precedence (eg. `1^3/4*2-1+12`), otherwise
 * the next number will be of type op, and should be called recursively to get the value of each number
 * (eg `1+op(3*4)`). */
struct Operation {
	unsigned parenthesis :1;
	struct NumSection_list numbers;
	struct char_list operators;
	char prefix;
};

struct Operation* exp_to_op(char *dice_exp, struct Dierror **errors);
/* Convert dice-expression to struct operation: a recusive struct representing the calculation
 * with the dice unrolled.
 *
 * On success:
 * 	A pointer to the operation is returned (which may be used with the functions below),
 * 		and *errors is set to NULL.
 * 	The returned pointer must be freed with clear_operation_pointer.
 *
 * On error:
 * 	The return value is always NULL.
 * 	
 * 	If a memory allocation error occured: *errors is also set to NULL.
 * 	Otherwise:
 * 		*errors is set to an array of errors terminated by dierror of type end_of_list.
 * 			(fprint_dierrors is available to print the array).
 * 		*errors must be freed with free.
 *
 * In each case, dice_exp will remain unmodified.
 */


size_t get_calc_string_length(const struct Operation *operation);
/* Return the needed length of the calc_string buffer optionally used by operate below.
 * (The maximum length required to represent the operation as a string.) */


double operate(const struct Operation *operation, char *calc_string, short flags);
/* Calculate operation into a number and optional string representing the calculation.
 *
 * pre:
 * 	operation is returned from exp_to_op, operation != NULL.
 * 	calc_string is either NULL (if calculation string is undesired), or a buffer
 *		the size of get_calc_string_length(operation) or bigger.
 * 	flags is a bit mask, and can be any of the following (giving other values may result in undefined behaivor): */
#define NO_FLAG	0
#define COLLAPSE_DICE 1	// 2*3d6 -> "2*11" instead of "2*(3+2+6)". Note 2+3d6 will still result in "2+3+2+6"
/*				(assuming 3,2,6 are rolled).
 * post:
 * 	operation is unmodified.
 * 	If calc_string != NULL, it is set to a '\0' terminated string representing the calculation.
 * 	The result of the calculation is returned.
 */


void clear_operation_pointer(struct Operation *operation);
/* Free memory associated with operation. */
void clear_num_section(struct NumSection section);
/* Free memory associated with section (not really needed in most cases outside this library). */


bool is_single_num_operation(struct Operation *operation);
/* Return true if operation contains only a single number or a dice to be rolled-once, with no operators.
 *
 * Examples:
 * 	"d20", "1d12", "42" would return true.
 * 	"2d10", "d20+1", "3+3" would return false.
 *
 * Note: operation must be returned from exp_to_op. */

