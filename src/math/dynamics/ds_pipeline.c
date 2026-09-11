/*
==========================================================================
    Copyright (C) 2025, 2026 Axel Sandstedt 

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

#include <stdlib.h>
#include <string.h>

#include "float32.h"
#include "ds_job.h"

POOL_DEFINE(ds_PhysicsEvent);

struct ds_DynamicsWorker *g_dynamics_worker;

void ds_DynamicsStaticAssert(void)
{
    ds_StaticAssert(sizeof(struct ds_BodyCompute) == DS_CACHE_LINE, "");
}

struct ds_Dynamics ds_DynamicsAlloc(struct arena *mem, const u32 initial_size, const u64 ns_tick, const u64 frame_memory, c_ShapeSDB *cshape_db, ds_BodyPrefabSDB *prefab_db, const u32 worker_count, const u64 worker_frame_size)
{
	struct ds_Dynamics pipeline =
	{
		.gravity = { 0.0f, -GRAVITY_CONSTANT_DEFAULT, 0.0f },
		.ns_tick = ns_tick,
        .timestep = (f32) ns_tick / NSEC_PER_SEC,
		.ns_elapsed = 0,
		.ns_start = 0,
		.frame = ArenaAlloc(mem, frame_memory),
		.frames_completed = 0,
	};

	static u32 init_solver_once = 0;
	if (!init_solver_once)
	{
        const f32 max_linear_velocity_magnitude = 400.0f;
        const f32 max_angular_velocity_magnitude = 10.0f * F32_PI;

		init_solver_once = 1;
		const u32 pgs_iteration_count = 8;
		const u32 ngs_iteration_count = 3;
		const u32 warmup_solver = 1;
		const vec3 gravity = { 0.0f, -GRAVITY_CONSTANT_DEFAULT, 0.0f };
       	const f32 baumgarte_constant = 0.1f;
        const f32 max_linear_correction = 0.2f;
		const f32 linear_dampening = 0.1f;
		const f32 angular_dampening = 0.1f;
		const f32 linear_slop = 0.005f;
		const f32 restitution_threshold = 0.001f;
		const u32 sleep_enabled = 1;
		const f32 sleep_time_threshold = 0.5f;
		f32 sleep_linear_velocity_sq_limit = 0.01f*0.01f; 
		f32 sleep_angular_velocity_sq_limit = 0.05f*0.05f;
		SolverConfigInit(pgs_iteration_count, ngs_iteration_count, warmup_solver, gravity, baumgarte_constant, max_linear_correction, max_linear_velocity_magnitude, max_angular_velocity_magnitude, linear_dampening, angular_dampening, linear_slop, restitution_threshold, sleep_enabled, sleep_time_threshold, sleep_linear_velocity_sq_limit, sleep_angular_velocity_sq_limit);
	}

	ds_AssertString(PowerOfTwoCheck(initial_size), "For simplicity of future data structures, expect pipeline sizes to be powers of two");

	pipeline.body_pool = ds_BodyPoolAlloc(NULL, initial_size, GROWABLE);
    pipeline.body_usage_set = ds_BitSetAlloc(NULL, initial_size, 0, GROWABLE);

    pipeline.joint_pool = ds_JointPoolAlloc(NULL, initial_size, GROWABLE);

	pipeline.shape_pool = ds_ShapePoolAlloc(NULL, initial_size, GROWABLE);
    pipeline.shape_dynamic_usage_set = ds_BitSetAlloc(NULL, initial_size, 0, GROWABLE);
	pipeline.dynamic_bvh = DbvhAlloc(NULL, 2*initial_size, GROWABLE);
	pipeline.static_bvh = DbvhAlloc(NULL, 2*initial_size, GROWABLE);
    pipeline.shape_dirty_set = ds_BitSetAlloc(NULL, initial_size, 0, GROWABLE);
    ds_CPoolAlloc(NULL, pipeline.dirty_shape_query, initial_size, GROWABLE);

	pipeline.event_pool = ds_PhysicsEventPoolAlloc(NULL, 256, GROWABLE);
	ds_DLLFlush(pipeline.event_list);

	pipeline.cshape_db = cshape_db;

    pipeline.contact_pool = ds_ContactPoolAlloc(NULL, 8*initial_size, GROWABLE);
    pipeline.contact_map = ds_HashMapAlloc(NULL, 8*initial_size, 8*initial_size, GROWABLE);

	pipeline.island_pool = ds_IslandPoolAlloc(NULL, initial_size, GROWABLE);
    pipeline.island_high_energy_set = ds_BitSetAlloc(NULL, initial_size, 0, GROWABLE);

    pipeline.margin_on = 0;
	pipeline.margin = COLLISION_DEFAULT_MARGIN;

    pipeline.broad_phase = ArenaPushAligned(mem, sizeof(struct ds_BroadJobPhase), DS_CACHE_LINE);
    pipeline.narrow_phase = ArenaPushAligned(mem, sizeof(struct ds_NarrowJobPhase), DS_CACHE_LINE);
    pipeline.solver_phase = ArenaPushAligned(mem, sizeof(struct ds_SolverJobPhase), DS_CACHE_LINE);
    pipeline.rebuild_phase = ArenaPushAligned(mem, sizeof(struct ds_RebuildJobPhase), DS_CACHE_LINE);
    ds_JobPhaseAlloc(mem, &pipeline.broad_phase->phase, BROAD_JOB_COUNT, ds_BroadJobPhaseDispatch);
    ds_JobPhaseAlloc(mem, &pipeline.narrow_phase->phase, NARROW_JOB_COUNT, ds_NarrowJobPhaseDispatch);
    ds_JobPhaseAlloc(mem, &pipeline.solver_phase->phase, SOLVER_JOB_COUNT, ds_SolverJobPhaseDispatch);
    ds_JobPhaseAlloc(mem, &pipeline.rebuild_phase->phase, REBUILD_JOB_COUNT, ds_RebuildJobPhaseDispatch);

    ds_CGraphAlloc(&pipeline, 4096);
    pipeline.numerics_config = ds_NumericsConfigDefault();

    pipeline.solver_set_pool = ds_SolverSetPoolAlloc(NULL, 4096, GROWABLE);
    const struct slot set_disabled = ds_SolverSetAdd(NULL, &pipeline, 0, 0, 0, 0, 0,  0);
    const struct slot set_static = ds_SolverSetAdd(NULL, &pipeline, 256, 0, 0, 0, 0, 0);
    const struct slot set_active = ds_SolverSetAdd(NULL, &pipeline, 4096, 4096, 4096, 0, 0, 4096);
    ds_Assert(set_disabled.index == SOLVER_SET_DISABLED);
    ds_Assert(set_static.index == SOLVER_SET_STATIC);
    ds_Assert(set_active.index == SOLVER_SET_ACTIVE);

    pipeline.island_to_split = DS_ID_NULL; 

    ds_StaticAssert((u64) &((struct ds_DynamicsWorker *)0)->frame_arr[1] < (u64) &((struct ds_DynamicsWorker *)0)->pad, "");
    ds_StaticAssert((u64) &((struct ds_DynamicsWorker *)0)->frame < (u64) &((struct ds_DynamicsWorker *)0)->pad, "");
    ds_StaticAssert((u64) &((struct ds_DynamicsWorker *)0)->metrics < (u64) &((struct ds_DynamicsWorker *)0)->pad, "");
    pipeline.worker = ArenaPushAligned(mem, worker_count*sizeof(struct ds_DynamicsWorker), DS_CACHE_LINE);
    pipeline.worker_count = worker_count;
    g_dynamics_worker = pipeline.worker;
    for (u32 i = 0; i < pipeline.worker_count; ++i)
    {
        pipeline.worker[i].frame_arr[0] = ArenaAlloc(NULL, worker_frame_size);
        pipeline.worker[i].frame_arr[1] = ArenaAlloc(NULL, worker_frame_size);
        ds_CPoolAlloc(NULL, pipeline.worker[i].draw.debug_segment_pool, 4096, GROWABLE);
    }

    pipeline.profile_length = 8192;
    pipeline.profile_next = pipeline.profile_length;
    pipeline.profile_buf = ArenaPushZero(mem, pipeline.profile_length*sizeof(struct ds_DynamicsProfile));

	return pipeline;
}

void ds_DynamicsFree(struct ds_Dynamics *pipeline)
{
    for (u32 i = 0; i < pipeline->worker_count; ++i)
    {
        ArenaFree(pipeline->worker[i].frame_arr + 0);
        ArenaFree(pipeline->worker[i].frame_arr + 1);
        ds_CPoolDealloc(pipeline->worker[i].draw.debug_segment_pool);
    }

    ds_BitSetDealloc(&pipeline->shape_dynamic_usage_set);
    ds_BitSetDealloc(&pipeline->shape_dirty_set);
    ds_CPoolDealloc(pipeline->dirty_shape_query);
	BvhFree(&pipeline->dynamic_bvh);
	BvhFree(&pipeline->static_bvh);
    ds_ContactPoolDealloc(&pipeline->contact_pool);
    ds_HashMapDealloc(&pipeline->contact_map);
	ds_IslandPoolDealloc(&pipeline->island_pool);
    ds_BitSetDealloc(&pipeline->island_high_energy_set);
	ds_BodyPoolDealloc(&pipeline->body_pool);
	ds_PhysicsEventPoolDealloc(&pipeline->event_pool);
	ds_ShapePoolDealloc(&pipeline->shape_pool);
    ds_JointPoolDealloc(&pipeline->joint_pool);
    ds_CGraphDealloc(pipeline);
    ds_BitSetDealloc(&pipeline->body_usage_set);
    
    for (u32 i = 0; i < pipeline->solver_set_pool.count_max; ++i)
    {
        const struct ds_SolverSet *set = pipeline->solver_set_pool.buf + i;
        if (ds_PoolSlotAllocated(set))
        {
            ds_SolverSetRemove(pipeline, i);
        }
    }
    ds_SolverSetPoolDealloc(&pipeline->solver_set_pool);
}

static void ds_DynamicsClearFrame(struct ds_Dynamics *pipeline)
{
	for (u32 i = 0; i < pipeline->worker_count; ++i)
	{
        ds_CPoolFlush(pipeline->worker[i].draw.debug_segment_pool);
	}
	ArenaFlush(&pipeline->frame);
    ds_CGraphFramePrepare(pipeline);
    ds_BitSetClear(&pipeline->island_high_energy_set, 0);
}


void ds_DynamicsFlush(struct ds_Dynamics *pipeline)
{
    for (u32 i = 0; i < pipeline->worker_count; ++i)
    {
        ArenaFlush(pipeline->worker[i].frame_arr + 0);
        ArenaFlush(pipeline->worker[i].frame_arr + 1);
        ds_CPoolFlush(pipeline->worker[i].draw.debug_segment_pool);
    }

    memset(pipeline->profile_buf, 0, pipeline->profile_length*sizeof(struct ds_DynamicsProfile));
    pipeline->profile_next = pipeline->profile_length;

    ds_SolverSetFlush(pipeline, 0);
    ds_SolverSetFlush(pipeline, 1);
    ds_SolverSetFlush(pipeline, 2);
    for (u32 i = SOLVER_SET_SLEEPING_FIRST; i < pipeline->solver_set_pool.count_max; ++i)
    {
        const struct ds_SolverSet *set = pipeline->solver_set_pool.buf + i;
        if (ds_PoolSlotAllocated(set))
        {
            ds_SolverSetRemove(pipeline, i);
        }
    }

    ds_ContactPoolFlush(&pipeline->contact_pool);
    ds_HashMapFlush(&pipeline->contact_map);
	ds_IslandPoolFlush(&pipeline->island_pool);
	
	ds_BodyPoolFlush(&pipeline->body_pool);
    ds_BitSetClear(&pipeline->body_usage_set, 0);

    ds_BitSetClear(&pipeline->shape_dynamic_usage_set, 0);
    ds_BitSetClear(&pipeline->shape_dirty_set, 0);
    ds_CPoolFlush(pipeline->dirty_shape_query);
	DbvhFlush(&pipeline->dynamic_bvh);
	DbvhFlush(&pipeline->static_bvh);
	ds_ShapePoolFlush(&pipeline->shape_pool);

	ds_PhysicsEventPoolFlush(&pipeline->event_pool);
	ds_DLLFlush(pipeline->event_list);

    ds_JointPoolFlush(&pipeline->joint_pool);
    ds_CGraphFlush(pipeline);

	ArenaFlush(&pipeline->frame);
	pipeline->frames_completed = 0;
	pipeline->ns_elapsed = 0;
}

void ds_DynamicsValidate(const struct ds_Dynamics *pipeline)
{
	ProfZone;

    for (u32 i = 0; i < pipeline->solver_set_pool.count_max; ++i)
    {
        const struct ds_SolverSet *set = pipeline->solver_set_pool.buf + i;
        if (ds_PoolSlotAllocated(set))
        {
            ds_SolverSetValidate(pipeline, i);
        }
    }

    ds_CGraphValidate(pipeline);
	ds_ContactValidateAll(pipeline);
	ds_IslandValidateAll(pipeline);

	ProfZoneEnd;
}

u32 ds_BroadJobPhaseDispatch(const ds_JobId job)
{
    ProfZone;

    struct ds_BroadJobPhase *phase = (struct ds_BroadJobPhase *) g_scheduler->phase;
    struct ds_Dynamics *pipeline = phase->pipeline;
    struct ds_ParallelForChain *chain = &phase->pf;
    struct ds_ParallelFor *pf = chain->parallel_for + 0;
    struct ds_BitSet *dirty = &pipeline->shape_dirty_set;
    struct ds_ProxyQuery *query = pipeline->dirty_shape_query.buf;
    struct arena *frame = pipeline->worker[ds_ThreadSelfIndex()].frame;
    struct arena *tmp = ArenaPushScratch();
    u32 low, high;

    ds_ParallelFor(pf, range_index)
    {
        ds_ParallelForRange(&low, &high, pf, range_index);
        for (u64 block = low; block < high; ++block)
        {
            struct ds_BitBlock it = ds_BitBlockInit(dirty->bits[block], block);
		    while (ds_BitBlockHasNext(&it))
		    {
                ArenaPushRecord(tmp);

                const u32 si = ds_BitBlockNext(&it);
                const struct ds_Shape *shape = pipeline->shape_pool.buf + si;
                const struct bvhNode *node = pipeline->dynamic_bvh.pool.buf + shape->proxy;
                const struct bvh_QuerySet dynamic_query = BvhQueryAndFilterOnBody(tmp, &pipeline->dynamic_bvh, node);
                const struct bvh_QuerySet static_query = BvhQuery(tmp, &pipeline->static_bvh, node);
                
                query[si].dynamic_count = 0;
                query[si].dynamic_query = ArenaPushPacked(frame, dynamic_query.count*sizeof(u32));
                for (u32 qi = 0; qi < dynamic_query.count; ++qi)
                {
                    const u32 neighbour_si = dynamic_query.shape[qi];
                    const struct ds_ContactKey key = ds_ContactKeyCanonical(si, neighbour_si);
                    if (!ds_ContactKeyLookup(pipeline, key).address)
                    {
                        query[si].dynamic_query[ query[si].dynamic_count ] = neighbour_si;
                        query[si].dynamic_count += 1;
                    }
                }
                ArenaPopPacked(frame, (dynamic_query.count - query[si].dynamic_count)*sizeof(u32));

                query[si].static_count = 0;
                query[si].static_query = ArenaPushPacked(frame, static_query.count*sizeof(u32));
                for (u32 qi = 0; qi < static_query.count; ++qi)
                {
                    const u32 neighbour_si = static_query.shape[qi];
                    const struct ds_ContactKey key = ds_ContactKeyCanonical(si, neighbour_si);
                    if (!ds_ContactKeyLookup(pipeline, key).address)
                    {
                        query[si].static_query[ query[si].static_count ] = neighbour_si;
                        query[si].static_count += 1;
                    }
                }
                ArenaPopPacked(frame, (static_query.count - query[si].static_count)*sizeof(u32));

                ArenaPopRecord(tmp);
		    }
        }
    }

    ArenaPopScratch();

    ProfZoneEnd;

    return U32_MAX;
}

u32 ds_NarrowJobPhaseDispatch(const ds_JobId job)
{
    ProfZone;

    struct ds_NarrowJobPhase *phase = (struct ds_NarrowJobPhase *) g_scheduler->phase;
    struct ds_Dynamics *pipeline = phase->pipeline;
    struct arena *frame = pipeline->worker[ds_ThreadSelfIndex()].frame;
    struct ds_ParallelForChain *chain;
    struct ds_ParallelFor *pf;
    u32 low, high;

    for (u32 i = 0; i < CG_COLOR_COUNT; ++i)
    {
        ProfZoneNamed("Color");
        struct ds_CGraphColor *color = pipeline->cgraph.color + i;
        chain = phase->pf + i;
        {
            pf = chain->parallel_for + 0;
            ds_ParallelFor(pf, range_index)
            {
                ds_ParallelForRange(&low, &high, pf, range_index);
                for (u32 si = low; si < high; ++si)
                {
                    const u32 ci = color->contact_pool.buf[si];
                    ds_ShapeContact(frame, pipeline, ci);
                }
            }
        }
        ProfZoneEnd;
    }

    chain = phase->pf + CG_COLOR_COUNT;
    {
        ProfZoneNamed("Active");
        struct ds_SolverSet *active = pipeline->solver_set_pool.buf + SOLVER_SET_ACTIVE;
        pf = chain->parallel_for + 0;
        ds_ParallelFor(pf, range_index)
        {
            ds_ParallelForRange(&low, &high, pf, range_index);
            for (u32 si = low; si < high; ++si)
            {
                const u32 ci = active->contact_pool.buf[si];
                ds_ShapeContact(frame, pipeline, ci);
            }
        }
        ProfZoneEnd;
    }

    ProfZoneEnd;

    return U32_MAX;
}

static void CollisionDetection(struct ds_Dynamics *pipeline)
{
    /*
     * Achieving Determinism and Parallelization in the Broadphase
     * ===========================================================
     *
     * All moving or new shapes from the last frame are dirty and have their corresponding index
     * bit set in the shape_dirty_set. The broadphase can then be transformed into a parallel-for,
     * in which each thread process a range in the dirty bitset and queries its shapes against the
     * pipeline's bounding volume hierarchies. In order to to get duplicate collisions reported from
     * two moving shapes, or from shapes sharing the same body, we enforce a filter for the dynamic
     * bvh queries that looks something like:
     *
     *          overlap : (s0, s1)  => (shape[s0].body < shape[s1].body)
     *
     * When a DbvhRebuild is triggered due to a high number of re-insertions required in
     * the previous frame, all dynamic shapes are treated as dirty.
     * 
     * To achieve determinism, after a thread has queried a moving shape si, in stores the resulting 
     * query data in 
     *
     *      pipeline->shape_query.buf[si],
     *
     * The master thread can then traverse the results from low to high, adding the contact in a 
     * canonical way.
     */
    struct ds_BroadJobPhase *broad_phase = pipeline->broad_phase;
    {
    	ProfZoneNamed("JobPhase(Broadphase)");
        pipeline->profile->ns_broadphase_start = ds_TimeNs();

        ds_JobPhaseBegin(&broad_phase->phase);

        broad_phase->pf = ds_ParallelForChainAlloc(&pipeline->frame, 1); 
        
        //TODO: this range size is random hardcoded value, change
        ds_ParallelForInit(broad_phase->pf.parallel_for, pipeline->shape_dirty_set.block_count, 1);

        broad_phase->pipeline = pipeline;
        broad_phase->job_count = g_scheduler->worker_count;
        broad_phase->job = ArenaPushZero(&pipeline->frame, broad_phase->job_count*sizeof(struct ds_BroadJob));
        ds_JobPhaseReserve(&broad_phase->phase, BROAD_JOB_SEED, broad_phase->job_count);

        for (u32 i = 0; i < broad_phase->job_count; ++i)
        {
            ds_WSDequePushBottom(g_scheduler->seed_deque, ds_JobIdInit(BROAD_JOB_SEED, i));
        }

        AtomicStoreRlx32(&g_scheduler->a_seeds_remaining, broad_phase->job_count);
        ds_JobPhaseAddFetchRemaining(&broad_phase->phase, broad_phase->job_count);
        ds_WSDequePublish(g_scheduler->seed_deque);
        for (u32 i = 1; i < g_scheduler->worker_count; ++i)
        {
            SemaphorePost(&g_scheduler->jobs_are_available);
        }

	    ds_MasterRunAvailableJobs();
        
        ds_JobPhaseEnd();

        pipeline->profile->ns_broadphase_end = ds_TimeNs();
        ProfZoneEnd;
    }

    {
        /* Allocate new contacts and update query[q].dynamic/static_shape to contact index. */
        ProfZoneNamed("Contact Allocation");
        
        struct ds_BitSet *dirty = &pipeline->shape_dirty_set;
        for (u64 block = 0; block < dirty->block_count; ++block)
        {
            struct ds_BitBlock it = ds_BitBlockInit(dirty->bits[block], block);
		    while (ds_BitBlockHasNext(&it))
		    {
                const u32 si = ds_BitBlockNext(&it);
                const struct ds_ProxyQuery *query = pipeline->dirty_shape_query.buf + si;

                for (u32 q = 0; q < query->dynamic_count; ++q)
                {
                    const struct ds_ContactKey key = ds_ContactKeyCanonical(si, query->dynamic_query[q]);
                    ds_ContactAdd(pipeline, key);
                }

                for (u32 q = 0; q < query->static_count; ++q)
                {
                    const struct ds_ContactKey key = ds_ContactKeyCanonical(si, query->static_query[q]);
                    ds_ContactAdd(pipeline, key);
                }
		    }
        }

        ProfZoneEnd;
    }

    struct ds_NarrowJobPhase *narrow_phase = pipeline->narrow_phase;
    {
    	ProfZoneNamed("JobPhase(Narrowphase)");
        pipeline->profile->ns_narrowphase_start = ds_TimeNs();

        ds_JobPhaseBegin(&narrow_phase->phase);

        for (u32 i = 0; i < CG_COLOR_COUNT + 1; ++i)
        {
            narrow_phase->pf[i] = ds_ParallelForChainAlloc(&pipeline->frame, 1); 
        }

        struct ds_ParallelFor *pf;
        for (u32 i = 0; i < CG_COLOR_COUNT; ++i)
        {
            const struct ds_CGraphColor *color = pipeline->cgraph.color + i;
            pf = narrow_phase->pf[i].parallel_for;
            //TODO: this range size is random hardcoded value, change
            ds_ParallelForInit(pf + 0, color->contact_pool.count, 8);
        }

        pf = narrow_phase->pf[CG_COLOR_COUNT].parallel_for;
        //TODO: this range size is random hardcoded value, change
        ds_ParallelForInit(pf + 0, pipeline->solver_set_pool.buf[SOLVER_SET_ACTIVE].contact_pool.count, 8);

        narrow_phase->pipeline = pipeline;
        narrow_phase->job_count = g_scheduler->worker_count;
        narrow_phase->job = ArenaPushZero(&pipeline->frame, narrow_phase->job_count*sizeof(struct ds_NarrowJob));
        ds_JobPhaseReserve(&narrow_phase->phase, NARROW_JOB_SEED, narrow_phase->job_count);

        for (u32 i = 0; i < narrow_phase->job_count; ++i)
        {
            ds_WSDequePushBottom(g_scheduler->seed_deque, ds_JobIdInit(NARROW_JOB_SEED, i));
        }

        AtomicStoreRlx32(&g_scheduler->a_seeds_remaining, narrow_phase->job_count);
        ds_JobPhaseAddFetchRemaining(&narrow_phase->phase, narrow_phase->job_count);
        ds_WSDequePublish(g_scheduler->seed_deque);
        for (u32 i = 1; i < g_scheduler->worker_count; ++i)
        {
            SemaphorePost(&g_scheduler->jobs_are_available);
        }

	    ds_MasterRunAvailableJobs();
        
        ds_JobPhaseEnd();

        pipeline->profile->ns_narrowphase_end = ds_TimeNs();
    	ProfZoneEnd;
    }

    {
    	ProfZoneNamed("Contact Promotion/Demotion");
        {
            const struct ds_SolverSet *active = pipeline->solver_set_pool.buf + SOLVER_SET_ACTIVE;
            u32 compute = 0; 
            while (compute < active->contact_pool.count)
            {
                const u32 ci = active->contact_pool.buf[compute];
                const struct ds_Contact *c = pipeline->contact_pool.buf + ci;
                if (c->narrowphase.manifold_count > 0)
                {
                    ds_ContactPromote(pipeline, ci);
                }
                else
                {
                    compute += 1;
                }
            }
        }

        for (u32 color_index = 0; color_index < CG_COLOR_COUNT; ++color_index)
        {
            const struct ds_CGraphColor *color = pipeline->cgraph.color + color_index;
            u32 compute = 0;
            while (compute < color->contact_pool.count)
            {
                const u32 ci = color->contact_pool.buf[compute];
                const struct ds_Contact *c = pipeline->contact_pool.buf + ci;
                if (c->narrowphase.manifold_count == 0)
                {
                    ds_ContactDemote(pipeline, ci);
                }
                else
                {
                    compute += 1;
                }
            }
        }
    	ProfZoneEnd;
    }

    {
        if (pipeline->island_to_split != DS_ID_NULL)
        {
    	    ProfZoneNamed("Island Split");
    
            const u32 split_index = ds_IdIndex(pipeline->island_to_split);
            if (ds_PoolSlotAllocated(pipeline->island_pool.buf + split_index) 
                    && pipeline->island_to_split == pipeline->island_pool.buf[split_index].id)
            {
                ds_IslandSplit(pipeline, split_index);
            }
    
    	    ProfZoneEnd;
        }
    }
}

u32 ds_SolverJobPhaseDispatch(const ds_JobId job)
{
    ProfZone;

    struct ds_SolverJobPhase *phase = (struct ds_SolverJobPhase *) g_scheduler->phase;
    struct ds_Dynamics *pipeline = phase->pipeline;
    struct ds_ParallelForChain *chain;
    struct ds_ParallelFor *pf;
    u32 low, high;

    chain = &phase->pf_body_update;
    {
        ProfZoneNamed("Body Update");
        pf = chain->parallel_for + 0;
        ds_ParallelFor(pf, range_index)
        {
            ds_ParallelForRange(&low, &high, pf, range_index);
            ds_BodyUpdateSolverDataRange(pipeline, low, high);

        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    chain = &phase->pf_contact_init;
    {
        ProfZoneNamed("Constraint Initialization");
        for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
        {
            pf = chain->parallel_for + ci;
            ds_ParallelFor(pf, range_index)
            {
                ds_ParallelForRange(&low, &high, pf, range_index);
                ds_ContactConstraintInitRange(pipeline, ci, low, high);
            }
        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    if (g_solver_config->warmup_solver)
    {
        chain = &phase->pf_contact_warmup;
        {
            ProfZoneNamed("Constraint Warmup");
            for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
            {
                pf = chain->parallel_for + ci;
                ds_ParallelFor(pf, range_index)
                {
                    ds_ParallelForRange(&low, &high, pf, range_index);
                    ds_ContactConstraintWarmupRange(pipeline, ci, low, high);
                }
            }
            ProfZoneEnd;
        }
        ds_ParallelForChainWait(chain);
    }

    chain = &phase->pf_velocity_solve;
    {
        ProfZoneNamed("Velocity Solve")
        for (u32 i = 0; i < g_solver_config->pgs_iteration_count; ++i)
        {
            ProfZoneNamed("Iteration");
            for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
            {
                ProfZoneNamed("Color");

                struct ds_CGraphColor *color = pipeline->cgraph.color + ci;
                const u32 pfi = i*CG_COLOR_COUNT + ci;
                pf = chain->parallel_for + pfi;
                ds_ParallelFor(pf, range_index)
                {
                    ds_ParallelForRange(&low, &high, pf, range_index);
                    ds_ContactConstraintIterateRange(pipeline, ci, low, high); 
                }
                ProfZoneEnd;
            }
            ProfZoneEnd;
        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    chain = &phase->pf_integrate;
    {
        ProfZoneNamed("Velocity Integrate");
        pf = chain->parallel_for + 0;
        ds_ParallelFor(pf, range_index)
        {
            ds_ParallelForRange(&low, &high, pf, range_index);
            ds_BodyIntegrateVelocitiesRange(pipeline, low, high);

        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    chain = &phase->pf_cache_impulse_and_position_init;
    {
        ProfZoneNamed("Cache Impulses and Initalize Position Constraints");
        for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
        {
            pf = chain->parallel_for + ci;
            ds_ParallelFor(pf, range_index)
            {
                ds_ParallelForRange(&low, &high, pf, range_index);
                ds_PositionConstraintInitAndCacheImpulsesRange(pipeline, ci, low, high);
            }
        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    chain = &phase->pf_position_solve;
    {
        ProfZoneNamed("Position Solve")
        for (u32 i = 0; i < g_solver_config->ngs_iteration_count; ++i)
        {
            ProfZoneNamed("Iteration");
            for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
            {
                ProfZoneNamed("Color");

                struct ds_CGraphColor *color = pipeline->cgraph.color + ci;
                const u32 pfi = i*CG_COLOR_COUNT + ci;
                pf = chain->parallel_for + pfi;
                ds_ParallelFor(pf, range_index)
                {
                    ds_ParallelForRange(&low, &high, pf, range_index);
                    ds_PositionConstraintIterateRange(pipeline, ci, low, high); 
                }
                ProfZoneEnd;
            }
            ProfZoneEnd;
        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    chain = &phase->pf_orientation;
    {
        ProfZoneNamed("Position Update");
        pf = chain->parallel_for + 0;
        ds_ParallelFor(pf, range_index)
        {
            struct ds_ProxyRange *proxy_range = phase->proxy_range + range_index;
            ds_ParallelForRange(&low, &high, pf, range_index);
            ds_BodyUpdateOrientationRange(pipeline, proxy_range, low, high);
        }
        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    ProfZoneEnd;

    return U32_MAX;
}

static void ds_RebuildProduceThinWork(struct ds_RebuildJobPhase *phase, const u32 node_index, const u32 base, const u32 count, const u32 depth, const u32 axis, const f32 pivot)
{
    ds_Assert(count <= phase->small_range_leaf_limit);
    
    const u32 wi = phase->a_thin_range_count;
    phase->a_thin_range_count += 1;
    if (wi >= phase->thin_range_max_count)
    {
        LogString(T_PHYSICS, S_FATAL, "RebuildPhase: thin_range_max_count reached, Exiting.");
        FatalCleanupAndExit();
    }

    struct ds_RebuildThinRange *new_range = phase->thin_range + wi;
    new_range->low = base;
    new_range->high = base + count;
    new_range->internal_index = node_index;
    new_range->axis = axis;
    new_range->pivot = pivot;
    new_range->depth = depth;
}

static void ds_RebuildProduceFatWork(struct arena *mem, struct ds_RebuildJobPhase *phase, const u32 node_index, const u32 base, const u32 count, const u32 depth, const u32 axis, const f32 pivot)
{
    const u32 wi = phase->a_fat_range_counter;
    phase->a_fat_range_counter += 1;
    if (wi >= phase->fat_range_max_count)
    {
        LogString(T_PHYSICS, S_FATAL, "RebuildPhase: fat_range_max_count reached, Exiting.");
        FatalCleanupAndExit();
    }

    struct ds_RebuildFatRange *new_range = phase->fat_range + wi;
    new_range->low = base;
    new_range->high = base + count;
    new_range->depth = depth;
    new_range->internal_index = node_index;
    new_range->axis = axis;
    new_range->pivot = pivot;
    new_range->a_low_count = 0;
    new_range->a_high_count = 0;
    new_range->pf = ds_ParallelForChainAlloc(mem, 1);

    const u32 new_count = new_range->high - new_range->low;
    ds_ParallelForInit(new_range->pf.parallel_for, new_count, phase->small_range_leaf_limit);
}

static void ds_RebuildLeafSetMinMax(vec3 min, vec3 max, const struct ds_RebuildLeaf *base, const u32 count)
{
	Vec3Set(min, F32_INFINITY, F32_INFINITY, F32_INFINITY);
	Vec3Set(max, -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
	for (u32 i = 0; i < count; ++i)
	{
        Vec3MinSelf(min, base[i].center);
        Vec3MaxSelf(max, base[i].center);
	}
}

static void ds_RebuildAxisPivot(u32 axis[2], f32 pivot[2], const vec3 min[2], const vec3 max[2])
{
    for (u32 i = 0; i < 2; ++i)
    {
        vec3 diff;
        Vec3Sub(diff, max[i], min[i]);
        axis[i] = 0;
        for (u32 a = 1; a < 3; ++a)
        {
            if (diff[axis[i]] < diff[a])
            {
                axis[i] = a;
            }
        }
        pivot[i] = min[i][axis[i]] + diff[axis[i]] / 2.0f;
    }
}

static void ds_RebuildGather(u32 axis[2], f32 pivot[2], const struct ds_RebuildJobPhase *phase)
{
    vec3 min[2] = 
    {
        { F32_INFINITY, F32_INFINITY, F32_INFINITY },
        { F32_INFINITY, F32_INFINITY, F32_INFINITY },
    };

    vec3 max[2] = 
    {
        { -F32_INFINITY, -F32_INFINITY, -F32_INFINITY },
        { -F32_INFINITY, -F32_INFINITY, -F32_INFINITY },
    };

    for (u32 j = 0; j < phase->job_count; ++j)
    {
        struct ds_RebuildJob *job = phase->job + j;
        Vec3MinSelf(min[0], job->min[0]);
        Vec3MaxSelf(max[0], job->max[0]);
        Vec3MinSelf(min[1], job->min[1]);
        Vec3MaxSelf(max[1], job->max[1]);
    }

    ds_RebuildAxisPivot(axis, pivot, min, max);
}

static void ds_RebuildProduceWork(struct arena *phase_mem, struct ds_RebuildJobPhase *phase, const u32 work_index)
{
    const struct ds_Dynamics *pipeline = phase->pipeline;
    const struct ds_RebuildFatRange *range = phase->fat_range + work_index;

    const u32 ri = (range->depth & 0x1);
    const struct ds_RebuildLeaf *leaf_write = phase->leaf_buf[1-ri];
    struct bvhNode *node_buf = pipeline->dynamic_bvh.pool.buf;
    
    u32 count[2] = 
    { 
        range->a_low_count, 
        range->a_high_count,
    };

    f32 pivot[2];
    u32 axis[2];
    if (count[0] == 0 || count[1] == 0)
    {
        vec3 min[2], max[2];
        count[0] = (range->high - range->low) / 2;
        count[1] = range->high - range->low - count[0];
        ds_RebuildLeafSetMinMax(min[0], max[0], leaf_write + range->low, count[0]);
        ds_RebuildLeafSetMinMax(min[1], max[1], leaf_write + range->low + count[0], count[1]);
        ds_RebuildAxisPivot(axis, pivot, min, max);
    }
    else
    {
        ds_RebuildGather(axis, pivot, phase);
    }

    const u32 base[2] = { range->low, range->low + count[0] };

    for (u32 i = 0; i < 2; ++i)
    {        
        const u32 child_index = (count[i] == 1)
                                ? leaf_write[ base[i] ].index
                                : AtomicFetchAddRlx32(&phase->a_internal_counter, 1);

        struct bvhNode *parent = node_buf + range->internal_index;
        struct bvhNode *child = node_buf + child_index;

        parent->bt_child[i] = child_index;
        child->bt_parent = range->internal_index;

        if (count[i] == 1)
        {
            child->bt_parent |= BT_LEAF_MASK;
            continue;
        }

        if (count[i] <= phase->small_range_leaf_limit)
        {
            ds_RebuildProduceThinWork(phase, child_index, base[i], count[i], range->depth+1, axis[i], pivot[i]);
        }
        else
        {
            ds_RebuildProduceFatWork(phase_mem, phase, child_index, base[i], count[i], range->depth+1, axis[i], pivot[i]);
        }
    }
}

u32 ds_RebuildJobPhaseDispatch(const ds_JobId job_id)
{
    ProfZone;

    struct arena *phase_mem = ArenaPushScratch();
    struct ds_RebuildJobPhase *phase = (struct ds_RebuildJobPhase *) g_scheduler->phase;
    struct ds_Dynamics *pipeline = phase->pipeline;
    const struct ds_BitSet *leaf_usage = &pipeline->dynamic_bvh.leaf_set;
    struct ds_ParallelForChain *chain;
    struct ds_ParallelFor *pf;
    struct ds_RebuildJob *job = phase->job + ds_JobIdIndex(job_id);
    u32 low, high, tmp_iteration;
    u32 local_iteration = 0;
    u32 local_completed = 0;

    ds_Assert(pipeline->dynamic_bvh.pool.count > 1);
    ds_Assert(phase->small_range_leaf_limit == 64*phase->leaf_blocks_per_work);

    chain = &phase->pf_proxy_update;
    {
        ProfZoneNamed("Proxy Update");

        Vec3Set(job->min[0], F32_INFINITY, F32_INFINITY, F32_INFINITY);
        Vec3Set(job->max[0], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
        Vec3Set(job->min[1], F32_INFINITY, F32_INFINITY, F32_INFINITY);
        Vec3Set(job->max[1], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
        pf = chain->parallel_for + 0;
        ds_ParallelFor(pf, range_index)
        {
            ds_ParallelForRange(&low, &high, pf, range_index);
            for (u32 block = low; block < high; ++block)
            {
                job->count[0] = 0;
                job->count[1] = 0;

                struct ds_BitBlock it = ds_BitBlockInit(leaf_usage->bits[block], block);
                while (ds_BitBlockHasNext(&it))
                {
                    const u32 pi = ds_BitBlockNext(&it);
                    struct bvhNode *node = pipeline->dynamic_bvh.pool.buf + pi;
                    const struct ds_Shape *shape = pipeline->shape_pool.buf + node->bt_child[0];
                    node->bbox = ds_ShapeWorldBbox(pipeline, shape);
                    node->bbox.hw[0] += shape->margin;
                    node->bbox.hw[1] += shape->margin;
                    node->bbox.hw[2] += shape->margin;

                    Vec3MinSelf(job->min[0], node->bbox.center);
                    Vec3MaxSelf(job->max[0], node->bbox.center);

                    Vec3Copy(job->leaf[0][ job->count[0] ].center, node->bbox.center);
                    job->leaf[0][ job->count[0] ].index = pi;
                    job->count[0] += 1;
                }

                if (job->count[0])
                {
                    const u32 base = AtomicFetchAddRlx32(&phase->a_next[0], job->count[0]);
                    memcpy(phase->leaf_buf[0] + base, job->leaf[0], job->count[0]*sizeof(struct ds_RebuildLeaf));
                }
            }
        }

        ProfZoneEnd;
    }
    ds_ParallelForChainWait(chain);

    /* 
     * One thread gets to finalize the setup, and initialize the first thin or fat work.
     */
    u32 lock = 0;
    if (AtomicCompareExchangeRlxRlx32(&phase->a_setup_completed, &lock, U32_MAX))
    {
        u32 axis[2];
        f32 pivot[2];
        ds_RebuildGather(axis, pivot, phase);
        const u32 node_index = AtomicFetchAddRlx32(&phase->a_internal_counter, 1);

        if (phase->leaf_count <= phase->small_range_leaf_limit)
        {
            ds_RebuildProduceThinWork(phase, node_index, 0, phase->leaf_count, 0, axis[0], pivot[0]);
        }
        else
        {
            ds_RebuildProduceFatWork(phase_mem, phase, node_index, 0, phase->leaf_count, 0, axis[0], pivot[0]);
        }
        
        AtomicStoreRel32(&phase->a_setup_completed, 1);
    }

    ds_Spin(AtomicLoadAcq32(&phase->a_setup_completed) != 1, 32, U32_MAX);

    /*
     * the Work queue is a FIFO queue, which ensures us that we will work on ranges in increasing depth order.
     * This allows us to work with only two leaf buffers. Depending on the depth, one is viewed as read-only,
     * while the other is write-only:
     *
     *          const u32 d = range->depth;
     *          const ds_RebuildLeaf *read_buf = phase->leaf_buffer + (d & 0x1);
     *                ds_RebuildLeaf *write_buf = phase->leaf_buffer + (1 - (d & 0x1));
     */
    ds_Assert(PowerOfTwoCheck(phase->fat_range_max_count));
    const u32 mask = phase->fat_range_max_count - 1;
    while (1)
    {
        u32 local_completed = AtomicLoadAcq32(&phase->a_fat_range_completed);
        u32 local_counter = AtomicLoadAcq32(&phase->a_fat_range_counter);
        if (local_counter == local_completed)
        {
            break;
        }

        local_iteration = local_completed;
        for (u32 i = local_completed; i < local_counter; ++i)
        {
            //TODO Sync once per depth? 
            
            const u32 wi = i & mask;
            struct ds_RebuildFatRange *range = phase->fat_range + wi;

            const u32 ri = (range->depth & 0x1);
            const struct ds_RebuildLeaf *leaf_read = phase->leaf_buf[ri];
            struct ds_RebuildLeaf *leaf_write = phase->leaf_buf[1-ri];

            Vec3Set(job->min[0], F32_INFINITY, F32_INFINITY, F32_INFINITY);
            Vec3Set(job->max[0], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
            Vec3Set(job->min[1], F32_INFINITY, F32_INFINITY, F32_INFINITY);
            Vec3Set(job->max[1], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
            chain = &range->pf;
            {
                pf = chain->parallel_for + 0;
                ds_ParallelFor(pf, range_index)
                {
                    ProfZoneNamed("FatRangeBlock");
                    ds_ParallelForRange(&low, &high, pf, range_index);
                    low += range->low;
                    high += range->low;

                    job->count[0] = 0;
                    job->count[1] = 0;
                    for (u32 li = low; li < high; ++li)
                    {
                        const struct ds_RebuildLeaf *leaf = leaf_read + li;
                        struct bvhNode *node = pipeline->dynamic_bvh.pool.buf + leaf->index;

                        const u32 si = (node->bbox.center[range->axis] > range->pivot);
                        Vec3MinSelf(job->min[si], node->bbox.center);
                        Vec3MaxSelf(job->max[si], node->bbox.center);
                        
                        const u32 li = job->count[si];
                        Vec3Copy(job->leaf[si][li].center, node->bbox.center);
                        job->leaf[si][li].index = leaf->index;
                        job->count[si] += 1;
                    }

                    if (job->count[0])
                    {
                        const u32 lbase = range->low + AtomicFetchAddRlx32(&range->a_low_count, job->count[0]);
                        memcpy(leaf_write + lbase, job->leaf[0], job->count[0]*sizeof(struct ds_RebuildLeaf));
                    }

                    if (job->count[1])
                    {
                        const u32 hbase = range->high - AtomicAddFetchRlx32(&range->a_high_count, job->count[1]);
                        memcpy(leaf_write + hbase, job->leaf[1], job->count[1]*sizeof(struct ds_RebuildLeaf));
                    }
                }

                    ProfZoneEnd;
            }

            ds_ParallelForChainWait(chain);

            /* Winner updates dynamic tree and setup new work. */
            tmp_iteration = local_iteration;
            local_iteration += 1;
            if (AtomicCompareExchangeRlxRlx32(&phase->a_fat_range_iteration, &tmp_iteration, local_iteration))
            {
                ds_RebuildProduceWork(phase_mem, phase, wi);
                AtomicFetchAddRel32(&phase->a_fat_range_completed, 1);
            }

            ds_Spin((local_completed = AtomicLoadAcq32(&phase->a_fat_range_completed)) < local_iteration, 32, U32_MAX);
        }
    }

    struct ds_RebuildThinRange *range_buf = ArenaPush(phase_mem, phase->small_range_leaf_limit*sizeof(struct ds_RebuildThinRange));
    u32 work_count = 0;

    const u32 thin_count = AtomicLoadRlx32(&phase->a_thin_range_count);
    u32 ti = AtomicFetchAddRlx32(&phase->a_thin_range_next, 1);
    while ((ti = AtomicFetchAddRlx32(&phase->a_thin_range_next, 1)) < thin_count)
    {
        ProfZoneNamed("ThinRange");

        range_buf[work_count] = phase->thin_range[ti];
        work_count = 1;

        while (work_count--)
        {
            const struct ds_RebuildThinRange *range = range_buf + work_count;
            const u32 ri = (range->depth & 0x1);
            struct ds_RebuildLeaf *leaf = phase->leaf_buf[ri];
            u32 low = range->low;
            u32 high = range->high;

            job->count[0] = 0;
            job->count[1] = 0;
            Vec3Set(job->min[0], F32_INFINITY, F32_INFINITY, F32_INFINITY);
            Vec3Set(job->max[0], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);
            Vec3Set(job->min[1], F32_INFINITY, F32_INFINITY, F32_INFINITY);
            Vec3Set(job->max[1], -F32_INFINITY, -F32_INFINITY, -F32_INFINITY);

            while (low < high)
            {
                while (low < high)
                {
                    if (leaf[low].center[range->axis] >= range->pivot)
                    {
                        break;
                    }

                    Vec3MinSelf(job->min[0], leaf[low].center);
                    Vec3MaxSelf(job->max[0], leaf[low].center);
                    low += 1;
                }

                while (low < high)
                {
                    if (leaf[high-1].center[range->axis] < range->pivot)
                    {
                        const struct ds_RebuildLeaf tmp = leaf[low];
                        leaf[low] = leaf[high-1];
                        leaf[high-1] = tmp;
                        Vec3MinSelf(job->min[0], leaf[low].center);
                        Vec3MaxSelf(job->max[0], leaf[low].center);
                        Vec3MinSelf(job->min[1], leaf[high-1].center);
                        Vec3MaxSelf(job->max[1], leaf[high-1].center);
                        low += 1;
                        high -= 1;
                        break;
                    }

                    Vec3MinSelf(job->min[1], leaf[high-1].center);
                    Vec3MaxSelf(job->max[1], leaf[high-1].center);
                    high -= 1;
                }
            }
           
            if (job->count[0] == 0 || job->count[1] == 0)
            {
                job->count[0] = (range->high - range->low) / 2;
                job->count[1] = range->high - range->low - job->count[0];
                ds_RebuildLeafSetMinMax(job->min[0], job->max[0], leaf + range->low, job->count[0]);
                ds_RebuildLeafSetMinMax(job->min[1], job->max[1], leaf + range->low + job->count[0], job->count[1]);
            }
 
            const u32 parent = range->internal_index;
            const u32 base[2] =
            {
                range->low,
                range->low + job->count[0],
            };

            u32 axis[2];
            f32 pivot[2];
            ds_RebuildAxisPivot(axis, pivot, job->min, job->max);
            
            for (u32 s = 0; s < 2; ++s)
            {
                if (job->count[s] <= 1)
                {
                    const u32 index = leaf[base[s]].index;
                    pipeline->dynamic_bvh.pool.buf[parent].bt_child[s] = index;
                    pipeline->dynamic_bvh.pool.buf[index].bt_parent = BT_LEAF_MASK | parent;
                }
                else
                {
                    ds_Assert(work_count < phase->small_range_leaf_limit);
                    const u32 index = AtomicFetchAddRlx32(&phase->a_internal_counter, 1);
                    range_buf[ work_count ].low = base[s];
                    range_buf[ work_count ].high = base[s] + job->count[s];
                    range_buf[ work_count ].axis = axis[s];
                    range_buf[ work_count ].pivot = pivot[s];
                    range_buf[ work_count ].depth = range->depth + 1;
                    range_buf[ work_count ].internal_index = index;
                    work_count += 1;
                    pipeline->dynamic_bvh.pool.buf[parent].bt_child[s] = index;
                    pipeline->dynamic_bvh.pool.buf[index].bt_parent = parent;
                }
            }
        }

        ProfZoneEnd;
    }
    

    ArenaPopScratch();

    ProfZoneEnd;

    return U32_MAX;
}

static void SolveConstraints(struct ds_Dynamics *pipeline) 
{
    struct ds_SolverJobPhase *solver_phase = pipeline->solver_phase;
    {
    	ProfZoneNamed("JobPhase(Solve)");
        pipeline->profile->ns_solverphase_start = ds_TimeNs();

        ds_JobPhaseBegin(&solver_phase->phase);

        solver_phase->pf_body_update = ds_ParallelForChainAlloc(&pipeline->frame, 1);
        solver_phase->pf_contact_init = ds_ParallelForChainAlloc(&pipeline->frame, CG_COLOR_COUNT);
        solver_phase->pf_contact_warmup = ds_ParallelForChainAlloc(&pipeline->frame, CG_COLOR_COUNT);
        solver_phase->pf_velocity_solve = ds_ParallelForChainAlloc(&pipeline->frame, g_solver_config->pgs_iteration_count*CG_COLOR_COUNT);
        solver_phase->pf_integrate = ds_ParallelForChainAlloc(&pipeline->frame, 1);
        solver_phase->pf_cache_impulse_and_position_init = ds_ParallelForChainAlloc(&pipeline->frame, CG_COLOR_COUNT);
        solver_phase->pf_position_solve = ds_ParallelForChainAlloc(&pipeline->frame, g_solver_config->ngs_iteration_count*CG_COLOR_COUNT);
        solver_phase->pf_orientation = ds_ParallelForChainAlloc(&pipeline->frame, 1);

        struct ds_ParallelFor *pfb = solver_phase->pf_body_update.parallel_for;
        struct ds_ParallelFor *pfin = solver_phase->pf_integrate.parallel_for;
        struct ds_ParallelFor *pfo = solver_phase->pf_orientation.parallel_for;
        //TODO: this range size is random hardcoded value, change
        ds_ParallelForInit(pfb + 0, pipeline->solver_set_pool.buf[SOLVER_SET_ACTIVE].body_compute_pool.count, 32);
        ds_ParallelForInit(pfin + 0, pipeline->solver_set_pool.buf[SOLVER_SET_ACTIVE].body_compute_pool.count, 32);
        ds_ParallelForInit(pfo + 0, pipeline->solver_set_pool.buf[SOLVER_SET_ACTIVE].body_compute_pool.count, 32);
        solver_phase->proxy_range = ArenaPush(&pipeline->frame, pfo->range_count*sizeof(struct ds_ProxyRange));

        struct ds_ParallelFor *pfci = solver_phase->pf_contact_init.parallel_for;
        struct ds_ParallelFor *pfcw = solver_phase->pf_contact_warmup.parallel_for;
        struct ds_ParallelFor *pfvs = solver_phase->pf_velocity_solve.parallel_for;
        struct ds_ParallelFor *pfcp = solver_phase->pf_cache_impulse_and_position_init.parallel_for;
        struct ds_ParallelFor *pfps = solver_phase->pf_position_solve.parallel_for;
        for (u32 ci = 0; ci < CG_COLOR_COUNT; ++ci)
        {
            struct ds_CGraphColor *color = pipeline->cgraph.color + ci;
            const u32 cc_count = color->contact_pool.count;
            //TODO: this range size is random hardcoded value, change
            u32 cc_per_range = (ci == CG_SERIAL_COLOR)
                ? cc_count + 1
                : 8;

            ds_ParallelForInit(pfci + ci, cc_count, cc_per_range);
            ds_ParallelForInit(pfcw + ci, cc_count, cc_per_range);
            ds_ParallelForInit(pfcp + ci, cc_count, cc_per_range);

            //const f32 d = (f32) cc_count / (cc_per_range * g_scheduler->worker_count);
            //if (ci == 0)
            //  fprintf(stderr, "range distribution: \n {");

            //(ci+1 < CG_COLOR_COUNT)
            //    ? fprintf(stderr, " %f,", d)
            //    : fprintf(stderr, " %f }\n", d);

            for (u32 pgsi = 0; pgsi < g_solver_config->pgs_iteration_count; ++pgsi)
            {
                const u32 pfi = pgsi*CG_COLOR_COUNT + ci;
                ds_ParallelForInit(pfvs + pfi, cc_count, cc_per_range);
            }

            for (u32 pgsi = 0; pgsi < g_solver_config->ngs_iteration_count; ++pgsi)
            {
                const u32 pfi = pgsi*CG_COLOR_COUNT + ci;
                ds_ParallelForInit(pfps + pfi, cc_count, cc_per_range);
            }
        }

        solver_phase->pipeline = pipeline;
        solver_phase->job_count = g_scheduler->worker_count;
        solver_phase->job = ArenaPushZero(&pipeline->frame, solver_phase->job_count*sizeof(struct ds_SolverJob));
        ds_JobPhaseReserve(&solver_phase->phase, SOLVER_JOB_SEED, solver_phase->job_count);

        for (u32 i = 0; i < solver_phase->job_count; ++i)
        {
            ds_WSDequePushBottom(g_scheduler->seed_deque, ds_JobIdInit(SOLVER_JOB_SEED, i));
        }

        AtomicStoreRlx32(&g_scheduler->a_seeds_remaining, solver_phase->job_count);
        ds_JobPhaseAddFetchRemaining(&solver_phase->phase, solver_phase->job_count);
        ds_WSDequePublish(g_scheduler->seed_deque);
        for (u32 i = 1; i < g_scheduler->worker_count; ++i)
        {
            SemaphorePost(&g_scheduler->jobs_are_available);
        }

	    ds_MasterRunAvailableJobs();
        
        ds_JobPhaseEnd();

        pipeline->profile->ns_solverphase_end = ds_TimeNs();
    	ProfZoneEnd;
    }

    {
        struct arena *tmp = ArenaPushScratch();

        struct ds_ParallelFor *pf = solver_phase->pf_orientation.parallel_for + 0;
        f32 dirty_count = 0; 
        f32 reinsert_count = 0;
        for (u32 ri = 0; ri < pf->range_count; ++ri)
        {
            const struct ds_ProxyRange *range = solver_phase->proxy_range + ri;
            dirty_count += range->count;
            reinsert_count += range->reinsert_count;
        }

        ds_Assert(dirty_count >= reinsert_count);
        if (dirty_count)
        {
            pipeline->profile->ns_rebuildphase_start = ds_TimeNs();

            const f32 reinsert_fraction = reinsert_count / dirty_count;
            if (reinsert_fraction > g_numerics_config->dbvh_reinsert_threshold)
            {
                const struct ds_BitSet *usage = &pipeline->shape_dynamic_usage_set;
                for (u64 block = 0; block < usage->block_count; ++block)
                {
                    pipeline->shape_dirty_set.bits[block] = usage->bits[block];
                }
                
                struct ds_RebuildJobPhase *rebuild_phase = pipeline->rebuild_phase;
                {
                	ProfZoneNamed("JobPhase(Rebuild)");

                    ds_JobPhaseBegin(&rebuild_phase->phase);

                    rebuild_phase->pf_proxy_update = ds_ParallelForChainAlloc(&pipeline->frame, 1); 
                    
                    //TODO: this range size is random hardcoded value, change
                    rebuild_phase->leaf_blocks_per_work = 2;
                    rebuild_phase->small_range_leaf_limit = 64*rebuild_phase->leaf_blocks_per_work;
                    ds_ParallelForInit(rebuild_phase->pf_proxy_update.parallel_for, pipeline->dynamic_bvh.leaf_set.block_count, rebuild_phase->leaf_blocks_per_work);

                    rebuild_phase->pipeline = pipeline;
                    rebuild_phase->job_count = g_scheduler->worker_count;
                    rebuild_phase->job = ArenaPushZero(&pipeline->frame, rebuild_phase->job_count*sizeof(struct ds_RebuildJob));
                    rebuild_phase->leaf_count = ds_BTLeafCount(pipeline->dynamic_bvh.bt);
                    rebuild_phase->leaf_buf[0] = ArenaPushAligned(&pipeline->frame, rebuild_phase->leaf_count*sizeof(struct ds_RebuildLeaf), DS_CACHE_LINE);
                    rebuild_phase->leaf_buf[1] = ArenaPushAligned(&pipeline->frame, rebuild_phase->leaf_count*sizeof(struct ds_RebuildLeaf), DS_CACHE_LINE);
                    rebuild_phase->internal_count = pipeline->dynamic_bvh.bt.count - rebuild_phase->leaf_count;
                    rebuild_phase->internal_buf = ArenaPushAligned(&pipeline->frame, rebuild_phase->internal_count*sizeof(u32), DS_CACHE_LINE);
                    rebuild_phase->fat_range_max_count = 512;
                    rebuild_phase->fat_range = ArenaPush(&pipeline->frame, rebuild_phase->fat_range_max_count*sizeof(struct ds_RebuildFatRange));
                    AtomicStoreRel32(&rebuild_phase->a_fat_range_counter, 0);
                    AtomicStoreRel32(&rebuild_phase->a_fat_range_completed, 0);
                    AtomicStoreRel32(&rebuild_phase->a_fat_range_iteration, 0);
                    AtomicStoreRel32(rebuild_phase->a_next + 1, 0);
                    AtomicStoreRel32(rebuild_phase->a_next + 0, 0);
                    AtomicStoreRel32(&rebuild_phase->a_setup_completed, 0);

                    struct memArray thin_arr = ArenaPushAlignedAll(tmp, sizeof(struct ds_RebuildThinRange), 8);
                    rebuild_phase->thin_range = thin_arr.addr;
                    rebuild_phase->thin_range_max_count = thin_arr.len;
                    AtomicStoreRel32(&rebuild_phase->a_thin_range_count, 0);
                    AtomicStoreRel32(&rebuild_phase->a_thin_range_next, 0);

                    for (u32 i = 0; i < rebuild_phase->job_count; ++i)
                    {
                        rebuild_phase->job[i].leaf[0] = ArenaPushAligned(&pipeline->frame, rebuild_phase->small_range_leaf_limit*sizeof(struct ds_RebuildLeaf), DS_CACHE_LINE);
                        rebuild_phase->job[i].leaf[1] = ArenaPushAligned(&pipeline->frame, rebuild_phase->small_range_leaf_limit*sizeof(struct ds_RebuildLeaf), DS_CACHE_LINE);
                    }

                    ds_JobPhaseReserve(&rebuild_phase->phase, REBUILD_JOB_SEED, rebuild_phase->job_count);

                    for (u32 i = 0; i < rebuild_phase->job_count; ++i)
                    {
                        ds_WSDequePushBottom(g_scheduler->seed_deque, ds_JobIdInit(REBUILD_JOB_SEED, i));
                    }

                    AtomicStoreRlx32(&g_scheduler->a_seeds_remaining, rebuild_phase->job_count);
                    ds_JobPhaseAddFetchRemaining(&rebuild_phase->phase, rebuild_phase->job_count);
                    ds_WSDequePublish(g_scheduler->seed_deque);
                    for (u32 i = 1; i < g_scheduler->worker_count; ++i)
                    {
                        SemaphorePost(&g_scheduler->jobs_are_available);
                    }

	                ds_MasterRunAvailableJobs();
                    
                    ds_JobPhaseEnd();

                    ProfZoneEnd;
                }

                for (u32 wi = 0; wi < rebuild_phase->a_thin_range_count; ++wi)
                {
                    fprintf(stderr, "[%u, %u](%u)\n"
                            , rebuild_phase->thin_range[wi].low
                            , rebuild_phase->thin_range[wi].high
                            , rebuild_phase->thin_range[wi].high - rebuild_phase->thin_range[wi].low
                            );
                }

                //{
                //    ProfZoneNamed("DBVH Rebuild");
                //    DbvhRebuild(&pipeline->dynamic_bvh);
                //    ProfZoneEnd;
                //}

                //{
                //    ProfZoneNamed("Dirtying Bboxes");
                //    const struct ds_BitSet *usage = &pipeline->shape_dynamic_usage_set;
                //    for (u64 block = 0; block < usage->block_count; ++block)
                //    {
                //        pipeline->shape_dirty_set.bits[block] = usage->bits[block];
                //        struct ds_BitBlock it = ds_BitBlockInit(usage->bits[block], block);
                //        while (ds_BitBlockHasNext(&it))
                //        {
                //            const u32 si = ds_BitBlockNext(&it);
                //            const struct ds_Shape *shape = pipeline->shape_pool.buf + si;
                //            const struct ds_Body *body = pipeline->body_pool.buf + shape->body;
                //            if (RB_IS_DYNAMIC(body))
                //            {
                //                pipeline->dynamic_bvh.pool.buf[ shape->proxy ].bbox = ds_ShapeWorldBbox(pipeline, shape);
                //                pipeline->dynamic_bvh.pool.buf[ shape->proxy ].bbox.hw[0] += shape->margin;
                //                pipeline->dynamic_bvh.pool.buf[ shape->proxy ].bbox.hw[1] += shape->margin;
                //                pipeline->dynamic_bvh.pool.buf[ shape->proxy ].bbox.hw[2] += shape->margin;
                //            }
                //        }
                //    }
                //                
                //    ProfZoneEnd;
                //}

                //{
                //    ProfZoneNamed("DBVH Rebuild");
                //    DbvhRebuild(&pipeline->dynamic_bvh);
                //    ProfZoneEnd;
                //}
            }
            else
            {
                {
                    ProfZoneNamed("DBVH Update");
                    for (u32 ri = 0; ri < pf->range_count; ++ri)
                    {
                        const struct ds_ProxyRange *range = solver_phase->proxy_range + ri;
                        for (u32 pi = 0; pi < range->count; ++pi)
                        {
                            const struct ds_ProxyDirty *dirty = range->proxy + pi;
                            ds_BitSetSet(&pipeline->shape_dirty_set, dirty->shape, 1);
                            if (dirty->reinsert)
                            {
                                ProfZoneNamed("Reinsert");
                                struct ds_Shape *shape = pipeline->shape_pool.buf + dirty->shape;
                        	    DbvhRemove(&pipeline->dynamic_bvh, shape->proxy);
                        	    shape->proxy = DbvhInsert(&pipeline->dynamic_bvh, shape->body, dirty->shape, &dirty->bbox);
                                ProfZoneEnd;
                            }
                        }
                    }
                    ProfZoneEnd;
                }

                {
                    ProfZoneNamed("Contact Removal");
                    struct ds_BitSet *dirty = &pipeline->shape_dirty_set;
                    for (u64 block = 0; block < dirty->block_count; ++block)
                    {
                        struct ds_BitBlock it = ds_BitBlockInit(dirty->bits[block], block);
		                while (ds_BitBlockHasNext(&it))
		                {
                            const u32 si = ds_BitBlockNext(&it);
                            const struct ds_Shape *shape = pipeline->shape_pool.buf + si;
                            i32 ci = shape->contact_list.first;
                            while (ci != DLL_SENTINEL)
                            {
                                struct ds_Contact *c = pipeline->contact_pool.buf + ci;
                                const i32 next = (si == c->key.shape[0])
                                               ? c->shape_contact[0].next
                                               : c->shape_contact[1].next;
                                if (!ds_ContactCheckBvhOverlap(pipeline, ci))
                                {
                                    ds_ContactRemove(pipeline, ci);
                                }
                                ci = next;
                            }
		                }
                    }
                    ProfZoneEnd;
                }
            }

            pipeline->profile->ns_rebuildphase_end = ds_TimeNs();
        }

        ArenaPopScratch();
    }
        
    struct ds_SolverSet *active = pipeline->solver_set_pool.buf + SOLVER_SET_ACTIVE;
    ds_BitSetClear(&pipeline->island_high_energy_set, 0);
    pipeline->island_to_split = DS_ID_NULL; 
    f32 global_max_low_velocity_time = F32_MIN_NEGATIVE_NORMAL;
    for (u32 i = 0; i < active->island_pool.count; ++i)
    {
        const u32 isi = active->island_pool.buf[i];
        const struct ds_Island *island = pipeline->island_pool.buf + isi; 
		f32 min_low_velocity_time = F32_MAX_POSITIVE_NORMAL;

        struct ds_Body *body;
        for (i32 bi = island->body_list.first; bi != DLL_SENTINEL; bi = body->island_body.next)
        {
            body = pipeline->body_pool.buf + bi;
            const struct ds_BodyCompute *compute = active->body_compute_pool.buf + body->sim;
			const f32 lv_sq = Vec3Dot(compute->linear_velocity, compute->linear_velocity);
			const f32 av_sq = Vec3Dot(compute->angular_velocity, compute->angular_velocity);
			if (lv_sq <= g_solver_config->sleep_linear_velocity_sq_limit && av_sq <= g_solver_config->sleep_angular_velocity_sq_limit)
			{
				body->low_velocity_time += pipeline->timestep;
			}
            else
            {
                body->low_velocity_time = 0.0f;
            }
			min_low_velocity_time = f32_min(min_low_velocity_time, body->low_velocity_time);
        }

        /* integrate final solver velocities and update bodies and find lowest low_velocity time */
	    if (min_low_velocity_time < g_solver_config->sleep_time_threshold)
	    {
            ds_BitSetSet(&pipeline->island_high_energy_set, i, 1);
	    }
        else if (global_max_low_velocity_time < min_low_velocity_time && island->constraint_remove_count)
        {
            global_max_low_velocity_time = min_low_velocity_time;
            pipeline->island_to_split = island->id; 
        }
    }

    for (u64 block = 0; block < pipeline->island_high_energy_set.block_count; ++block)
	{
        const u64 sleep_block = ~pipeline->island_high_energy_set.bits[block];
        struct ds_BitBlock it = ds_BitBlockInit(sleep_block, block);
		while (ds_BitBlockHasNext(&it))
		{

            const u32 i = ds_BitBlockPeekNext(&it);
            if (i >= active->island_pool.count)
            {
                goto DONE;
            }

            u32 pop = 1;
            const u32 isi = active->island_pool.buf[i];
            const struct ds_Island *island = pipeline->island_pool.buf + isi;
            if (island->constraint_remove_count == 0)
            {
                if (i != active->island_pool.count-1 && ds_BitSetGet(&pipeline->island_high_energy_set, active->island_pool.count-1) == 0)
                {
                    pop = 0;
                }
                ds_SolverSetSleep(pipeline, isi);
	            PhysicsEventIslandAsleep(pipeline, island->id);
                active = pipeline->solver_set_pool.buf + SOLVER_SET_ACTIVE;
            }

            if (pop)
            {
                ds_BitBlockNext(&it);
            }
		}
	}
DONE:
}

void ds_DynamicsSleepEnable(struct ds_Dynamics *pipeline)
{
	ds_Assert(g_solver_config->sleep_enabled == 0);
	if (g_solver_config->sleep_enabled)
	{
        return;
    }

	g_solver_config->sleep_enabled = 1;
    for (u32 set_index = 0; set_index < pipeline->solver_set_pool.count_max; ++set_index)
    {
        struct ds_SolverSet *set = pipeline->solver_set_pool.buf + set_index;
        if (ds_PoolSlotAllocated(set))
        {
            for (u32 k = 0; k < set->island_pool.count; ++k)
            {
                const u32 island_index = set->island_pool.buf[k];
        	    struct ds_Island *is = pipeline->island_pool.buf + k;
            }

            ds_SolverSetWakeUp(pipeline, set_index);
        }
    }
}

void ds_DynamicsSleepDisable(struct ds_Dynamics *pipeline)
{
	ds_Assert(g_solver_config->sleep_enabled == 1);
	if (!g_solver_config->sleep_enabled)
    {
        return;
    }
	
	g_solver_config->sleep_enabled = 0;
    for (u32 set_index = 0; set_index < pipeline->solver_set_pool.count_max; ++set_index)
    {
        struct ds_SolverSet *set = pipeline->solver_set_pool.buf + set_index;
        if (ds_PoolSlotAllocated(set))
        {
            for (u32 k = 0; k < set->island_pool.count; ++k)
            {
                const u32 island_index = set->island_pool.buf[k];
	    	    struct ds_Island *is = pipeline->island_pool.buf + k;
            }

            ds_SolverSetWakeUp(pipeline, set_index);
        }
	}
}

static void UpdateSolverConfig(struct ds_Dynamics *pipeline)
{
	g_solver_config->warmup_solver = g_solver_config->pending_warmup_solver;
	g_solver_config->pgs_iteration_count = g_solver_config->pending_pgs_iteration_count;
	g_solver_config->ngs_iteration_count = g_solver_config->pending_ngs_iteration_count;
	g_solver_config->linear_slop = g_solver_config->pending_linear_slop;
	g_solver_config->baumgarte_constant = g_solver_config->pending_baumgarte_constant;
	g_solver_config->restitution_threshold = g_solver_config->pending_restitution_threshold;
	g_solver_config->linear_dampening = g_solver_config->pending_linear_dampening;
	g_solver_config->angular_dampening = g_solver_config->pending_angular_dampening;

	if (g_solver_config->pending_sleep_enabled != g_solver_config->sleep_enabled)
	{
		(g_solver_config->pending_sleep_enabled)
			? ds_DynamicsSleepEnable(pipeline)
			: ds_DynamicsSleepDisable(pipeline);

		g_solver_config->sleep_enabled = g_solver_config->pending_sleep_enabled;
	}
}

void ds_DynamicsSimulateFrame(struct ds_Dynamics *pipeline)
{
    pipeline->profile->ns_frame_start = ds_TimeNs();

	/* update, if possible, any pending values in contact solver config */
	UpdateSolverConfig(pipeline);

	/* broadphase => narrowphase => solve => integrate */
    CollisionDetection(pipeline);
	SolveConstraints(pipeline);

    for (u32 i = 0; i < pipeline->worker_count; ++i)
    {
        ds_DynamicsMetricsAdd(&pipeline->metrics, &pipeline->worker[i].metrics);
    }
    ds_DynamicsMetricsPrint(stderr, &pipeline->metrics);

	PHYSICS_PIPELINE_VALIDATE(pipeline);

    pipeline->profile->ns_frame_end = ds_TimeNs();
}

static void ds_DynamicsProfileBegin(struct ds_Dynamics *pipeline)
{
    ds_Assert(PowerOfTwoCheck(pipeline->profile_length));
    const u32 pi = pipeline->profile_next & (pipeline->profile_length-1);
    pipeline->profile = pipeline->profile_buf + pi;
    pipeline->profile_next += 1;
}

static void ds_DynamicsProfileEnd(struct ds_Dynamics *pipeline)
{
    struct ds_DynamicsProfile *p = pipeline->profile;

    p->ns_frame_duration = p->ns_frame_end - p->ns_frame_start;
    p->ns_broadphase_duration = p->ns_broadphase_end - p->ns_broadphase_start;
    p->ns_narrowphase_duration = p->ns_narrowphase_end - p->ns_narrowphase_start;
    p->ns_solverphase_duration = p->ns_solverphase_end - p->ns_solverphase_start;
    p->ns_rebuildphase_duration = p->ns_rebuildphase_end - p->ns_rebuildphase_start;

    ds_DynamicsProfilePrint(stderr, p);

    if (p->ns_frame_duration > pipeline->ns_tick)
    {
        const f32 ms_frame_duration = (f32) p->ns_frame_duration / NSEC_PER_MSEC;
        const f32 ms_frame_budget = (f32) pipeline->ns_tick / NSEC_PER_MSEC;
        Log(T_PHYSICS, S_ERROR, "Dynamics time budget surpassed! %f > %f (ms)", ms_frame_duration, ms_frame_budget);
    }
}

void ds_DynamicsProfilePrint(FILE *file, const struct ds_DynamicsProfile *p)
{
    const f32 ms_frame_duration = (f32) p->ns_frame_duration / NSEC_PER_MSEC;
    const f32 ms_broadphase_duration = (f32) p->ns_broadphase_duration / NSEC_PER_MSEC;
    const f32 ms_narrowphase_duration = (f32) p->ns_narrowphase_duration / NSEC_PER_MSEC;
    const f32 ms_solverphase_duration = (f32) p->ns_solverphase_duration / NSEC_PER_MSEC;
    const f32 ms_rebuildphase_duration = (f32) p->ns_rebuildphase_duration / NSEC_PER_MSEC;

    const f32 ms_broadphase_perc = 100.0f * ms_broadphase_duration / ms_frame_duration;
    const f32 ms_narrowphase_perc = 100.0f * ms_narrowphase_duration / ms_frame_duration;
    const f32 ms_solverphase_perc = 100.0f * ms_solverphase_duration / ms_frame_duration;
    const f32 ms_rebuildphase_perc = 100.0f * ms_rebuildphase_duration / ms_frame_duration;

    fprintf(file, "================= Profile ==============\n");
    fprintf(file, "        Time: %fms (100.0%%)\n", ms_frame_duration);
    fprintf(file, "  Broadphase: %fms (%f%%)\n", ms_broadphase_duration, ms_broadphase_perc);
    fprintf(file, " Narrowphase: %fms (%f%%)\n", ms_narrowphase_duration, ms_narrowphase_perc);
    fprintf(file, " Solverphase: %fms (%f%%)\n", ms_solverphase_duration, ms_solverphase_perc);
    fprintf(file, "Reubildphase: %fms (%f%%)\n", ms_rebuildphase_duration, ms_rebuildphase_perc);
}


void ds_DynamicsTick(struct ds_Dynamics *pipeline)
{
	ProfZone;

    ds_NumericsConfigPush(&pipeline->numerics_config);

	if (pipeline->frames_completed)
	{
		ds_DynamicsClearFrame(pipeline);
	}
	pipeline->frames_completed += 1;

    const u64 f = pipeline->frames_completed & 0x1;
    for (u32 i = 0; i < pipeline->worker_count; ++i)
    {
        ArenaFlush(pipeline->worker[i].frame_arr + f);
        pipeline->worker[i].frame = pipeline->worker[i].frame_arr + f;
        ds_DynamicsMetricsFlush(&pipeline->worker[i].metrics);
    }
    ds_DynamicsMetricsFlush(&pipeline->metrics);

    ds_DynamicsProfileBegin(pipeline);
    {
	    ds_DynamicsSimulateFrame(pipeline);
    }
    ds_DynamicsProfileEnd(pipeline);

    ds_NumericsConfigPop();

	ProfZoneEnd;
}

u64 ds_DynamicsOrientationHash(const struct ds_Dynamics *pipeline)
{
    XXH3_state_t* state = XXH3_createState();
    if (!state)
    {
	    Log(T_SYSTEM, S_FATAL, "Out of memory in %s\n", __func__);
	    FatalCleanupAndExit();
    }

    XXH3_64bits_reset(state);
    for (u32 i = 0; i < pipeline->body_pool.count_max; ++i)
    {
        const struct ds_Body *body = pipeline->body_pool.buf + i;
        if (!ds_PoolSlotAllocated(body))
        {
            continue;
        }

        const struct ds_SolverSet *set = pipeline->solver_set_pool.buf + body->set;
        const struct ds_BodySim *sim = set->body_sim_pool.buf + body->sim;

        XXH3_64bits_update(state, &i, sizeof(u32));
        XXH3_64bits_update(state, &body->flags, sizeof(body->flags));
        XXH3_64bits_update(state, sim->world.position, sizeof(vec3));
        XXH3_64bits_update(state, sim->world.rotation, sizeof(quat));
        if (body->set == SOLVER_SET_ACTIVE)
        {
            const struct ds_BodyCompute *compute = set->body_compute_pool.buf + body->sim;
            XXH3_64bits_update(state, compute->linear_velocity, sizeof(vec3));
            XXH3_64bits_update(state, compute->angular_velocity, sizeof(vec3));
        }
    }

    const u64 hash = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    return hash;
}

u32f32 ds_DynamicsRaycastParameter(const struct ds_Dynamics *pipeline, const struct ray *ray)
{
    struct arena *tmp = ArenaPushScratch();

	struct bvhRaycastInfo d_info = BvhRaycastInit(tmp, &pipeline->dynamic_bvh, ray);
	while (d_info.hit_queue.count)
	{
		const u32f32 tuple = MinQueueFixedPop(&d_info.hit_queue);
		if (d_info.hit.f < tuple.f)
		{
			break;	
		}

		if (ds_BTLeafCheck(d_info.node + tuple.u))
		{
			const u32 si = d_info.node[tuple.u].bt_child[0];
			const struct ds_Shape *shape = pipeline->shape_pool.buf + si;
			const f32 t = ds_ShapeRaycastParameter(pipeline, shape, ray);
			if (t < d_info.hit.f)
			{
				d_info.hit = u32f32_inline(si, t);
			}
		}
		else
		{
			BvhRaycastTestAndPushChildren(&d_info, tuple);
		}
	}

    ArenaFlush(tmp);

	struct bvhRaycastInfo s_info = BvhRaycastInit(tmp, &pipeline->static_bvh, ray);
	while (s_info.hit_queue.count)
	{
		const u32f32 tuple = MinQueueFixedPop(&s_info.hit_queue);
		if (s_info.hit.f < tuple.f)
		{
			break;	
		}

		if (ds_BTLeafCheck(s_info.node + tuple.u))
		{
			const u32 si = s_info.node[tuple.u].bt_child[0];
			const struct ds_Shape *shape = pipeline->shape_pool.buf + si;
			const f32 t = ds_ShapeRaycastParameter(pipeline, shape, ray);
			if (t < s_info.hit.f)
			{
				s_info.hit = u32f32_inline(si, t);
			}
		}
		else
		{
			BvhRaycastTestAndPushChildren(&s_info, tuple);
		}
	}

    ArenaPopScratch();

	return (d_info.hit.f < s_info.hit.f)
        ? d_info.hit
        : s_info.hit;
}

struct ds_PhysicsEvent *ds_PhysicsEventPush(struct ds_Dynamics *pipeline)
{
	struct slot slot = ds_PhysicsEventPoolAdd(&pipeline->event_pool);
    ds_DLLAppend(pipeline->event_list, pipeline->event_pool.buf, slot.index, node);
	struct ds_PhysicsEvent *event = slot.address;
	event->ns = pipeline->ns_start + pipeline->frames_completed * pipeline->ns_tick;
	return event;
}

void ds_DynamicsPrintUsage(const struct ds_Dynamics *pipeline)
{
    fprintf(stderr, "Physics:\n");
    fprintf(stderr, "\tbodies:                      %u\n", pipeline->body_pool.count);
    fprintf(stderr, "\tshapes:                      %u\n", pipeline->shape_pool.count);
    fprintf(stderr, "\tdynamic_bvh nodes:           %u\n", pipeline->dynamic_bvh.pool.count);
    fprintf(stderr, "\tevents:                      %u\n", pipeline->event_pool.count);
    fprintf(stderr, "\tislands:                     %u\n", pipeline->island_pool.count);
    fprintf(stderr, "\tcontacts:                    %u\n", pipeline->contact_pool.count);
}
