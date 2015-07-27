/*
    libfitter - a library to fit values to sinus using ROOT

    Copyright (C) 2015  Andrew A Savchenko <bircoph@gentoo.org>
                        Dmitry Yu Okunev <dyokunev@ut.mephi.ru>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <TGraph.h>
#include <TF1.h>
#include <TVirtualFitter.h>
#include <math.h>
#include <stdlib.h>

#include "malloc.h"
#include <stdint.h>

#include "configuration.h"
#include "analyzer_root.h"

#define MAX_THREADS 1

double fitfunc(double *x, double *par)
{
	return par[0] * sin(par[1] * (*x) + par[2]) + par[3];
}

//double *x[MAX_THREADS] = {NULL};
//double *y[MAX_THREADS] = {NULL};
static TGraph *gr[MAX_THREADS] = {NULL};
static TF1 *f1[MAX_THREADS] = {NULL};

static double amp     = -1;
static double avg     = -1;
static double phase   = -1;

int
fitter_init(uint64_t history_size, double *par) {
	int i = 0;

	TVirtualFitter::SetDefaultFitter("Minuit2");
	//TVirtualFitter::SetPrecision(1E-4);
	//TVirtualFitter::SetMaxIterations(100);

	while (i < MAX_THREADS) {
		gr[i] = new TGraph(history_size);
		f1[i] = new TF1("f1", fitfunc, 0, 1, 4);

		i++;
	}

	if (par != NULL) {
		amp   = par[0];
		phase = par[2];
		avg   = par[3];
	}
}

int
fitter_deinit() {
	int i = 0;

	while (i < MAX_THREADS) {
		delete gr[i];
		delete f1[i];
		i++;
	}
}

double
fitter(int thread_id, history_item_t *history, uint64_t filled, float frequency, double *par)
{
	//TGraph gr(filename, "%*lg %lg %lg");
	uint64_t i;


	uint32_t yMin = history->row.value, yMax = history->row.value;
	double deltaXMax = 0;
	double sum = 0;
	double x_old = history[0].row.sensorTS;

	//printf("filled == %li; %p %p\n", filled, gr[thread_id], f1[thread_id]);
	gr[thread_id]->Set(filled);

	i = 0;
	while (i < filled) {
		double x, y, deltaX;

		x = history[i].row.sensorTS;

		deltaX = x - x_old;
		x_old = x;
		if (deltaX > deltaXMax)
			deltaXMax = deltaX;

		y = history[i].row.value;
		//x[i] = history[i].unixTSNano;
//		x[i] = history[i].sensorTS;
//		y[i] = history[i].value;

		if (y < yMin)
			yMin = y;

		if (y > yMax)
			yMax = y;

	//	printf("%li %li %i\n", i, history[i].unixTSNano, history[i].value);
		gr[thread_id]->SetPoint(i, x, y);
		sum += y;
		i++;
	}

	uint64_t x_start = history[0].row.sensorTS;
	uint64_t x_end   = history[filled-1].row.sensorTS;

	f1[thread_id]->SetRange(x_start, x_end);

	uint64_t TSDiff       = x_end - x_start;
	uint64_t unixTSDiff   = history[filled-1].row.unixTSNano - history[0].row.unixTSNano;
	double   lambda       = (double)2*M_PI / TSDiff;
	//double   avg          = (yMax + yMin) / 2;
	static double avg_pre;
	avg_pre = sum / filled;

	if (deltaXMax/10 > ((double)TSDiff/(double)filled))
		return -1;

	volatile double amp_cur   = amp;
	volatile double avg_cur   = avg;
	volatile double phase_cur = phase;


	//TF1 f1[]("f1", fitfunc, x[0], x[filled-1], 4);

	double amp_pre = (yMax - yMin) / 2;
//	f1[thread_id]->SetErrors(amp*0.01, lambda*0.01, phase*0.01, avg*0.01);
	f1[thread_id]->SetParameters(amp_cur < 0 ? amp_pre : amp_cur, lambda, phase < 0 ? 0 : phase, avg_cur < 0 ? avg_pre : avg_cur);
	//f1[thread_id]->SetParameters(amp_pre, lambda, phase < 0 ? 0 : phase, avg);
	if (amp_cur < 0) {
		f1[thread_id]->SetParLimits(0, amp_pre*0.9, amp_pre*1.1);
	} else {
		f1[thread_id]->SetParLimits(0, amp_cur*0.9999, amp_cur*1.0001);
	}
	//f1[thread_id]->SetParLimits(1, lambda*0.9, lambda*1.1);
	f1[thread_id]->SetParLimits(1, lambda*0.9999, lambda*1.0001);
	if (phase_cur < 0) {
		f1[thread_id]->SetParLimits(2, 0, 2*M_PI);
	} else {
		f1[thread_id]->SetParLimits(2, phase_cur-(0.0001*2*M_PI), phase_cur+(0.0001*2*M_PI));
	}
	if (avg_cur < 0) {
		f1[thread_id]->SetParLimits(3, avg_pre*0.9999, avg_pre*1.0001);
	} else {
		f1[thread_id]->SetParLimits(3, avg_cur*0.9999, avg_cur*1.0001);
	}

	gr[thread_id]->Fit(f1[thread_id], "WROQ");

/*
	printf("amp = %e (%lu)\n", amp, (yMax - yMin) / 2);

	printf("chi2/ndf = %lf\n", f1->GetChisquare() / f1->GetNDF());
	printf("probability = %lf\n", f1->GetProb());
	printf("lambda = %e\n", lambda);
	printf("avg = %e\n", avg);
	printf("par0 = %e\n", f1->GetParameter(0));
	printf("par1 = %e\n", f1->GetParameter(1));
	printf("par2 = %e\n", f1->GetParameter(2));
	printf("par3 = %e\n", f1->GetParameter(3));
*/
	if (par != NULL) {
		par[0] = f1[thread_id]->GetParameter(0);
		par[1] = f1[thread_id]->GetParameter(1);//*(double)TSDiff/(double)unixTSDiff;
		par[2] = f1[thread_id]->GetParameter(2);
		par[3] = f1[thread_id]->GetParameter(3);
	}

	if (thread_id == 0) {
		if (f1[thread_id]->GetChisquare() / f1[thread_id]->GetNDF() < 100000) {
			amp   = f1[thread_id]->GetParameter(0);
			phase = f1[thread_id]->GetParameter(2);
			avg   = f1[thread_id]->GetParameter(3);
		}
	}
	return f1[thread_id]->GetChisquare() / f1[thread_id]->GetNDF();
}
