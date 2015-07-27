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

#include <stdint.h>

#include "analyzer_root.h"

extern void   _Z11fitter_initm   (uint64_t history_size);
extern void   _Z13fitter_deinitv ();
extern double _Z7fitfuncPdS_     ( double *x, double *par );
extern double _Z6fitteriP12history_itemmfPd ( int thread_id, history_item_t *history, uint64_t filled, float frequency, double *par );

