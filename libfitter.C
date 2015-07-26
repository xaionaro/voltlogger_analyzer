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
fitter(history_item_t *history, uint64_t filled, float frequency)
{
	//TGraph gr(filename, "%*lg %lg %lg");
	uint64_t i;


	uint32_t yMin = history->value, yMax = history->value;
	double sum = 0;

	printf("filled == %li; %p\n", filled, gr);
	gr->Set(filled);

	i = 0;
	while (i < filled) {
		x[i] = history[i].unixTSNano;
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

	uint64_t unixTSNanoDiff = x[filled-1] - x[0];
	double   lambda = (double)2*M_PI / frequency / 1E9;
	double   amp    = (yMax - yMin) / 2;
	//double   avg    = (yMax + yMin) / 2;
	double   avg = sum / filled;
	static double phase = -1;


	//TF1 f1("f1", fitfunc, x[0], x[filled-1], 4);

	f1->SetParameters(amp, lambda, phase < 0 ? 0 : phase, avg);
	f1->SetParLimits(0, amp*0.7, amp*1.3);
	f1->SetParLimits(1, lambda*0.9, lambda*1.1);
	if (phase < 0) {
		f1->SetParLimits(2, 0, 2*M_PI);
	} else {
		f1->SetParLimits(2, phase*0.9, phase*1.1);
	}
	f1->SetParLimits(3, avg, avg);

	gr->Fit(f1, "WMROQ");
/*
	printf("chi2/ndf = %lf\n", f1->GetChisquare() / f1->GetNDF());
	printf("probability = %lf\n", f1->GetProb());
	printf("lambda = %e\n", lambda);
	printf("amp = %e\n", amp);
	printf("avg = %e\n", avg);
	printf("par0 = %e\n", f1->GetParameter(0));
	printf("par1 = %e\n", f1->GetParameter(1));
	printf("par2 = %e\n", f1->GetParameter(2));
	printf("par3 = %e\n", f1->GetParameter(3));
*/

	phase = f1->GetParameter(2);
	return f1->GetChisquare() / f1->GetNDF();
}
