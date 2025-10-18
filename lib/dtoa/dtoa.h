/*
==========================================================================
    Copyright (C) 2025 Axel Sandstedt 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
==========================================================================
*/

#ifndef __DMG_DTOA_H__
#define __DMG_DTOA_H__

#include "kas_common.h"

/* Initalize dtoa/strod locks and thread count */
void	dmg_dtoa_init(const u32 max_thread_count);

/*
 * Returns nearest machine number to the input decimal string. Ties are broken by the IEEE round-even rule.
 *
 * s00 - string to double
 * se - output pointer to first unconverted character 
 */
double 	dmg_strtod(const char *s00, char **se);

/*
 * Returns a heap-allocated string using malloc representing the double.
 *
 * dd 		- Double to convert
 * mode 	- output format (see dtoa_r docs)
 * ndigits 	- Number of digits (context dependent)
 * decpt 	- returned position of decimal point
 * sign 	- returned position of sign (=1 if negative)
 * rve		- returned pointer to end of string
 *
 * 	example shortest string:
 * 		
 * 		dtoa(dd, 0, 0, &decpt, &sign, NULL)
 */
char *  dmg_dtoa(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve);
/* free dmg_dtoa string */
void 	freedtoa(char *s);

#endif

