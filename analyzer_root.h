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

#ifndef __ANALYZER_ROOT_H
#define __ANALYZER_ROOT_H

#include <stdint.h>

extern void root_analyze_sin(FILE *in, FILE *out, float frequency, double error_threshold, char realtime);

struct history_item {
	uint64_t		 unixTSNano;
	uint64_t		 sensorTS;
	uint32_t		 value;
	void			*procdata;
};
typedef struct history_item history_item_t;

#endif

