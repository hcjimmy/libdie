/* Convert operation to a number, and related functions.
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

#include <limits.h>
#include <math.h>

/* Prototypes */

// For calculating an operation:

// Recursively converts the operation.
double operate_rec(const struct Operation *operation, char **calc_string, short flags);
// Do a calculation on 2 values.
double binary_calc(double val1, char operand, double val2);
double calc_section(struct NumSection section, char **calc_string, short flags);
double roll_dice(struct Die die, char **calc_string, short flags);
// See collapse flag in header.
int roll_nocollapse(struct Die die, char **calc_string);
// Self explanatory.
int just_roll(struct Die die);

// To calculate the maximum buffer length needed by operate:

// Recursively count needed length.
size_t get_calc_string_length_rec(const struct Operation *operation);
// Count a section.
size_t get_section_calc_string_length(struct NumSection section);
// Convert int to number of chars required for it as a decimal string.
size_t int_req_digits(int num);


// The number of maximum digits displayed after the dot.
#define NUM_PRECISION 4

// Internal flag for operate functions.
#define HIGHER_OPERAND (1<<1)

// To reduce code duplication:
#define ROLL_D(sides) (rand() % (sides) + 1)


/* -- Functions used for the calculation -- */

int just_roll(struct Die die)
{
	unsigned reps;
	int ret;

	ret = 0;
	reps = die.repetitions;

	while(reps-- > 0)
		ret += ROLL_D(die.sides);

	return ret;
}

// (See COLLAPSE_DICE flag in header)
int roll_nocollapse(struct Die die, char **calc_string)
{
	int roll;
	int ret;
	unsigned reps;

	roll = ROLL_D(die.sides);
	sprintf_move(calc_string, "%d", roll);
	ret = roll;

	reps = die.repetitions;
	while(reps-- > 1) {
		roll = ROLL_D(die.sides);
		sprintf_move(calc_string, "+%d", roll);
		ret += roll;
	}

	return ret;
}

double roll_dice(struct Die die, char **calc_string, short flags)
{
	lassert(die.repetitions != 0, ASSERT_LVL_FAST);

	int rolls;

	if(calc_string == NULL)
		return (double) just_roll(die);

	if(die.repetitions == 1 || (flags & HIGHER_OPERAND && flags & COLLAPSE_DICE)) {
		rolls = just_roll(die);
		sprintf_move(calc_string, "%d", rolls);
		return (double) rolls;
	}

	if(flags & HIGHER_OPERAND)
		*((*calc_string)++) = '(';
	rolls = roll_nocollapse(die, calc_string);
	if(flags & HIGHER_OPERAND)
		*((*calc_string)++) = ')';

	return (double) rolls;
}


double calc_section(const struct NumSection section, char **calc_string, short flags)
{
	switch (section.type) {
	case(type_num):
		if(calc_string != NULL)
			*calc_string = stringify_double(section.data.num, NUM_PRECISION, *calc_string);
		return section.data.num;
	case(type_die):
		return roll_dice(section.data.die, calc_string, flags);
	case(type_op):
		return operate_rec(section.data.operation, calc_string, flags);
		
	default:
		exit(1);	// Should never happen.
	}
}

/* Receives 2 values and a legal operand,
 * and return the result of the operation `val1 operand val2`. */
double binary_calc(double val1, char operand, double val2)
{
	switch (operand) {
	case('+'):
		return val1 + val2;
	case('-'):
		return val1 - val2;
	case('*'):
		return val1 * val2;
	case('/'):
		return val1 / val2;
	case('%'):
		return fmod(val1, val2);
	case('^'):
		return pow(val1, val2);
		
	default:
		exit(1);	// Should never happen.
	}
}

double operate_rec(const struct Operation *operation, char **calc_string, short flags)
{
	
	char_iterator operand_ite = get_char_list_iterator(&operation->operators);
	NumSection_iterator sec_ite = get_NumSection_list_iterator(&operation->numbers);

	char operand;
	char next_operand;
	struct NumSection section;
	double ret;
	double next_value;

	if(calc_string) {
		if(operation->parenthesis)
			*((*calc_string)++) = '(';
		if(operation->prefix == '-')
			*((*calc_string)++) = '-';
	}

	NumSection_list_get(&sec_ite, &operation->numbers, &section);

	// Get first operator. If it doesn't exist, we have 1 section.
	if(char_list_get(&operand_ite, &operation->operators, &operand)) {
		ret = calc_section(section, calc_string, flags);
		if(calc_string && operation->parenthesis)
			*((*calc_string)++) = ')';
		return (operation->prefix == '-') ? -ret : ret;
	}

	// If the section is a die, then we care if the precedence is higher than +-.
	// Pass internal flag to indicate it.
	if(section.type == type_die && ((operand != '+' && operand != '-')))
		ret = calc_section(section, calc_string, flags | HIGHER_OPERAND);
	else
		ret = calc_section(section, calc_string, flags);
	if(operation->prefix == '-')
		ret = -ret;

	// Continue until there is no operand after the last.
	// (We're parsing operand not next_operand now, but we need to know next operand
	// to parse the section, since if it's dice an operand higher than +- after will change
	// the results).
	while(!char_list_get(&operand_ite, &operation->operators, &next_operand)) {

		if(calc_string)
			*((*calc_string)++) = operand;	// add the operand.

		// Get next section and calculate it.
		NumSection_list_get(&sec_ite, &operation->numbers, &section);

		if(section.type == type_die &&
				((operand != '+' && operand != '-') || (next_operand != '+' && next_operand != '-')))
			next_value = calc_section(section, calc_string, flags | HIGHER_OPERAND);
		else
			next_value = calc_section(section, calc_string, flags);

		ret = binary_calc(ret, operand, next_value);
		operand = next_operand;
	}

	if(calc_string)
		*((*calc_string)++) = operand;

	NumSection_list_get(&sec_ite, &operation->numbers, &section);
	if(section.type == type_die && ((operand != '+' && operand != '-')))
		next_value = calc_section(section, calc_string, flags | HIGHER_OPERAND);
	else
		next_value = calc_section(section, calc_string, flags);

	ret = binary_calc(ret, operand, next_value);

	if(calc_string && operation->parenthesis)
		*((*calc_string)++) = ')';

	return ret;
}

/* operate - calculate operation with the dice rolled.
 *
 * calc_string_buf - is a buffer to contain the calculation, the length
 * should be received from get_operation_calc_string_len. */
double operate(const struct Operation *operation, char *calc_string, short flags)
{
	double ret;

	if(calc_string) {
		ret = operate_rec(operation, &calc_string, flags);
		*calc_string = '\0';
	} else
		ret = operate_rec(operation, NULL, flags);

	return ret;
}


/* -- Functions used to get the buffer length -- */

/* Return the number of digits required to representing num as a string.
 *
 * Prerequisite: num >= 0
 *
 * Instead of using casting to log10, let's optimize this if int is 32 bit...
 *
 * (Honestly I saw this kind of optimization on stack overflow sometime back... I didn't copy paste technically...
 * don't really have the link either...)
 */
#if INT_MAX == 2147483647	// (2^31-1)
size_t int_req_digits(int num)
{
	lassert(num >= 0, ASSERT_LVL_FAST);

	if(num < 10000) {	// 4 digits or less
		if(num < 100)
			return (num < 10) ? 1 : 2;

		// 3 or 4 digits
		return (num < 1000) ? 3 : 4;
	}

	// 5 to 10
	if(num < 10000000) {	// 5 to 7
		if(num < 1000000) // 5 or 6
			return (num < 100000) ? 5 : 6;
		return 7;
	}
	// 8 to 10
	if(num < 1000000000) // 8 or 9
		return (num < 100000000) ? 8 : 9;

	return 10;
}
#else	// It's not 32bit, just use a log10...
size_t int_req_digits(int num)
{
	lassert(num >= 0, ASSERT_LVL_FAST);

	if(num == 0)
		return 1;
	return ((size_t) log10((double) num)) + 1;
}
#endif

/* Get the maximum required length for section.
 * Note: assumes dice is not collapsed (see operate flag in header),
 * 	and does not account for dice parenthesis in case of higher operand (caller needs to handle that).
 */
size_t get_section_calc_string_length(const struct NumSection section)
{
	switch (section.type) {
	case(type_num):
		return snprintf(NULL, 0, "%." STRINGER(NUM_PRECISION) "lf", section.data.num);
	case(type_die):
		return int_req_digits(section.data.die.sides) * section.data.die.repetitions	// Each dice if rolled highest.
			+ section.data.die.repetitions -1;	// The operators.
	case(type_op):
		return get_calc_string_length_rec(section.data.operation);

	default:
		exit(1);	// Should never happen.
	}
}

size_t get_calc_string_length_rec(const struct Operation *operation)
{
	size_t length;

	NumSection_iterator section_ite;
	char_iterator operator_ite;

	char operator, next_operator;
	struct NumSection section;

	// Count the operators.
	length = char_list_length(&operation->operators);

	// Check prefix and parenthesis.
	if(operation->prefix == '-')
		++length;
	if(operation->parenthesis)
		length += 2;

	// Prepare to got over the lists.
	section_ite = get_NumSection_list_iterator(&operation->numbers);
	operator_ite = get_char_list_iterator(&operation->operators);

	NumSection_list_get(&section_ite, &operation->numbers, &section);	// Get first section.

	if(char_list_get(&operator_ite, &operation->operators, &operator))	// Get first operator.
		return length + get_section_calc_string_length(section);	// We're done if there's none.

	// If there an operator of precedence higher than +- near dice that repeats more than once,
	// we need to account for parenthesis.
	if(section.type == type_die && section.data.die.repetitions != 1 && (operator != '+' && operator != '-'))
		length += 2;
	length += get_section_calc_string_length(section);	// Count the section.
	
	NumSection_list_get(&section_ite, &operation->numbers, &section);

	// While there's an operand after section.
	while(!char_list_get(&operator_ite, &operation->operators, &next_operator)) {

		// Count each section and account for dice parenthesis.
		if(section.type == type_die && section.data.die.repetitions != 1 &&
				((operator != '+' && operator != '-') || (next_operator != '+' && next_operator != '-')))
			length += 2;
		length += get_section_calc_string_length(section);

		NumSection_list_get(&section_ite, &operation->numbers, &section);
		operator = next_operator;
	}
	
	if(section.type == type_die && section.data.die.repetitions != 1 && (operator != '+' && operator != '-'))
		length += 2;

	return length + get_section_calc_string_length(section);
}

size_t get_calc_string_length(const struct Operation *operation)
{
	return get_calc_string_length_rec(operation)
		+ 1;	// (To account for '\0')
}

/* -- Other -- */

bool is_single_num_operation(struct Operation *operation)
{
	struct NumSection section;

	if(char_list_length(&operation->operators) != 0)
		return false;

	section = NumSection_list_get_index(&operation->numbers, 0);
	if(section.type == type_op || (section.type == type_die && section.data.die.repetitions != 1))
		return false;

	return true;
}

