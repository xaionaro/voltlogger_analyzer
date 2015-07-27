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
#include <math.h>
#include <stdlib.h>

#include "malloc.h"
#include <stdint.h>

#include "analyzer_root.h"

double fitfunc(double *x, double *par)
{
	return par[0] * sin(par[1] * (*x) + par[2]) + par[3];
}

double *x = NULL;
double *y = NULL;
static TGraph *gr = NULL;
static TF1 *f1 = NULL;

int
fitter_init(uint64_t history_size) {
	x = (double *)calloc(history_size, sizeof(*x));
	y = (double *)calloc(history_size, sizeof(*x));
	gr = new TGraph();//history_size);
	f1 = new TF1("f1", fitfunc, 0, 1, 4);
}

int
fitter_deinit() {
	free(x);
	free(y);
	delete gr;
	delete f1;
}

double
fitter(history_item_t *history, uint64_t filled, float frequency, double *par)
{
	//TGraph gr(filename, "%*lg %lg %lg");
	uint64_t i;


	uint32_t yMin = history->value, yMax = history->value;
	double sum = 0;

	printf("filled == %li; %p\n", filled, gr);
	gr->Set(filled);

	i = 0;
	while (i < filled) {
		//x[i] = history[i].unixTSNano;
		x[i] = history[i].sensorTS;
		y[i] = history[i].value;

		if (y[i] < yMin)
			yMin = y[i];

		if (y[i] > yMax)
			yMax = y[i];

	//	printf("%li %li %i\n", i, history[i].unixTSNano, history[i].value);
		gr->SetPoint(i, x[i], y[i]);
		sum += y[i];
		i++;
	}

	f1->SetRange(x[0], x[filled-1]);

	uint64_t TSDiff       = x[filled-1] - x[0];
	uint64_t unixTSDiff   = history[filled-1].unixTSNano - history[0].unixTSNano;
	double   lambda       = (double)2*M_PI / TSDiff;
	static double amp     = -1;
	//double   avg          = (yMax + yMin) / 2;
	static double avg_pre;
	avg_pre = sum / filled;
	static double avg     = -1;
	static double phase   = -1;


	//TF1 f1("f1", fitfunc, x[0], x[filled-1], 4);

	double amp_pre = (yMax - yMin) / 2;
//	f1->SetErrors(amp*0.01, lambda*0.01, phase*0.01, avg*0.01);
	f1->SetParameters(amp < 0 ? amp_pre : amp, lambda, phase < 0 ? 0 : phase, avg < 0 ? avg_pre : avg);
	//f1->SetParameters(amp_pre, lambda, phase < 0 ? 0 : phase, avg);
	if (amp < 0) {
		f1->SetParLimits(0, amp_pre*0.9, amp_pre*1.1);
	} else {
		f1->SetParLimits(0, amp*0.9999, amp*1.0001);
	}
	//f1->SetParLimits(1, lambda*0.9, lambda*1.1);
	f1->SetParLimits(1, lambda*0.9999, lambda*1.0001);
	if (phase < 0) {
		f1->SetParLimits(2, 0, 2*M_PI);
	} else {
		f1->SetParLimits(2, phase-(0.0001*2*M_PI), phase+(0.0001*2*M_PI));
	}
	if (avg < 0) {
		f1->SetParLimits(3, avg_pre*0.9999, avg_pre*1.0001);
	} else {
		f1->SetParLimits(3, avg*0.9999, avg*1.0001);
	}

	gr->Fit(f1, "WROQ");

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
		par[0] = f1->GetParameter(0);
		par[1] = f1->GetParameter(1);//*(double)TSDiff/(double)unixTSDiff;
		par[2] = f1->GetParameter(2);
		par[3] = f1->GetParameter(3);
	}

	if (f1->GetChisquare() / f1->GetNDF() < 100000) {
		amp   = f1->GetParameter(0);
		phase = f1->GetParameter(2);
		avg   = f1->GetParameter(3);
	}
	return f1->GetChisquare() / f1->GetNDF();
}
