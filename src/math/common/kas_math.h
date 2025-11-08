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

#ifndef __MORE_MATH_H__
#define __MORE_MATH_H__

#include "kas_common.h"
#include <stdbool.h>


#include "matrix.h"
#include "vector.h"
#include "quaternion.h"
#include "float32.h"

#define MM_PI_F       3.14159f
#define MM_PI_2_F     (2.0f * MM_PI_F)

#define MM_PI       3.14159265358979323846
#define MM_PI_2     (2.0 * MM_PI)

/* Return 1 if n = 2*k for some k >= 0, otherwise return 0 */
u32	is_power_of_two(const u64 n);
/* Return smallest value 2^k >= n where k >= 0 */
u64	power_of_two_ceil(const u64 n); 

#endif
