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
#include "libdie.h"

named_list_def_funcs(struct Dierror, Dierror)
named_list_def_funcs(struct NumSection, NumSection)

void clear_num_section(struct NumSection section)
{
	if((section).type == type_op)
		clear_operation_pointer((section).data.operation);
}

/* Free operation. */
void clear_operation_pointer(struct Operation *operation)
{
	NumSection_list_close(&operation->numbers, clear_num_section);
	char_list_close(&operation->operators, NULL);
	free(operation);
}

