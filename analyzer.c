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


typedef int (*vl_checkfunct_t)(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointfile, int concurrency, void *arg, double error_threshold, char realtime);

#define SIN_PHASE_PARTS 3

int vl_realcheck_sin(history_item_t *value_history, uint64_t value_history_filled, char *checkpointfile, int concurrency, float frequency_p, double error_threshold, char realtime) {
	history_item_t *p, *e;

	p =  value_history;
	e = &value_history[value_history_filled-1];

	uint64_t sensorTS_start     = p->row.sensorTS;
	uint64_t sensorTSDiff_total = e->row.sensorTS - sensorTS_start;
	double y_sum = 0;

	while (p <= e) {
		y_sum += p->row.value;

		p++;
	}


	double y_avg = (double)y_sum / (double)value_history_filled;
	double y_int = 0;

	history_item_t *p_old = p = value_history;

	while (++p <= e) {
		double y_diff = p->row.value - y_avg;
		y_int += y_diff*y_diff * (p->row.sensorTS - p_old->row.sensorTS);

		p_old = p;
	}

	y_int *= 2*M_PI / sensorTSDiff_total;

	double lambda = (double)2*M_PI / sensorTSDiff_total;
	double amp    = sqrt(y_int / M_PI);

	// f(x) =  amp * sin(lambda*x + phase) + avg
	double phase;

	p_old = p = value_history;
	phase = 0;

/*	//TODO: Complete this method of phase calculation (instead of iteration method below)

	while (++p <= e) {
		if ((p_old->value - y_avg) * (p->value - y_avg) < 0) {
			double avg_value = (p->value + p_old->value) / 2;

			if (p_old->value < y_avg)
				phase = -lambda * avg_value;
			else
				phase =  lambda * avg_value;

			break;
		}

		p_old = p;
	}
*/

	double sqdeviation_min = 1E100;

	double phase_add_best = M_PI;

	double scan_interval = 2*M_PI;
	while (scan_interval > 1E-3) {
		double phase_add = -scan_interval;
		double phase_add_best_add = 0;
		//fprintf(stderr, "scanning: %le -> %le\n", phase_add_best + phase_add, phase_add_best + scan_interval);
		while (phase_add_best + phase_add < phase_add_best + scan_interval) {
			double sqdeviation = 0;
			p = value_history;
			while (p <= e) {
				double y_calc = amp*sin(lambda*p->row.sensorTS + phase_add_best + phase + phase_add) + y_avg;

				double y_diff = y_calc - (double)p->row.value;
				//fprintf(stderr, "Y: %li %i %lf %le\n", p->sensorTS, p->value, y_calc, y_diff);

				sqdeviation += y_diff*y_diff;
				p++;
			}
			//fprintf(stderr, "try: %le: %le\n", phase_add_best + phase_add, sqdeviation);
			if (sqdeviation < sqdeviation_min) {
				sqdeviation_min    = sqdeviation;
				phase_add_best_add = phase_add;
			}

			phase_add += scan_interval/SIN_PHASE_PARTS;
		}
		phase_add_best += phase_add_best_add;
		scan_interval /= SIN_PHASE_PARTS;
		//fprintf(stderr, "phase_add_best == %le (%le). sqdeviation_min == %le. scan_interval == %le\n", phase_add_best, phase_add_best_add, sqdeviation_min, scan_interval);
	}

	double err = sqdeviation_min/value_history_filled/(value_history_filled-1);

	if (err > error_threshold) {
		p = value_history;
		while (p <= e) {
			printf("Z\t%lu\t%lu\t%u\t%lf\t%lf\t%lf\t%lf\t%lf\n", p->row.unixTSNano, p->row.sensorTS, p->row.value, err, amp, lambda, phase, y_avg);
			p++;
		}
	}

	return 0;
}

int vl_check_sin(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointfile, int concurrency, void *_frequency_p, double error_threshold, char realtime) {
	float frequency = *(float *)_frequency_p;

	//history_item_t *cur = &value_history[*value_history_filled_p - 1];

	uint64_t expectedEndOffset = (uint64_t)(1E9 /* nanosecs in sec */ / frequency);
	uint64_t     currentOffset = *value_history_filled_p * 50000;

	int rc = 0;
	if ( currentOffset  >=  expectedEndOffset ) {

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

		history_item_ptr->row.unixTSNano = get_uint64(i_f, realtime);
		history_item_ptr->row.sensorTS   = get_uint64(i_f, realtime);
		history_item_ptr->row.value      = get_uint32(i_f, realtime);

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


