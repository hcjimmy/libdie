/* exp_to_op and it's sub-functions.
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
#include "libdie.h"
#include "lassert.h"
#include "string_ops.h"

// Recursively does the parsing.
bool exp_to_op_rec(struct Operation * const operation, char **dice_exp,
		const bool set_prefix, short last_op_precedence,
		const short parent_last_op_precedence, char *parent_next_operator,
		char *parenthesis_start, char expected_parenthesis,
		struct Dierror_list *error_list);
// Parse section (number, dice, or parenthesis operation).
bool parse_num_section(struct NumSection *out, char **dice_exp,
		struct Dierror_list *error_list);
bool parse_operators(char * const out_operator, char **dice_exp, bool after_parenthesis_section,
		struct Dierror_list *error_list);
// Receive operator and return it's precedence.
short get_operator_precedence(char operator);
// Allocate an operation and initialize it's content.
struct Operation* make_operation(bool parenthesis);
// Make an operation and add initial_num and initial_operator.
struct Operation* make_operation_with_start(bool parenthesis,
		char prefix, struct NumSection initial_num, char initial_operator);

// Return values of exp_to_op_rec
#define ETOP__MEM_FAIL true
#define ETOP__NO_MEM_FAIL false

// Return values of parse_num_section
#define PNS__MEM_FAIL true
#define PNS__NO_MEM_FAIL false

// Legalparenthesis are treated like an operand, but are replaced by '*', (printing parenthesis is done with the flag in Operation).
#define LEGAL_OPERANDS 			"+-/*%^"			// All legal operands
#define LEGAL_PARENTHESIS_OPENING	"([{"				// (Update code below if adding parenthesis)
#define LEGAL_PARENTHESIS_CLOSING	")]}"				// (especially parse_num_section).
#define LEGAL_PARENTHESIS	 	LEGAL_PARENTHESIS_OPENING LEGAL_PARENTHESIS_CLOSING	// All legal parenthesis
#define LEGAL_MODS LEGAL_OPERANDS LEGAL_PARENTHESIS	// All legal characters that are not a number/dice (not [d0-9.]).

#define BELOW_MINIMAL_PRECEDENCE -1
#define PLUS_MINUS_PERCEDENCE 0
#define HIGHEST_PRECEDENCE 2

/* -- Other -- */

/* Receive operator, and return it's precedence.
 *
 * Operator *must* be a char in LEGAL_OPERANDS, if it isn't it is considered a bug
 * and the program will exit. */
short get_operator_precedence(char operator)
{
	static const char operators_array[] =               { '+', '-', '*', '(', '/', '%', '^', '\0'};
	static const short operator_precedence_array[] =    { 0,   0,   1,   1,   1,   1,   2 };

	for(size_t i = 0; operators_array[i]; i++)
		if(operator == operators_array[i])
			return operator_precedence_array[i];
	exit(1);	// Should never happen.
}

/* -- Error handling -- */

/* Add error to error list.
 * Returns true if memory error occures, otherwise false. */
bool add_dierror(struct Dierror_list *errors, enum dierror_type type,
		const char *start, const char *end)
{
	struct Dierror error = {
		.type = type,
		.invalid_section_start = start,
		.invalid_section_end = end 
	};

	if(Dierror_list_append(errors, error))
		return true;

	return false;

}

/* -- Allocating -- */

/* Allocate struct operation with it's content initialized.
 * Returns NULL if a memory allocation error occures. */
struct Operation* make_operation(bool parenthesis)
{
	struct Operation *operation;

	// Allocate it, then allocate the lists.
	operation = malloc(sizeof(*operation));
	if(!operation)
		return NULL;
	if(NumSection_list_init(&operation->numbers)) {
		free(operation);
		return NULL;
	}
	if(char_list_init(&operation->operators)) {
		NumSection_list_close(&operation->numbers, NULL);
		free(operation);
		return NULL;
	}

	operation->parenthesis = parenthesis;
	return operation;
}

/* Creates new operation and adds initial_num and initial_operator
 * to ops list and numbers.
 *
 * If a memory allocation error occures NULL is returned and initial_num is unmodified. */
struct Operation* make_operation_with_start(bool parenthesis, char prefix,
		struct NumSection initial_num, char initial_operator)
{
	struct Operation *operation;

	if((operation = make_operation(parenthesis)) == NULL)
		return NULL;

	if(char_list_append(&operation->operators, initial_operator)
			|| NumSection_list_append(&operation->numbers, initial_num)) {
		clear_operation_pointer(operation);
		return NULL;
	}

	operation->prefix = prefix;

	return operation;
}

/* -- Parsing -- */

/* Parse part of expression expected to be a section of a number, dice, or parenthesis,
 * and return it as NumSection.
 *
 * out:		pointer to the num section that is to be set.
 * dice_exp:	pointer to string containing the expression where the number is expected.
 * 		*dice_exp is guaranteed to be set after the number section, at the first mod
 * 		to occur after it.
 * 			(Note: if starting with parenthesis, it's content will
 * 			be parsed as a sub-operation, and *dice_exp will be set after the closing
 * 			parenthesis if they exist).
 * error_list:	List of dierrors, expected to be initialized.
 *
 *
 * pre:
 * 	*out is modifiable.
 * 	*dice_exp points to where the section is expected to start.
 * 	*dice_exp (the pointer) is modifiable (not neccesarily it's content).
 * 	error_list is initialized.
 *
 * post:
 *	If a memory allocation error occures:
 *		true is returned, *out and dice_exp are undefined.
 *
 *	Otherwise:
 *		*out is set to a value representing the section.
 *		If initially **dice_exp is an opening parenthesis, *dice_exp is set to the matching closing parenthesis if it exists,
 *			or '\0' if it doesn't.
 *			If there's no closing parenthesis, an error is added.
 *		Otherwise *dice_exp is set at the first mod found or '\0'.
 *
 *		On error (other than memory failure) it is added to the list and *out is:
 *			If section is a num: the value is 0.
 *			If section is dice: each invalid number is set to 1.
 *			If section is operation (parenthesis): ... it is set by exp_to_op_rec and this function.
 */
bool parse_num_section(struct NumSection *out_section, char **dice_exp,
		struct Dierror_list *error_list)
{

	char *section_start;
	char *ch_pointer;
	bool invalid_char;
	bool memory_failed;

	// If starting with parenthesis, recursively call exp_to_op_rec to proccess it,
	// then check for memory parenthesis errors.
	if(equals_any(**dice_exp, LEGAL_PARENTHESIS_OPENING)) {

		char closing_parenthesis;
		if(**dice_exp == '(')
			closing_parenthesis = ')';
		else
			closing_parenthesis = **dice_exp+2;	// See ascii table...

		out_section->type = type_op;
		if(!(out_section->data.operation = make_operation(true)))
			return PNS__MEM_FAIL;


		// Keep parenthesis, then proccess after it.
		ch_pointer = (*dice_exp)++;
		memory_failed = exp_to_op_rec(out_section->data.operation, dice_exp, true, HIGHEST_PRECEDENCE,
				BELOW_MINIMAL_PRECEDENCE, NULL, *dice_exp, closing_parenthesis, error_list);

		if(memory_failed) {
			clear_operation_pointer(out_section->data.operation);
			return PNS__MEM_FAIL;
		}

		// Move *dice_exp after ')'.
		if(**dice_exp == closing_parenthesis)
			(*dice_exp)++;
		else
			lassert(**dice_exp == '\0', ASSERT_LVL_FAST);

		return PNS__NO_MEM_FAIL;
	}

	// Set section_start to start, and *dice_exp to either the end or 'd'.
	section_start = *dice_exp;
	*dice_exp = get_next_in_chars(*dice_exp, LEGAL_MODS "d");

	if(**dice_exp != 'd') {
		// It's a number.
		out_section->type = type_num;

		if(*dice_exp == section_start) {	// If missing number.
			out_section->data.num = 0.0;
			return (add_dierror(error_list, missing_num, section_start, section_start+1))
				? PNS__MEM_FAIL : PNS__NO_MEM_FAIL;
		}

		out_section->data.num = strtod_noprefix(section_start, &ch_pointer);

		if(ch_pointer != *dice_exp) {	// Check for invalid char.
			if(add_dierror(error_list, invalid_num, section_start, *dice_exp))
				return PNS__MEM_FAIL;
			out_section->data.num = 0.0;
		}

		return PNS__NO_MEM_FAIL;

	}

	// It's a die.
	// Set ch_pointer to 'd', and move *dice_exp to the end.
	ch_pointer = *dice_exp;
	*dice_exp = get_next_in_chars(*dice_exp, LEGAL_MODS);
	
	out_section->type = type_die;

	// Get the number of repetitions.
	if(section_start == ch_pointer) {
		out_section->data.die.repetitions = 1;
	} else {
		out_section->data.die.repetitions = str_section_to_unsigned(section_start, ch_pointer, &invalid_char);

		// Add error if one occured, and check memory failure.
		if((invalid_char || out_section->data.die.repetitions == 0)) {
			if(add_dierror(error_list, (invalid_char) ? invalid_reps : zero_reps, section_start, ch_pointer))
				return PNS__MEM_FAIL;

			out_section->data.die.repetitions = 1;
		}
	}

	// Sides now.
	// Check if number is missing...
	if(*dice_exp == &ch_pointer[1]) {
		out_section->data.die.sides = 1;
		return add_dierror(error_list, non_existant_sides, section_start, *dice_exp)
			? PNS__MEM_FAIL : PNS__NO_MEM_FAIL;
	}

	// Get number of sides, then check errors.
	out_section->data.die.sides = str_section_to_unsigned(&ch_pointer[1], *dice_exp, &invalid_char);

	if((invalid_char || out_section->data.die.sides == 0)) {
		if(add_dierror(error_list, (invalid_char) ? invalid_sides : zero_sides, &ch_pointer[1], *dice_exp))
			return PNS__MEM_FAIL;
		out_section->data.die.sides = 1;
	}

	return PNS__NO_MEM_FAIL;
}

/* Parse section of operand/s (multiple operands are illegal unless minuses)
 * after running parse_num_section.
 *
 * If no operator is found, *out_operator is set to '*' because of the expectation
 * that we're dealing with parenthesis (either before or after).
 *
 * If multiple operators are found, unless they're all minus, they're reported.
 *
 * To clarify, there are three legal states (in which no error is reported):
 * 	No operators (replaced with '*').
 * 	Single operator.
 * 	Multiple minus operators.
 *
 * Simplest way to use this function is to meet it's requirements.
 *
 * pre:
 *	**dice_exp != '\0'
 *	**dice_exp is not in LEGAL_PARENTHESIS_CLOSING.
 * 	out_operand != NULL
 * 	Function is called after successful call of parse_num_section (for guarantees relating to dice_exp).
 * 	after_parenthesis_section is indicates whether or not the last NumSection parsed
 * 		(the one before this operator) was parenthesis.
 *
 * post:
 * 	If memory alloction failed: true is returned.
 * 	Otherwise:
 * 	*dice_exp is after the operators.
 *	*out_operand is set to the first legal operator.
 * 	Unless parenthesis, multiple operators are errenous, and are added to the error list.
 * 	*/
bool parse_operators(char *const out_operator, char **dice_exp, bool after_parenthesis_section,
		struct Dierror_list *error_list)
{
	char *operator_section_start;
	char *operator_section_ptr;

	// Legal cases:					E.g.
	// single operator, <section-end>		"+<section-end>"
	// Multiple minus, <section-end>		"----<section-end>"
	// <section-end>				"<section-end>"
	//
	// <section-end> is the part after the operator section, and is not a concern here (even if it's '\0').
	// Multiple operators are not allowed.
	//
	// <section-end> alone is legal here, since illegal cases would be handled parse_num_section
	// (missing_num error).

	// Skip the operands and keep the start.
	operator_section_start = *dice_exp;
	*dice_exp = get_next_non_pchars(*dice_exp, LEGAL_OPERANDS);

	if(*dice_exp == operator_section_start) {	// Zero operators
		// It must be parenthesis start or be after a parenthesis section to be legal.
		lassert(after_parenthesis_section || equals_any(**dice_exp, LEGAL_PARENTHESIS),
				ASSERT_LVL_PRETTY_FAST);
		*out_operator = '*';	// (Replace parenthesis with '*').
	} else if(*dice_exp != operator_section_start + 1) {	// Multiple operators.

		// Check if only '-'.
		operator_section_ptr = get_next_non_pchars(operator_section_start, "-");
		if(operator_section_ptr != *dice_exp) {
			// Error, not all are '-'.
			*out_operator = *operator_section_start;
			return add_dierror(error_list, invalid_operator, operator_section_start, *dice_exp);
		}

		*out_operator = '+' + (((operator_section_ptr - operator_section_start) % 2) * 2);
		// Equivalent to:
		// 	*out_operator = ((operator_section_ptr - operator_section_start) % 2) == 0 ? '+' : '-';
		// only avoid branch prediction. (See ascii table).

	} else {	// Single operator.
		*out_operator = *operator_section_start;
	}

	return false;

}

bool parse_prefix(char *const out_prefix, char **dice_exp, Dierror_list *error_list)
{
	char* prefix_start;
	prefix_start = *dice_exp;	// Keep the start.

	if(**dice_exp == '-') {
		do {
			++*dice_exp;
		} while(**dice_exp == '-');

		if(equals_any(**dice_exp, LEGAL_OPERANDS)) {	// Operand found after minus(es).
			*out_prefix = '+';
			*dice_exp = get_next_non_pchars(++*dice_exp, LEGAL_OPERANDS);
			return add_dierror(error_list, invalid_operator, prefix_start, *dice_exp);
		}

		*out_prefix = '+' + (((*dice_exp - prefix_start) % 2) * 2);
		// Set to '+' or '-' depending on the number of occurences of '-',
		// only avoid branch prediction. (See ascii table).
		// Equivalent to:
		//	*out_prefix = ((*dice_exp - prefix_start) % 2 == 0) ? '+' : '-';
		return false;
	}

	*dice_exp = get_next_non_pchars(*dice_exp, LEGAL_OPERANDS);
	*out_prefix = '+';	// We know it's not '-', so that's the only other legal option.

	if(*dice_exp > prefix_start+1 ||
			(*dice_exp == prefix_start+1 && *prefix_start != '+'))
		return add_dierror(error_list, invalid_operator, prefix_start, *dice_exp);

	return false;

}


/* Recursively convert dice_exp into Operation.
 *
 * pre:
 * 	dice_exp != NULL
 * 	*dice_exp != NULL
 * 	*dice_exp is modifiable (the pointer not neccesarily the characters).
 * 	dierrors is initialized.
 *
 * post:
 * 	The calculation is converted to Operation*.
 * 	If errors occure they are added to dierrors, and if memory fails, true is returned and operation
 * 	will be freed.
 *
 */
bool exp_to_op_rec(struct Operation * const operation, char **dice_exp,
		const bool set_prefix, short last_op_precedence,
		const short parent_last_op_precedence, char *parent_next_operator,
		char *parenthesis_start, char expected_parenthesis,
		struct Dierror_list *error_list)
{

	struct NumSection section;
	struct Operation *sub_operation;
	char operator;
	short op_precendence;

	// If we're supposed to set the prefix, check if + or -, set it and move *dice_exp.
	if(set_prefix) {
		if(parse_prefix(&operation->prefix, dice_exp, error_list))
			return ETOP__MEM_FAIL;

		if(operation->prefix == '-')	// We only need to update the precedence if it's minus since
			last_op_precedence = PLUS_MINUS_PERCEDENCE;	// it doesn't matter otherwise.
	}

	if(parse_num_section(&section, dice_exp, error_list) == PNS__MEM_FAIL)
		return ETOP__MEM_FAIL;

	// Continue while haven't reached the end of the section we're responsible for.
	while(**dice_exp != expected_parenthesis) {

		if(**dice_exp == '\0') {
			if(add_dierror(error_list, unclosed_parenthesis, parenthesis_start, *dice_exp)) {
				clear_num_section(section);
				return ETOP__MEM_FAIL;
			}
			break;
		}

		if(equals_any(**dice_exp, LEGAL_PARENTHESIS_CLOSING)) {
			if(add_dierror(error_list, invalid_parenthesis, *dice_exp, *dice_exp+1)) {
				clear_num_section(section);
				return ETOP__MEM_FAIL;
			}
			++*dice_exp;
			continue;
		}

		if(parse_operators(&operator, dice_exp,
				(section.type == type_op && ((struct Operation*)section.data.operation)->parenthesis),
				error_list)) {
			clear_num_section(section);
			return ETOP__MEM_FAIL;
		}

		// If the precedence is higher than last operation, make it a sub-operation.
		// (eg. "2+3*5" would become 2+op(3*5))
		op_precendence = get_operator_precedence(operator);
		if(op_precendence  > last_op_precedence) {

			// The last parsed section and operators are passed to the sub-section.
			sub_operation = make_operation_with_start(false, '+', section, operator);
			if(!sub_operation) {
				clear_num_section(section);
				return ETOP__MEM_FAIL;
			}
			if(exp_to_op_rec(sub_operation, dice_exp, false, op_precendence,
					last_op_precedence, &operator,
					parenthesis_start, expected_parenthesis,
					error_list) == ETOP__MEM_FAIL) {
				clear_operation_pointer(sub_operation);
				return ETOP__MEM_FAIL;
			}
			section.type = type_op;
			section.data.operation = sub_operation;

			if(operator == '\0') {
				lassert(**dice_exp == '\0' || **dice_exp == ')', ASSERT_LVL_FAST);
				break;
			}

			op_precendence = get_operator_precedence(operator);
		}

		// If the precedence is lower than parent's last, we should continue from parent.
		// (eg. "2*3^4+5" should become 2*op(3^4)+5 rather than 2*op(3^4+5))
		if(op_precendence <= parent_last_op_precedence) {
			// Insert the section then let parent continue with the operator.
			if(NumSection_list_append(&operation->numbers, section)) {
				clear_num_section(section);
				return ETOP__MEM_FAIL;
			}

			lassert(parent_next_operator != NULL, ASSERT_LVL_FAST);
			*parent_next_operator = operator;
			return ETOP__NO_MEM_FAIL;
		}

		last_op_precedence = op_precendence;	// Update precedence.
		if(char_list_append(&operation->operators, operator)	// Add number and operator.
				|| NumSection_list_append(&operation->numbers, section)) {
			clear_num_section(section);
			return ETOP__MEM_FAIL;
		}

		if(parse_num_section(&section, dice_exp, error_list))
			return ETOP__MEM_FAIL;
	}

	if(NumSection_list_append(&operation->numbers, section)) {
		clear_num_section(section);
		return ETOP__MEM_FAIL;
	}

	if(parent_next_operator != NULL)
		*parent_next_operator = '\0';

	return ETOP__NO_MEM_FAIL;
}

struct Operation* exp_to_op(char *dice_exp, struct Dierror **errors)
{
	lassert(dice_exp != NULL, ASSERT_LVL_FAST);

	struct Operation *ret;
	struct Dierror_list error_list;

	// Initialize error list.
	if(Dierror_list_init(&error_list)) {
		*errors = NULL;
		return NULL;
	}

	// Check if dice_exp is empty.
	if(*dice_exp == '\0') {
		if(add_dierror(&error_list, empty_expression, NULL, NULL) ||
				add_dierror(&error_list, end_of_list, NULL, NULL) ||
				((*errors = Dierror_list_to_array(&error_list)) == NULL))
			goto memory_failed__close_error_list;

		return NULL;
	}

	// Not empty, parse it.
	if(!(ret = make_operation(false)))
		goto memory_failed__close_error_list;
	if(exp_to_op_rec(ret, &dice_exp, true,
				HIGHEST_PRECEDENCE, BELOW_MINIMAL_PRECEDENCE,
				NULL, NULL, '\0', &error_list) == ETOP__MEM_FAIL) {
		clear_operation_pointer(ret);
		goto memory_failed__close_error_list;
	}

	lassert(*dice_exp == '\0', ASSERT_LVL_FAST);

	// If erred, free op and return the errors.
	if(Dierror_list_length(&error_list) != 0) {
		clear_operation_pointer(ret);
		if(add_dierror(&error_list, end_of_list, NULL, NULL) ||
				(*errors = Dierror_list_to_array(&error_list)) == NULL)
			goto memory_failed__close_error_list;
		return NULL;
	}

	// All good, free the error buffer and return.
	Dierror_list_close(&error_list, NULL);
	*errors = NULL;
	return ret;

memory_failed__close_error_list:
	Dierror_list_close(&error_list, NULL);
	*errors = NULL;
	return NULL;
}

