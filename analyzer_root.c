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
#include <sys/prctl.h>	/* prctl()	*/


#include "configuration.h"
#include "error.h"
#include "malloc.h"
#include "analyzer_root.h"
#include "binary.h"
#include "pthreadex.h"
#include "libfitter.h"

#define MAX_HISTORY (1 << 16)


typedef int (*root_checkfunct_t)(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointpath, int concurrency, double error_threshold, void *arg);


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

struct checkpoint {
	double par[4];
	uint64_t unixTSNano;
	uint64_t statistics_events;
};
typedef struct checkpoint checkpoint_t;

int root_realcheck_sin(int proc_id, int concurrency, char *checkpointpath, history_item_t *value_history, uint64_t value_history_filled, double error_threshold, float frequency) {
	double par[4];
	int rc;

	/*if (proc_id != 0) {
		fprintf(stderr, "multi-procing is not supported, yet");
		exit(-1);
	}*/

	//static uint64_t statistics_events = 0;
	double error = _Z6fitteriP12history_itemmfPd(0, value_history, value_history_filled, frequency, par);

	if (concurrency > 1)
		concurrency--;

	if (error > error_threshold)
		rc = 1;

	if (proc_id == 0) {
		procs_meta->statistics_events++;
	}

	printf("_Z6fitterP12history_itemmf -> %lf %lu %lu\n", error, value_history->row.unixTSNano, procs_meta->statistics_events);


	if (proc_id == 0) {
		if ((procs_meta->statistics_events & 0xff) == 0) {
			if (checkpointpath != NULL) {
				checkpoint_t checkpoint;
				FILE *checkpointfile;
				memcpy(checkpoint.par, par, sizeof(par));
				checkpoint.unixTSNano        = value_history[value_history_filled-1].row.unixTSNano;
				checkpoint.statistics_events = procs_meta->statistics_events;
				fprintf(stderr, "Saving a checkpoint: %lu\n", checkpoint.unixTSNano);
				//assert ( (checkpointfile = tmpfile()) != NULL );
				checkpointfile = fopen(checkpointpath, "w");
				if (checkpointfile == NULL) {
					fprintf(stderr, "Cannot open file \"%s\" for writing: %s\n", checkpointpath, strerror(errno));
					abort ();
				}
				assert ( fwrite(&checkpoint, sizeof(checkpoint), 1, checkpointfile) >= 1 );
				fclose (checkpointfile);
			}
		}
	}

	if (rc && procs_meta->statistics_events > 10000) {
		uint64_t i;
		i = 0;

		fprintf(stderr, "proc_id == %i, procs_meta->canprint_proc == %i\n", proc_id, procs_meta->canprint_proc);

		while (proc_id != procs_meta->canprint_proc);

		while (i < value_history_filled) {
			printf("Z\t%lu\t%lu\t%u\t%lf\t%lf\t%lf\t%lf\t%lf\t%li\n", value_history[i].row.unixTSNano, value_history[i].row.sensorTS, value_history[i].row.value, error, par[0], par[1], par[2], par[3], procs_meta->statistics_events);
			i++;
		}

		procs_meta->canprint_proc++;
		procs_meta->canprint_proc %= concurrency;
		fprintf(stderr, "procs_meta->canprint_proc --> %i\n", procs_meta->canprint_proc);
	}

//	frequency = (float)2*M_PI / (float)par[1];
//	*(float *)_frequency_p = frequency;

	return rc;
}

pthread_mutex_t masterprocess_lock = PTHREAD_MUTEX_INITIALIZER;

void worker_finished(int sig) {
	pthread_mutex_unlock(&masterprocess_lock);
	return;
}

void worked_died(int sig) {
	fprintf(stderr, "Child died\n");
	if (procs_meta->statistics_events > 10000)
		abort ();
	return;
}

int root_check_sin(history_item_t *value_history, uint64_t *value_history_filled_p, char *checkpointpath,int concurrency, double error_threshold, void *_frequency_p) {
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

		if (concurrency > 1 && procs_meta->statistics_events > 10000) {
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
			fprintf(stderr, "Running the fitting in the master process (%u, %lu)\n", concurrency, procs_meta->statistics_events);
			root_realcheck_sin(0, 1, checkpointpath, value_history, *value_history_filled_p, error_threshold, frequency);
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

int worker(int proc_id, int concurrency, char *checkpointpath) {
	volatile uint64_t counter;
	volatile struct root_realcheckproc_sin_procmeta *procmeta = &procs_meta->p[proc_id];
	fprintf(stderr, "Worker #%i Initializing (checkpointpath: \"%s\").\n", proc_id, checkpointpath);
	parent_pid = getppid();

	signal(SIGUSR1, worker_start);

	procs_meta->p[proc_id].rc = 0;

	pthread_mutex_lock(&thisworker_lock);
	fprintf(stderr, "Worker #%i Initialized (checkpointpath: \"%s\").\n", proc_id, checkpointpath);

	while (1) {
		fprintf(stderr, "Worker #%i: Waiting for a job.\n", proc_id);
		counter = 0;
		while (procs_meta->p[proc_id].rc != -1)
			if (counter++ > SPINLOCK_TIMEOUT_ITERATIONS)
				while (pthread_mutex_reltimedlock(&thisworker_lock, 1, 0) == ETIMEDOUT)
					if (!parent_isalive())
						return 0;
		fprintf(stderr, "Worker #%i: I have a job.\n", proc_id);

		procmeta->rc = root_realcheck_sin(proc_id, concurrency, checkpointpath, (void *)procmeta->value_history, procmeta->value_history_filled, procmeta->error_threshold, procmeta->frequency);
		kill(parent_pid, SIGUSR1);
	}
	fprintf(stderr, "Worker #%i: Finished.\n", proc_id);

	return 0;
}

int seekto_unixTSNano(FILE *i_f, uint64_t unixTSNano) {
	uint64_t unixTSNano_s, unixTSNano_e, unixTSNano_c;
	int64_t  unixTSNano_diff, unixTSNano_diff_req;
	int iterations;
	double scale;

	long pos = 0;


	assert (fseek(i_f, 0,					SEEK_SET) != -1);

	unixTSNano_s = get_uint64(i_f, 0);
	
	assert (fseek(i_f, 0,					SEEK_END) != -1);
	long pos_end = ftell(i_f) / sizeof(history_item_row_t);

	pos_end--;

	assert (fseek(i_f, pos_end*sizeof(history_item_row_t),SEEK_SET) != -1);

	unixTSNano_e    = get_uint64(i_f, 0);

	unixTSNano_diff = (int64_t)unixTSNano_e - (int64_t)unixTSNano_s;
	scale = (double)pos_end / (double)unixTSNano_diff;

	fprintf(stderr, "%lu %lu\n", unixTSNano, unixTSNano_s);
	assert (unixTSNano > unixTSNano_s);

	unixTSNano_diff_req = (int64_t)unixTSNano - (int64_t)unixTSNano_s;

	assert (unixTSNano_e > unixTSNano_s);

	pos = (double)unixTSNano_diff_req*scale;

	fprintf(stderr, "pos == %li (/%li)\n", pos, pos_end);
	assert (fseek(i_f, pos*sizeof(history_item_row_t), SEEK_SET)	!= -1);

	unixTSNano_c    = get_uint64(i_f, 0);

	assert (unixTSNano_c >= unixTSNano_s);

	if (unixTSNano_c == unixTSNano)
		return 0;

	iterations = 0;
	while (1) {
		unixTSNano_diff_req = (int64_t)unixTSNano - (int64_t)unixTSNano_c;

		long pos_diff = (double)unixTSNano_diff_req * scale;

		if (pos_diff == 0)
			pos_diff = unixTSNano_diff_req > 0 ? 1 : -1;

		fprintf(stderr, "pos_diff == %li (%lu [%lu %lu] %le)\n", pos_diff, unixTSNano_diff_req, unixTSNano, unixTSNano_c, scale);

		pos += pos_diff;

		if (pos > pos_end)
			pos = pos_end;
		if (pos < 0)
			pos = 0;

		fprintf(stderr, "pos == %li\n", pos);

		assert (fseek(i_f, pos*sizeof(history_item_row_t), SEEK_SET) != -1);

		fprintf(stderr, "pos_cur*sizeof() == %li\n", ftell(i_f));

		unixTSNano_c    = get_uint64(i_f, 0);
		assert (unixTSNano_c >= unixTSNano_s);

		if (unixTSNano_c == unixTSNano) {
			assert (fseek(i_f, (pos+1)*sizeof(history_item_row_t), SEEK_SET)  != -1);
			return 0;
		}

		assert (iterations++ < 65536);
	}

	return 2;
}

void root_analize(FILE *i_f, FILE *o_f, char *checkpointpath, int concurrency, void *arg, root_checkfunct_t checkfunct, double error_threshold, char realtime) {
	static uint32_t unixTS_old = 0;
	history_item_t *value_history, *history_item_ptr;
	uint64_t        value_history_filled = 0;
	char *checkpointpath_eff = checkpointpath;

	if (concurrency > 1)
		assert (unshare(CLONE_NEWIPC) == 0);

	value_history = xcalloc(MAX_HISTORY, sizeof(*value_history));
	procs_meta = shm_calloc(1, sizeof(*procs_meta));

	if (access(checkpointpath, R_OK)) {
		checkpointpath_eff = NULL;
	}

	if (checkpointpath_eff == NULL) {
		_Z11fitter_initmPd(MAX_HISTORY, NULL);
	} else {
		checkpoint_t checkpoint;

		FILE *checkpointfile;

		assert ( (checkpointfile = fopen(checkpointpath_eff, "r"))		!= NULL);
		assert ( fread(&checkpoint, sizeof(checkpoint), 1, checkpointfile)	>= 1);
		assert ( seekto_unixTSNano(i_f, checkpoint.unixTSNano)			== 0);

		_Z11fitter_initmPd(MAX_HISTORY, checkpoint.par);
		procs_meta->statistics_events = checkpoint.statistics_events;

		fclose(checkpointfile);
	}

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
				exit (worker(i, concurrency, checkpointpath));
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


	if (concurrency > 1) {
		prctl(PR_SET_PDEATHSIG, SIGCHLD);
		signal(SIGUSR1, worker_finished);
		signal(SIGCHLD,	worked_died);
	}
	//fprintf(stderr, "Running\n");

	while (1) {
		assert (value_history_filled < MAX_HISTORY);	// overflow

		history_item_ptr = &value_history[ value_history_filled++ ];

		history_item_ptr->row.unixTSNano = get_uint64(i_f, realtime);
		history_item_ptr->row.sensorTS   = get_uint64(i_f, realtime);
		history_item_ptr->row.value      = get_uint32(i_f, realtime);

		uint32_t unixTS = history_item_ptr->row.unixTSNano / 1E9;
		if (unixTS != unixTS_old) {
			fprintf(stderr, "TS: %u\n", unixTS);
			unixTS_old = unixTS;
		}

		if (checkfunct(value_history, &value_history_filled, checkpointpath, concurrency, error_threshold, arg)) {
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

void root_analyze_sin(FILE *i_f, FILE *o_f, char *checkpointpath, int concurrency, float frequency, double error_threshold, char realtime)
{
	float *frequency_p = xmalloc(sizeof(float));
	*frequency_p = frequency;

	root_analize(i_f, o_f, checkpointpath, concurrency, frequency_p, root_check_sin, error_threshold, realtime);

	free(frequency_p);
	return;
}

