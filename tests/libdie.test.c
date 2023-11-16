/* 
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
#include "../libdie.h"
#include "../list_defs.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <string.h>

bool parse_num_section(struct NumSection *out, char **dice_exp,
		struct Dierror_list *error_list);

int comp_num_sections(struct NumSection s1, struct NumSection s2);

struct Operation* make_operation(bool parenthesis);

/* Help functions / macros */
#define EPS 0.0000001
#define COMP_DBLS(dbl1, dbl2) (					\
			((dbl1) <= (dbl2) + EPS) &&		\
			((dbl1) >= (dbl2) - EPS)		\
		) ? 0: (dbl1) < (dbl2) ? -1 : +1

#define COMP_DICE(d1, d2) (	\
			((d1).repetitions != (d2).repetitions) 	\
				? ((d1).repetitions - (d2).repetitions)	\
			: ((d1).sides != (d2).sides)	\
				? (d1.sides - d2.sides)	\
			: 0	\
		)			

int comp_chars(char ch1, char ch2)
{
	return ch1 - ch2;
}

bool comp_operations(struct Operation *op1, struct Operation *op2)
{
	int comparison;

	if(op1->parenthesis != op2->parenthesis)
		return op1->parenthesis - op2->parenthesis;
	
	comparison = char_list_comp(&op1->operators, &op2->operators, comp_chars);
	if(comparison != 0)
		return comparison;

	return NumSection_list_comp(&op1->numbers, &op2->numbers, comp_num_sections);
}

int comp_num_sections(struct NumSection s1, struct NumSection s2)
{
	if(s1.type != s2.type)
		return s1.type - s2.type;

	switch (s1.type) {
	case(type_num):
		return COMP_DBLS(s1.data.num, s2.data.num);
	case(type_die):
		return COMP_DICE(s1.data.die, s2.data.die);
	case(type_op):
		return comp_operations(s1.data.operation, s2.data.operation);

	default:
		exit(1);	// Should never happen.
	}
}

char* dierror_type_as_str(enum dierror_type type)
{
	switch(type) {
	case(invalid_num):
		return "invalid number";
	case(multiple_operators):
		return "multiple operators";
	case(invalid_reps):
		return "invalid repetitions";
	case(zero_reps):
		return "zero repetitions";
	case(invalid_sides):
		return "invalid sides";
	case(non_existant_sides):
		return "non_existant_sides";
	case(zero_sides):
		return "zero_sides";
	case(missing_num):
		return "missing number";
	case(unclosed_parenthesis):
		return "unclosed_parenthesis";
	case(invalid_parenthesis):
		return "invalid_parenthesis";
	case(empty_expression):
		return "empty expression";
	default:
		return "<unknown error type>";
	}
}

unsigned dierror_list_contains(struct Dierror_list *error_list, enum dierror_type error_type)
{
	struct Dierror_iterator iterator;
	struct Dierror checked_error;
	unsigned ret;

	ret = 0;
	iterator = get_Dierror_list_iterator(error_list);
	while(!Dierror_list_get(&iterator, error_list, &checked_error)) {
		if(checked_error.type == error_type)
			ret++;
	}

	return ret;
}

void fprint_dierror_list(FILE *fp,struct Dierror_list *error_list)
{
	struct Dierror_iterator iterator;
	struct Dierror error_type;

	iterator = get_Dierror_list_iterator(error_list);

	if(Dierror_list_get(&iterator, error_list, &error_type))
		return;

	fprintf(fp, "<%s>", dierror_type_as_str(error_type.type));

	while(!Dierror_list_get(&iterator, error_list, &error_type)) {
		fprintf(fp,", <%s>",dierror_type_as_str(error_type.type));
	}
}

unsigned dierror_array_contains(struct Dierror *errors, enum dierror_type error_type)
{
	unsigned count = 0;
	for(; errors->type != end_of_list; errors++) {
		if(errors->type == error_type)
			count++;
	}

	return count;
}

void fprint_dierror_array(FILE *fp, struct Dierror *errors)
{
	if(errors->type == end_of_list)
		return;

	fprintf(fp, "<%s>", dierror_type_as_str(errors->type));
	while((++errors)->type != end_of_list)
		fprintf(fp, ", <%s>", dierror_type_as_str(errors->type));
}

/* Number of elements in the array, not counting the last (terminating one). */
size_t dierror_array_len(struct Dierror *array)
{
	size_t len = 0;

	for(; array->type != end_of_list; ++array)
		len++;
	return len;
}

struct Operation* create_operation(bool parenthesis, char prefix, unsigned num_of_sections, ...)
{
	struct Operation *operation;
	va_list ap;

	if((operation = make_operation(parenthesis)) == NULL) {
		fputs("Memory allocation failed making operation, dying...\n", stderr);
		exit(1);
	}

	if(num_of_sections == 0)
		return operation;

	operation->prefix = prefix;

	if(num_of_sections != 0) {
		va_start(ap, num_of_sections);

		if(NumSection_list_append(&operation->numbers, va_arg(ap, struct NumSection))) {
			fputs("Memory allocation failed making operation while pushing.\n", stderr);
			exit(1);
		}

		while(--num_of_sections) {
			if(char_list_append(&operation->operators, (char) va_arg(ap, int))
					|| NumSection_list_append(&operation->numbers, va_arg(ap, struct NumSection))) {
				fputs("Memory allocation failed making operation while pushing.\n", stderr);
				exit(1);
			}
		}

		va_end(ap);
	}

	return operation;

}

struct NumSection create_op_section(struct Operation *operation)
{
	struct NumSection section;

	section.type = type_op;
	section.data.operation = operation;

	return section;
}

struct NumSection create_dice_section(unsigned reps, int sides)
{
	assert(sides >= 1);

	struct NumSection section;

	section.type = type_die;
	section.data.die.repetitions = reps;
	section.data.die.sides = sides;

	return section;
}

struct NumSection create_num_numsection(double num)
{
	struct NumSection section;

	section.type = type_num;
	section.data.num = num;
	return section;
}

void fprint_operation(FILE* fp, struct Operation *op);
void fprint_numsection(FILE* fp, struct NumSection section)
{
	switch (section.type) {
	case type_num:
		fprintf(fp, "%lf", section.data.num);
		break;
	case type_die:
		fprintf(fp, "%ud%d",
				section.data.die.repetitions, section.data.die.sides);
		break;
	case type_op:
		fprint_operation(fp, section.data.operation);
	}
}

void fprint_operation(FILE* fp, struct Operation *operation)
{
	struct char_iterator ops_ite;	// Iterators.
	struct NumSection_iterator section_ite;

	struct NumSection printed_section;	// Printed values.
	char printed_operator;

	bool nums_done;				// Flags.
	bool operators_done;

	nums_done = operators_done = false;

	ops_ite = get_char_list_iterator(&operation->operators);
	section_ite = get_NumSection_list_iterator(&operation->numbers);

	fputc('[', fp);


	if(operation->parenthesis) {
		fputc(' ', stderr);
		fputc('(', stderr);

	}
	if(operation->prefix != '\0') {
		fputc(operation->prefix, fp);
		fputc(' ', fp);
	}

	if(!(nums_done = NumSection_list_get(&section_ite, &operation->numbers, &printed_section)))
				fprint_numsection(fp, printed_section);

	do {
		if(!operators_done && !(operators_done = char_list_get(&ops_ite, &operation->operators, &printed_operator))) {
			fputc(' ', fp);
			fputc(printed_operator, fp);
		}
		if(!nums_done && !(nums_done = NumSection_list_get(&section_ite, &operation->numbers, &printed_section))) {
			fputc(' ', fp);
			fprint_numsection(fp, printed_section);
		}

	} while(!operators_done || !nums_done);

	if(operation->parenthesis) {
		fputc(')', stderr);
		fputc(' ', stderr);
	}

	fputc(']', fp);
}

void fprint_identifier(FILE* fp, char* identifier)
{
	fprintf(fp, "(%s) ", identifier);
}

/* input		string given to parse_num_section.
 * expected_output	The expected result.
 * expected_pointer	The pointer input should equal after passing parse_num_section.
 * error_amount		The amount of errors expected.
 * ...			Each error (as enum dierror_type).
 */
int test_parse_num_section(char *input,
		struct NumSection expected_output, char *expected_pointer,
		unsigned error_amount, ...)
{
	struct NumSection output;
	struct Dierror_list error_list;
	enum dierror_type expected_error_type;
	va_list ap;
	char *input_start = input;
	int failed = 0;
	bool error_failure = false;


	if(Dierror_list_init(&error_list)) {
		fprint_identifier(stderr, input_start);
		fputs("Memory allocation failed initializing Dierror_list...\n", stderr);
		return 1;
	}

	if(parse_num_section(&output, &input, &error_list)) {
		fprint_identifier(stderr, input_start);
		fputs("Memory allocation failed running parse_num_section...\n", stderr);
		Dierror_list_close(&error_list, NULL);
		return 1;
	}

	if((failed = (comp_num_sections(output, expected_output) != 0))) {
		fprint_identifier(stderr, input_start);
		fputs("Returned num section is different from expected.\n"
				"\tExpected output:\t", stderr);
		fprint_numsection(stderr, expected_output);
		fputs("\n"
			"\tOutput:\t\t\t", stderr);
		fprint_numsection(stderr, output);
		fputc('\n', stderr);
	}

	if((input != expected_pointer)) {
		fprint_identifier(stderr, input_start);
		fprintf(stderr, "Pointer moved to unexpected index in \"%s\": expecting %ld but got %ld.\n"
				"	(Expecting \"%s\" but got \"%s\")\n",
				input_start,
				expected_pointer - input_start, input - input_start,
				expected_pointer, input);
		failed = 1;
	}

	if(Dierror_list_length(&error_list) != error_amount) {
		fprint_identifier(stderr, input_start);
		fprintf(stderr, "Expecting %d errors but got %ld.\n",
				error_amount, Dierror_list_length(&error_list));
		failed = 1;
		error_failure = true;
	}

	if(error_amount) {
		va_start(ap, error_amount);
		do {
			expected_error_type = va_arg(ap, enum dierror_type);
			// Check if expected error is found 0 times.
			if(dierror_list_contains(&error_list, expected_error_type) == 0) {	
				fprint_identifier(stderr, input_start);
				fprintf(stderr ,"Expected error <%s> not found.\n",
						dierror_type_as_str(expected_error_type));
				failed = 1;
				error_failure = true;
			}
		} while(--error_amount != 0);
		va_end(ap);
	}

	if(error_failure) {
		fprint_identifier(stderr, input_start);
		fputs("Errors found: ", stderr);
		fprint_dierror_list(stderr, &error_list);
		fputc('\n', stderr);
	}

	clear_num_section(output);
	Dierror_list_close(&error_list, NULL);
	return failed;
}

int parse_num_section_tester()
{
	int fails;
	char *dice_exp;
	struct NumSection expected_section;

	// Numbers.
	
	// Valid
	dice_exp = "14";
	fails = test_parse_num_section(dice_exp, create_num_numsection(14.0), dice_exp+2, 0);

	dice_exp = "006-";
	fails += test_parse_num_section(dice_exp, create_num_numsection(6.0), dice_exp+3, 0);

	dice_exp = "6.66*";
	fails += test_parse_num_section(dice_exp, create_num_numsection(6.66), dice_exp+4, 0);

	// Invalid
	dice_exp = "+5";
	fails += test_parse_num_section(dice_exp, create_num_numsection(0.0), dice_exp+0, 1, missing_num);

	dice_exp = "53ha!34";	// No mods
	fails += test_parse_num_section(dice_exp, create_num_numsection(0.0), dice_exp+7, 1, invalid_num);

	// Dice
	
	dice_exp = "2d4";
	fails += test_parse_num_section(dice_exp, create_dice_section(2, 4), dice_exp+3, 0);

	dice_exp = "d630";
	fails += test_parse_num_section(dice_exp, create_dice_section(1, 630), dice_exp+4, 0);

	dice_exp = "12d4+";
	fails += test_parse_num_section(dice_exp, create_dice_section(12, 4), dice_exp+4, 0);

	dice_exp = "0d10";
	fails += test_parse_num_section(dice_exp, create_dice_section(1, 10), dice_exp+4, 1, zero_reps);

	dice_exp = "5d0";
	fails += test_parse_num_section(dice_exp, create_dice_section(5, 1), dice_exp+3, 1, zero_sides);

	dice_exp = "0d0";
	fails += test_parse_num_section(dice_exp, create_dice_section(1, 1), dice_exp+3, 2, zero_reps, zero_sides);

	dice_exp = "3d";
	fails += test_parse_num_section(dice_exp, create_dice_section(3, 1), dice_exp+2, 1, non_existant_sides);

	dice_exp = "11vali d20";
	fails += test_parse_num_section(dice_exp, create_dice_section(1, 20), dice_exp+10, 1, invalid_reps);

	dice_exp = "11d20d12";
	fails += test_parse_num_section(dice_exp, create_dice_section(11, 1), dice_exp+8, 1, invalid_sides);

	dice_exp = "11d1haha+5";
	fails += test_parse_num_section(dice_exp, create_dice_section(11, 1), dice_exp+8, 1, invalid_sides);

	// Operation
	
	dice_exp = "(14*3d4-5)";
	expected_section = create_op_section(
			create_operation(true, '\0', 3,
				create_num_numsection(14.0), '*',
				create_dice_section(3, 4), '-',
				create_num_numsection(5.0)
				));
	fails += test_parse_num_section(dice_exp, expected_section, dice_exp+10, 0);
	clear_num_section(expected_section);

	dice_exp = "(d20+3^5)";
	expected_section = create_op_section(
			create_operation(true, '\0', 2,
				create_dice_section(1, 20), '+',
				create_op_section(
					create_operation(false, '\0',2,
						create_num_numsection(3.0), '^', create_num_numsection(5.0))
					)
				)
			);
	fails += test_parse_num_section(dice_exp, expected_section,dice_exp+9 , 0);
	clear_num_section(expected_section);

	return fails;
}

/*
 * dice_exp 		the input.
 * expected_output	The expected returned operation.
 * ex_error_amount 	The number of errors we're expecting.
 * ...			Each error as error_type.
 * */
bool test_exp_to_op(char *dice_exp, struct Operation *expected_output, unsigned ex_error_amount, ...)
{
	struct Operation *output;
	struct Dierror *errors;

	bool failed = false;

	va_list ap;
	enum dierror_type checked_error;

	output = exp_to_op(dice_exp, &errors);

	if(output == NULL) {
		if(!errors) {
			fprint_identifier(stderr, dice_exp);
			fputs("Memory allocation failed...\n", stderr);
			clear_operation_pointer(output);
			return 1;
		}

		// Errors occured.
		if(expected_output != NULL) {
			fprint_identifier(stderr, dice_exp);
			fputs("Expecting no errors, yet returned errors.\n", stderr);
			failed = true;
		}

		if(dierror_array_len(errors) == 0) {
			fprint_identifier(stderr, dice_exp);
			fputs("Error array is not NULL, but it has no elements.\n", stderr);
			failed = true;
		}

		if(dierror_array_len(errors) != ex_error_amount) {
			fprint_identifier(stderr, dice_exp);
			fprintf(stderr, "Expecting %ld errors but has %d\n",
					dierror_array_len(errors), ex_error_amount);
			failed = true;
		}

		if(ex_error_amount != 0) {
			va_start(ap, ex_error_amount);
			do {
				checked_error = va_arg(ap, enum dierror_type);
				if(dierror_array_contains(errors, checked_error) == 0) {
					fprint_identifier(stderr, dice_exp);
					fprintf(stderr, "Expected error <%s> not returned.\n", dierror_type_as_str(checked_error));
					failed = true;
				}
			} while(--ex_error_amount != 0);
			va_end(ap);
		}

		free(errors);

	} else{
		if(expected_output == NULL) {
			fprint_identifier(stderr, dice_exp);
			fputs("Expecting NULL return but got:\n\t", stderr);
			fprint_operation(stderr, output);
			failed = true;
		} else if(comp_operations(output, expected_output) != 0) {
			fprint_identifier(stderr, dice_exp);
			fputs("Returned operation different from expected.\n"
					"\tExpected output:\t", stderr);
			fprint_operation(stderr, expected_output);
			fputs("\n"
				"\tOutput:\t\t\t", stderr);
			fprint_operation(stderr, output);
			fputc('\n', stderr);

			failed = true;
		}

		if(errors != NULL) {
			fprint_identifier(stderr, dice_exp);
			fputs("Returned operation is not NULL, but errors not NULL.\n", stderr);
			failed = true;
			free(errors);
		} else if(ex_error_amount != 0) {
			fprint_identifier(stderr, dice_exp);
			fputs("Expecting errors, yet no errors returned.\n", stderr);
			failed = true;
		}

		clear_operation_pointer(output);
	}

	return failed;
}

int exp_to_op_tester()
{
	int fails = 0;
	char *dice_exp;
	struct Operation *operation;

	dice_exp = "2d7+5/d6+4(d10)";
	operation = create_operation(false, '\0',  3,
		create_dice_section(2,7),
		'+',
		create_op_section(create_operation(false, '\0', 2,
			create_num_numsection(5.0),
			'/',
			create_dice_section(1,6))
		), 
		'+',
		create_op_section(create_operation(false, '\0', 2,
					create_num_numsection(4.0),
					'*',
					create_op_section(create_operation(true, '\0', 1,
							create_dice_section(1, 10)
							))
				))
			);
	fails = test_exp_to_op(dice_exp, operation, 0);
	clear_operation_pointer(operation);

	dice_exp = "33.5*4d12+0";
	operation = create_operation(false, '\0', 3,
			create_num_numsection(33.5),
			'*', create_dice_section(4, 12),
			'+', create_num_numsection(0.0));
	fails += test_exp_to_op(dice_exp, operation, 0);
	clear_operation_pointer(operation);

	dice_exp = "666.666(d12/2.5)^4";
	operation = create_operation(false, '\0', 2,
			create_num_numsection(666.666),
			'*', create_op_section(create_operation(false, '\0', 2,
					create_op_section(create_operation(true, '\0', 2,
							create_dice_section(1, 12),
							'/', create_num_numsection(2.5))
						),
					'^', create_num_numsection(4.0))
				)
			);
	fails += test_exp_to_op(dice_exp, operation, 0);
	clear_operation_pointer(operation);

	dice_exp = "5+6ha+d3";
	fails += test_exp_to_op(dice_exp, NULL, 1, invalid_num);


	return fails;
}

size_t int_req_digits(int num);

bool test_int_req_digits(int num)
{
	size_t res;
	size_t ex_res;
	if((res = int_req_digits(num)) != (ex_res = (num < 9.999) ? 1 : (((size_t) log10((double) num)) + 1))) {
		fprintf(stderr, "Expected digits required for %d: %lu, but got %lu.\n",
				num, ex_res, res);
		return true;
	}
	return false;
}
int int_req_digits_tester()
{

	srand(time(NULL));

	return test_int_req_digits(0) + test_int_req_digits(6) +
		test_int_req_digits(15) + test_int_req_digits(10) +
		test_int_req_digits(255)
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand()) 
		+ test_int_req_digits(rand()) + test_int_req_digits(rand());
}

bool test_get_calc_string_length(struct Operation *operation, char *id, size_t expected_length)
{
	size_t returned_length;
	if((returned_length = get_calc_string_length(operation)) != expected_length) {
		fprint_identifier(stderr, id);
		fprintf(stderr, "Returned length %lu is different from expected: %lu.\n",
				returned_length, expected_length);
		return true;
	}

	return false;
}

int get_calc_string_length_tester()
{
	struct Operation *operation;
	int fails;

	operation = create_operation(false, '\0', 3,
			create_num_numsection(33.5),
			'*',
			create_dice_section(4, 12),
			'+',
			create_num_numsection(0.0));
	fails = test_get_calc_string_length(operation, "33.5*4d12+0",
			2	// operators
			+ 2+1+4	// 33.5
			+ 4*2+3	// 4d12
			+ 2	// parenthesis
			+ 1+1+4	// 0
			+ 1);	// '\0'
	clear_operation_pointer(operation);

	operation = create_operation(false, '\0', 1, create_dice_section(3, 15));
	fails += test_get_calc_string_length(operation, "3d15",
			3*2+2
			+1);
	clear_operation_pointer(operation);

	operation = create_operation(false, '\0', 2,
			create_num_numsection(22.11),
			 '+',
			create_num_numsection(122.0));
	fails += test_get_calc_string_length(operation, "22.11+122",
			1	// operator
			+2+1+4	// 22.11
			+ 3+1+4	// 122
			+1);	// '\0'
	clear_operation_pointer(operation);

	
	operation = create_operation(false, '\0', 3,
			create_num_numsection(22.11),
			'+', create_num_numsection(122.0),
			'+', create_dice_section(3,5));

	fails += test_get_calc_string_length(operation, "22.11+122+3d5",
			2	// operator
			+2+1+4	// 22.11
			+ 3+1+4	// 122
			+ 3*1+2
			+1);	// '\0'
	clear_operation_pointer(operation);

	// 2d8+4*666-1d13
	operation = create_operation(false, '-', 3, 
			create_dice_section(2, 8),
			'+', create_op_section(create_operation(false, '\0', 2, 
					create_num_numsection(4.0),
					'*', create_num_numsection(666.0))),
			'-', create_dice_section(1, 13));
	fails += test_get_calc_string_length(operation,	"-2d8+4*666-1d13",
			1	// Prefix
			+ 3	// Operators	3
			+ 2+1	// 2d8		6
			+ 1+5	// 4		12
			+ 3+5	// 666 		20
			+ 2	// 1d13		22
			+ 1);	// '\0'		23
	clear_operation_pointer(operation);

	return fails;
}

double operate(const struct Operation *operation, char *calc_string, short flags);

char *daprintf(char* format, ...)
{
	char *buffer;
	size_t req_size;
	va_list ap;

	va_start(ap, format);
	req_size = vsnprintf(NULL, 0, format, ap) + 1;
	va_end(ap);

	buffer = malloc(req_size * sizeof(*buffer));
	if(!buffer)
		exit(1);

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	return buffer;
}

void dice_roller(int *roll_buf, ...)
{
	va_list ap;
	int roll;

	va_start(ap, roll_buf);
	while((roll = va_arg(ap, int)) > 0) {
		*roll_buf++ = ((rand() % roll) + 1);
	}
	va_end(ap);
}

bool test_operate(struct Operation const *input, double ex_result, 
		short flags, char *expected_calc_string_fmt, ...) {
	char *calc_string_buf;
	double result;
	size_t expected_max_len;
	bool failed;

	va_list ap;
	char *expected_calc_string;

	if(expected_calc_string_fmt) {

		// Get expected calc string length.
		va_start(ap, expected_calc_string_fmt);
		const size_t expected_calc_string_len = vsnprintf(NULL, 0, expected_calc_string_fmt, ap);
		va_end(ap);
		// Allocate the buffer.
		expected_calc_string = alloca((expected_calc_string_len+1) * sizeof(*expected_calc_string));

		// Get the expected calc string.
		va_start(ap, expected_calc_string_fmt);
		vsprintf(expected_calc_string, expected_calc_string_fmt, ap);
		va_end(ap);

		expected_max_len = get_calc_string_length(input);
		calc_string_buf = alloca(2 * expected_max_len * sizeof(*calc_string_buf));


		result = operate(input, calc_string_buf, flags);

	} else {
		result = operate(input, NULL, flags);
	}

	if((failed = (COMP_DBLS(result, ex_result) != 0))) {
		fprintf(stderr, "Result is different from expected.\n\t"
				"Expecting %lf but got %lf.\n", ex_result, result);
	}

	if(expected_calc_string_fmt) {
		if(strcmp(calc_string_buf, expected_calc_string) != 0) {
			fprintf(stderr, "Calculation string is different form expected.\n\t"
					"Expecting \"%s\" but returned \"%s\".\n",
					expected_calc_string, calc_string_buf);
			failed = true;
		}
		if(strlen(calc_string_buf) >= expected_max_len) {
			fprintf(stderr, "Calculation string exceeded the expected maximum buffer.\n"
					"\tExpected maximum %lu but string length is %lu.\n",
					expected_max_len, strlen(calc_string_buf) + 1);
			failed = true;
		}
	}

	return failed;
}

int operate_tester()
{
	struct Operation *operation;
	int fails;
	double ex_result;
	
	time_t seed = time(NULL);
	int rolls[40];

	// 33.5*4d12+0
	operation = create_operation(false, '\0', 3,
			create_num_numsection(33.5),
			'*', create_dice_section(4, 12),
			'+', create_num_numsection(0.0));
	srand(seed);
	dice_roller(rolls, 12, 12, 12, 12, -1);
	ex_result = 33.5 * (rolls[0] + rolls[1] + rolls[2] + rolls[3]) + 0.0;

	srand(seed);
	fails = test_operate(operation, ex_result, NO_FLAG,
			"33.5*(%d+%d+%d+%d)+0", rolls[0], rolls[1], rolls[2], rolls[3]);
	srand(seed);
	fails += test_operate(operation, ex_result, COLLAPSE_DICE,
			"33.5*%d+0", rolls[0] + rolls[1] + rolls[2] + rolls[3]);
	clear_operation_pointer(operation);

	// "2d7+5/d6+4(d10)"
	operation = create_operation(false, '\0',  3,
		create_dice_section(2,7),
		'+', create_op_section(create_operation(false, '\0', 2,
					create_num_numsection(5.0),
					'/',
					create_dice_section(1,6))
		), 
		'+', create_op_section(create_operation(false, '\0', 2,
					create_num_numsection(4.0),
					'*',
					create_op_section(create_operation(true, '\0', 1,
							create_dice_section(1, 10)
							))
				))
			);
	srand(seed);
	dice_roller(rolls, 7,7,6,10, -1);
	ex_result = rolls[0] + rolls[1] + 5.0 / rolls[2] + 4 * rolls[3];

	srand(seed);
	fails += test_operate(operation, ex_result, NO_FLAG, "%d+%d+5/%d+4*(%d)",
			rolls[0], rolls[1], rolls[2], rolls[3]);
	srand(seed);
	fails += test_operate(operation, ex_result, COLLAPSE_DICE, "%d+%d+5/%d+4*(%d)",
			rolls[0], rolls[1], rolls[2], rolls[3]);

	clear_operation_pointer(operation);
	return fails;
}


