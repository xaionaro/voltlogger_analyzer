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


#include "configuration.h"
#include "macros.h"

#include <stdio.h>	/* FILE		*/
#include <stdlib.h>	/* size_t	*/
#include <stdint.h>	/* uint64_t	*/
#include <string.h>	/* memset()	*/
#include <math.h>	/* fabs()	*/
#include <assert.h>	/* assert()	*/
//#include <errno.h>	/* errno	*/

#include "error.h"
#include "malloc.h"
#include "analyzer.h"
#include "binary.h"

#define MAX_HISTORY (1 << 24)

struct history_item {
	uint64_t		 unixTSNano;
	uint64_t		 sensorTS;
	uint32_t		 value;
	void			*procdata;
};
typedef struct history_item history_item_t;

typedef int (*vl_checkfunct_t)(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointfile, int concurrency, void *arg, double error_threshold, char realtime);

#define SIN_NUM_STAGES 16

int vl_realcheck_sin(history_item_t *value_history, uint64_t value_history_filled, char *checkpointfile, int concurrency, float frequency_p, double error_threshold, char realtime) {
	history_item_t *stages[SIN_NUM_STAGES], *p, *e;

	p =  value_history;
	e = &value_history[value_history_filled-1];

	uint64_t sensorTS_start     = p->sensorTS;
	uint64_t sensorTSDiff_total = e->sensorTS - sensorTS_start;

	int stage_old = -1;

	while (p <= e) {
		uint64_t sensorTSDiff_cur = p->sensorTS - sensorTS_start;

		int stage = sensorTSDiff_cur*SIN_NUM_STAGES / sensorTSDiff_total;

		if (stage >= SIN_NUM_STAGES)
			stage = SIN_NUM_STAGES-1;

		if (stage != stage_old) {
			if (stage != stage_old + 1) {
				return 0;
			}
			printf("%i %u\n", stage, p->value);
			stages[stage] = p;
			stage_old = stage;
		}

		p++;
	}

	(void)stages;

	return 0;
}

int vl_check_sin(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointfile, int concurrency, void *_frequency_p, double error_threshold, char realtime) {
	float frequency = *(float *)_frequency_p;

	history_item_t *cur = &value_history[*value_history_filled_p - 1];

	uint64_t expectedEndOffset_unixTSNano = (uint64_t)(1E9 /* nanosecs in sec */ / frequency);
	uint64_t     currentOffset_unixTSNano = cur->unixTSNano - value_history[0].unixTSNano;

	int rc = 0;
	if ( currentOffset_unixTSNano  >=  expectedEndOffset_unixTSNano ) {

		rc = vl_realcheck_sin(value_history, *value_history_filled_p, checkpointfile, concurrency, frequency, error_threshold, realtime);
		*value_history_filled_p = 0;
	}

	return rc;
}

static inline void vl_analize(FILE *i_f, FILE *o_f, char *checkpointfile, int concurrency, void *arg, vl_checkfunct_t checkfunct, double error_threshold, char realtime) {
	history_item_t *value_history, *history_item_ptr;
	uint64_t        value_history_filled = 0;

	value_history = xcalloc(MAX_HISTORY, sizeof(*value_history));

	while (!feof(i_f)) {
		assert (value_history_filled < MAX_HISTORY);	// overflow

		history_item_ptr = &value_history[ value_history_filled++ ];

		history_item_ptr->unixTSNano = get_uint64(i_f, realtime);
		history_item_ptr->sensorTS   = get_uint64(i_f, realtime);
		history_item_ptr->value      = get_uint32(i_f, realtime);

		if (checkfunct(value_history, &value_history_filled, checkpointfile, concurrency, arg, error_threshold, realtime)) {
			printf("Problem\n");
		}


	}

	free(value_history);

	return;
}

void vl_analyze_sin(FILE *i_f, FILE *o_f, char *checkpointfile, int concurrency, float frequency, double error_threshold, char realtime)
{
	float *frequency_p = xmalloc(sizeof(float));
	*frequency_p = frequency;

	vl_analize(i_f, o_f, checkpointfile, concurrency, frequency_p, vl_check_sin, error_threshold, realtime);

	free(frequency_p);
	return;
}


