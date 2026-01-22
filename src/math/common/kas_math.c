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

#include "ds_math.h"
#include "sys_public.h"

u32 is_power_of_two(const u64 n)
{
	/* k > 0: 2^k =>   (10... - 1)  =>   (01...) = 0
	 *               & (10...    )     & (10...)
	 */

	/* k > 0: NOT 2^k =>   (1XXX10... - 1)  =>   (1XXX01...) = (1XXX00...
	 *                   & (1XXX10...    )     & (1XXX10...)
	 */

	return (n & (n-1)) == 0 && n > 0;
}

u64 power_of_two_ceil(const u64 n)
{
	if (n == 0)
	{
		return 1;
	}

	if (is_power_of_two(n))
	{
		return n;
	}

	/* [1, 63] */
	const u32 lz = clz64(n);
	ds_AssertString(lz > 0, "Overflow in power_of_two_ceil");
	return (u64) 0x8000000000000000 >> (lz-1);
}

