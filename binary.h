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
#include <stdio.h>	/* FILE		*/
#include <stdint.h>	/* uint64_t	*/

extern uint64_t get_uint64(FILE *i_f, char realtime);
extern uint32_t get_uint32(FILE *i_f, char realtime);
extern uint16_t get_uint16(FILE *i_f, char realtime);
extern uint8_t  get_uint8 (FILE *i_f, char realtime);

