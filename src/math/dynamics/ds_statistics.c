/*
==========================================================================
    Copyright (C) 2026 Axel Sandstedt 

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

void ds_DynamicsStatsFlush(struct ds_DynamicsStats *stats)
{
    stats->hull_call_count = 0;
    stats->hull_cache_count = 0;
    stats->hull_eviction_count = 0;

    stats->mesh_hull_call_count = 0;
    stats->mesh_hull_cache_count = 0;
    stats->mesh_hull_eviction_count = 0;
}

void ds_DynamicsStatsAdd(struct ds_DynamicsStats *sum, const struct ds_DynamicsStats *stats)
{
   sum->hull_call_count += stats->hull_call_count;
   sum->hull_cache_count += stats->hull_cache_count;
   sum->hull_eviction_count += stats->hull_eviction_count;

   sum->mesh_hull_call_count += stats->mesh_hull_call_count;
   sum->mesh_hull_cache_count += stats->mesh_hull_cache_count;
   sum->mesh_hull_eviction_count += stats->mesh_hull_eviction_count;
}

void ds_DynamicsStatsPrint(FILE *file, const struct ds_DynamicsStats *stats)
{
    fprintf(file, "================= Statistics ==============\n");

    if (stats->mesh_hull_call_count)
    {
        const f32 valid_cache_count = stats->mesh_hull_cache_count - stats->mesh_hull_eviction_count;
        const f32 mesh_hull_full_compute_rate = 100.0f - 100.0f * valid_cache_count / stats->mesh_hull_call_count;
        fprintf(file, "mesh_hull_full_compute_rate: %.2f%%\n", mesh_hull_full_compute_rate);
    }

    if (stats->mesh_hull_cache_count)
    {
        const f32 mesh_hull_eviction_rate = 100.0f * (f32) stats->mesh_hull_eviction_count / stats->mesh_hull_cache_count;
        fprintf(file, "mesh_hull_eviction_rate:     %.2f%%\n", mesh_hull_eviction_rate);
    }

    if (stats->hull_call_count)
    {
        const f32 valid_cache_count = stats->hull_cache_count - stats->hull_eviction_count;
        const f32 hull_full_compute_rate = 100.0f - 100.0f * valid_cache_count / stats->hull_call_count;
        fprintf(file, "hull_full_compute_rate:      %.2f%%\n", hull_full_compute_rate);
    }

    if (stats->hull_cache_count)
    {
        const f32 hull_eviction_rate = 100.0f * (f32) stats->hull_eviction_count / stats->hull_cache_count;
        fprintf(file, "hull_eviction_rate:          %.2f%%\n", hull_eviction_rate);
    }
}
