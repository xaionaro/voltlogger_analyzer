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

#define _GNU_SOURCE	/* for unshare() */

#include "configuration.h"
#include "macros.h"

#include <stdio.h>	/* FILE		*/
#include <stdlib.h>	/* size_t	*/
#include <stdint.h>	/* uint64_t	*/
#include <unistd.h>	/* usleep()	*/
#include <string.h>	/* memset()	*/
#include <math.h>	/* fabs()	*/
#include <assert.h>	/* assert()	*/
#include <sched.h>	/* unshare()	*/
#include <sys/types.h>	/* waitpid()	*/
#include <sys/wait.h>	/* waitpid()	*/
#include <errno.h>	/* errno	*/
#include <pthread.h>	/* PTHREAD_MUTEX_INITIALIZER */


#include "configuration.h"
#include "error.h"
#include "malloc.h"
#include "analyzer_root.h"
#include "binary.h"
#include "pthreadex.h"
#include "libfitter.h"

#define MAX_HISTORY (1 << 16)


typedef int (*root_checkfunct_t)(history_item_t *value_history, uint64_t *value_history_filled_p, int concurrency, double error_threshold, void *arg);


struct root_realcheckproc_sin_procmeta {
	int		 proc_id;
	int		 concurrency;
	history_item_t	 value_history[MAX_HISTORY];
	uint64_t	 value_history_filled;
	double		 error_threshold;
	float		 frequency;

	pid_t		 pid;
	int		 rc;
};
struct root_realcheckproc_sin_procmetas {
	struct root_realcheckproc_sin_procmeta	p[MAX_PROCS];
	int canprint_proc;
	uint64_t statistics_events;
};
volatile struct root_realcheckproc_sin_procmetas *procs_meta;

int root_realcheck_sin(int proc_id, int concurrency, history_item_t *value_history, uint64_t value_history_filled, double error_threshold, float frequency) {
	double par[4];
	int rc;

	/*if (proc_id != 0) {
		fprintf(stderr, "multi-procing is not supported, yet");
		exit(-1);
	}*/

	static uint64_t statistics_events = 0;
	double error = _Z6fitteriP12history_itemmfPd(0, value_history, value_history_filled, frequency, par);

	if (error > error_threshold)
		rc = 1;

	if (proc_id == 0) {
		procs_meta->statistics_events++;
	}

	printf("_Z6fitterP12history_itemmf -> %lf %lu %lu\n", error, value_history->unixTSNano, procs_meta->statistics_events);

	if (rc && statistics_events > 10000) {
		uint64_t i;
		i = 0;

		while (proc_id != procs_meta->canprint_proc);

		while (i < value_history_filled) {
			printf("Z\t%lu\t%lu\t%u\t%lf\t%lf\t%lf\t%lf\t%lf\n", value_history[i].unixTSNano, value_history[i].sensorTS, value_history[i].value, error, par[0], par[1], par[2], par[3]);
			i++;
		}

		procs_meta->canprint_proc++;
		procs_meta->canprint_proc %= concurrency;
	}

//		frequency = (float)2*M_PI / (float)par[1];
//		*(float *)_frequency_p = frequency;

	return rc;
}

pthread_mutex_t masterprocess_lock = PTHREAD_MUTEX_INITIALIZER;

void worker_finished(int sig) {
	pthread_mutex_unlock(&masterprocess_lock);
}

int root_check_sin(history_item_t *value_history, uint64_t *value_history_filled_p, int concurrency, double error_threshold, void *_frequency_p) {
	float frequency = *(float *)_frequency_p;
	//double par[4];
	static int concurrency_current = 0;

	//history_item_t *cur = &value_history[*value_history_filled_p - 1];

	uint64_t expectedEndOffset_unixTSNano = (uint64_t)(1 * 1E9 /* nanosecs in sec */ / frequency);
//	uint64_t     currentOffset_unixTSNano = cur->unixTSNano - value_history[0].unixTSNano;
	uint64_t     currentOffset_unixTSNano = *value_history_filled_p * 50000;

	int rc = 0;
	if ( currentOffset_unixTSNano  >=  expectedEndOffset_unixTSNano ) {
		if (concurrency_current >= concurrency-1) {
			int i = 0;
			while (i < concurrency_current) {
				volatile uint64_t counter = 0;
				while (procs_meta->p[i].rc == -1)
					if (counter++ > SPINLOCK_TIMEOUT_ITERATIONS)
						while (pthread_mutex_reltimedlock(&masterprocess_lock, 0, MUTEX_DEFAULTTIMEOUT_NSECS) == ETIMEDOUT);

				i++;
			}
			//fprintf(stderr, "Workers finished\n");
			concurrency_current = 0;
		}

		if (concurrency > 1) {
			while (concurrency_current) {
				if (procs_meta->p[concurrency_current-1].rc == -1)
					break;

				concurrency_current--;
			}
			//fprintf(stderr, "Sending to worker #%i\n", concurrency_current);
			volatile struct root_realcheckproc_sin_procmeta *procmeta = &procs_meta->p[concurrency_current];
			memcpy((void *)procmeta->value_history, value_history, sizeof(*value_history) * (*value_history_filled_p));
			procmeta->proc_id              =  concurrency_current;
			procmeta->concurrency          =  concurrency;
			procmeta->value_history_filled = *value_history_filled_p;
			procmeta->error_threshold      =  error_threshold;
			procmeta->frequency            =  frequency;
			procmeta->rc                   =  -1;

			kill (procmeta->pid, SIGUSR1);

			concurrency_current++;
		} else {
			//fprintf(stderr, "Running the fitting in the master process\n");
			root_realcheck_sin(0, 1, value_history, *value_history_filled_p, error_threshold, frequency);
		}
		*value_history_filled_p = 0;
	}

	return rc;
}

pid_t parent_pid;

int parent_isalive() {
	int rc;

	if ((rc=kill(parent_pid, 0))) {
		if (errno == ESRCH) {
			return 0;
		}
	}

	return 1;
}

pthread_mutex_t thisworker_lock = PTHREAD_MUTEX_INITIALIZER;

void worker_start(int sig) {
	pthread_mutex_unlock(&thisworker_lock);
}

int worker(int proc_id, int concurrency) {
	volatile uint64_t counter;
	volatile struct root_realcheckproc_sin_procmeta *procmeta = &procs_meta->p[proc_id];
	parent_pid = getppid();

	signal(SIGUSR1, worker_start);

	procs_meta->p[proc_id].rc = 0;

	pthread_mutex_lock(&thisworker_lock);
	//fprintf(stderr, "Worker #%i Initialized.\n", proc_id);

	while (1) {
		counter = 0;
		while (procs_meta->p[proc_id].rc != -1)
			if (counter++ > SPINLOCK_TIMEOUT_ITERATIONS)
				while (pthread_mutex_reltimedlock(&thisworker_lock, 1, 0) == ETIMEDOUT)
					if (!parent_isalive())
						return 0;
		//fprintf(stderr, "Worker #%i: I have a job.\n", proc_id);

		procmeta->rc = root_realcheck_sin(proc_id, concurrency, (void *)procmeta->value_history, procmeta->value_history_filled, procmeta->error_threshold, procmeta->frequency);
		kill(parent_pid, SIGUSR1);
	}

	return 0;
}

void root_analize(FILE *i_f, FILE *o_f, int concurrency, void *arg, root_checkfunct_t checkfunct, double error_threshold, char realtime) {
	static uint32_t unixTS_old = 0;
	history_item_t *value_history, *history_item_ptr;
	uint64_t        value_history_filled = 0;

	_Z11fitter_initm(MAX_HISTORY);

	if (concurrency > 1)
		assert (unshare(CLONE_NEWIPC) == 0);

	value_history = xcalloc(MAX_HISTORY, sizeof(*value_history));
	procs_meta = shm_calloc(1, sizeof(*procs_meta));

	int i = 0;
	while (i < concurrency-1) {
		volatile struct root_realcheckproc_sin_procmeta *procmeta = &procs_meta->p[i];
		procs_meta->p[i].rc = -1;

		pid_t pid = fork();

		switch (pid) {
			case -1:
				abort ();
				break;
			case 0:
				exit (worker(i, concurrency));
		}

		procmeta->pid = pid;

		i++;
	}
	i = 0;
	while (i < concurrency) {
		//fprintf(stderr, "Waiting for worker #%i\n", i);
		while (procs_meta->p[i].rc != 0);// fprintf(stderr, "p[%i].rc == %i\n", i, procs_meta->p[i].rc);
		i++;
	}

	signal(SIGUSR1, worker_finished);
	//fprintf(stderr, "Running\n");

	while (1) {
		assert (value_history_filled < MAX_HISTORY);	// overflow

		history_item_ptr = &value_history[ value_history_filled++ ];

		history_item_ptr->unixTSNano = get_uint64(i_f, realtime);
		history_item_ptr->sensorTS   = get_uint64(i_f, realtime);
		history_item_ptr->value      = get_uint32(i_f, realtime);

		uint32_t unixTS = history_item_ptr->unixTSNano / 1E9;
		if (unixTS != unixTS_old) {
			fprintf(stderr, "TS: %u\n", unixTS);
			unixTS_old = unixTS;
		}

		if (checkfunct(value_history, &value_history_filled, concurrency, error_threshold, arg)) {
			printf("Problem\n");
		}

		if (feof(i_f)) {
			if (!realtime)
				break;

			while (feof(i_f)) {
				usleep(RETRY_DELAY_USECS);
				clearerr(i_f);
			}
		}
	}

	shm_free((void *)procs_meta);
	free(value_history);

	_Z13fitter_deinitv();

	return;
}

void root_analyze_sin(FILE *i_f, FILE *o_f, int concurrency, float frequency, double error_threshold, char realtime)
{
	float *frequency_p = xmalloc(sizeof(float));
	*frequency_p = frequency;

	root_analize(i_f, o_f, concurrency, frequency_p, root_check_sin, error_threshold, realtime);

	free(frequency_p);
	return;
}

