/*
    voltlogger_oscilloscope
    
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


#include <assert.h>	/* assert()	*/
#include <unistd.h>
#include <stdio.h>

#include "configuration.h"
#include "binary.h"


#define DECLARE_GET_X_t(x) 								\
	x ## _t get_ ## x (FILE *i_f, char realtime) {					\
		x ## _t r;								\
											\
											\
		if (fread(&r, sizeof(r), 1, i_f) < 1) {					\
			if (feof(i_f)) {						\
				if (realtime) {						\
					do {						\
						usleep(RETRY_DELAY_USECS);		\
						clearerr(i_f);				\
						if (fread(&r, sizeof(r), 1, i_f) < 1 && !feof(i_f))\
							assert (ferror(i_f));		\
						return r;				\
					} while (feof(i_f));				\
				}							\
			}								\
			assert (ferror(i_f));						\
		}									\
											\
		return r;								\
	}

DECLARE_GET_X_t(uint64);
DECLARE_GET_X_t(uint32);
DECLARE_GET_X_t(uint16);
DECLARE_GET_X_t(uint8);

