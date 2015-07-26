/*
    voltlogger_analyzer
    
    Copyright (C) 2015 Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include "configuration.h"
#include "macros.h"

#include <stdio.h>	/* getline()	*/
#include <stdlib.h>	/* atof()	*/
#include <unistd.h>	/* getopt()	*/
#include <math.h>

#include "error.h"
#include "analyzer_root.h"

enum functype {
	FT_SIN
};
typedef enum functype functype_t;

static char *const functypes[] = {
	[FT_SIN] = "sin",
	NULL
};



int main(int argc, char *argv[]) {
	functype_t functype = FT_SIN;
	float frequency = 50;

	// Initializing output subsystem
	{
		int output_method     = OM_STDERR;
		int output_quiet      = 0;
		int output_verbosity  = 9;
		int output_debuglevel = 9;

		error_init(&output_method, &output_quiet, &output_verbosity, &output_debuglevel);
	}

	// Parsing arguments
	char c;
	while ((c = getopt (argc, argv, "f:F:")) != -1) {
		char *arg;
		arg = optarg;

		switch (c)
		{
			case 'f': {
				char *value;
				functype = getsubopt(&arg, functypes, &value);
				break;
			}
			case 'F':
				frequency = atof(optarg);
				break;
			default:
				abort ();
		}
	}

	switch (functype) {
		case FT_SIN:
			root_analyze_sin(stdin, stdout, frequency);
			break;
		default:
			fprintf(stderr, "Unknown approximation function\n");
			abort ();
	}

	return 0;
}

