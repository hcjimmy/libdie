#include "../libdie.h"

#include <time.h>

int main()
{
	srand(time(NULL));

	char *dice_exp = "d100-2^2d4";
	struct Operation *operation;
	struct Dierror *dierrors;

	// Get operation from dice_exp.
	operation = exp_to_op(dice_exp, &dierrors);
	if(!operation) {
		// Error!
		
		if(!dierrors) {
			// Memory error!
			fputs("OOM...\n", stderr);
			exit(1);
		}

		// Invalid expression...
		// We're not gonna print it here.
		fprintf(stderr, "Invalid expression...\n");
		free(dierrors);	// Technically not neccessary since we're exiting now...
		exit(2);
	}

	// Valid expression.

	// Optionally create buffer to contain the calculation (not the result)...
	char *calc_string_buffer = alloca(get_calc_string_length(operation) * sizeof(*calc_string_buffer));

	// Rollllllll
	double result = operate(operation, calc_string_buffer, NO_FLAG);
	printf("%s = %lf\n", calc_string_buffer, result);

	clear_operation_pointer(operation);

	return 0;
}
