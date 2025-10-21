#ifndef __KAS_CSG_H__
#define __KAS_CSG_H__

#include "kas_common.h"
#include "kas_string.h"
#include "allocator.h"
#include "string_database.h"
#include "quaternion.h"
#include "geometry.h"

enum csg_primitive
{
	CSG_PRIMITIVE_BOX,	
	CSG_PRIMITIVE_CUSTOM,	/* custom primitive constructed using csg_op */
	CSG_PRIMITIVE_COUNT
};

enum csg_op
{
	CSG_OP_NONE,		/* No op, csg_node is leaf 			*/
	CSG_OP_UNION,		/* node is union of left and right 		*/
	CSG_OP_INTERSECTION,	/* node is intersection of left and right 	*/
	CSG_OP_DIFFERENCE,	/* node is left without intersection with right	*/
	CSG_OP_COUNT
};

/* TODO: */
struct csg_brush
{
	enum csg_primitive	primitive;		/* primitive type 	*/

	STRING_DATABASE_SLOT_STATE;
};

/* TODO: */
struct csg_instance
{
	utf8			brush;			/* brush 			*/
	u32			node;			/* csg_node (leaf) index 	*/

	quat			rotation;		/* normalized quaternion 	*/
	vec3			position;

	POOL_SLOT_STATE;
};

/* TODO: */
struct csg_node
{
	//TODO place below into a binary_tree_macro
	u32 		parent;
	u32		left;
	u32		right;

	enum csg_op	op;

	POOL_SLOT_STATE;
};

/* TODO: */
struct csg
{
	struct string_database	brush_database;
	struct pool		instance_pool;
	struct pool		node_pool;

	struct dcel_allocator	dcel_allocator;
};

/* allocate a csg structure */
struct csg 	csg_alloc(void);
/* deallocate a csg structure */
void		csg_dealloc(struct csg *csg);
/* flush a csg structure's resources */
void		csg_flush(struct csg *csg);
/* serialize a csg structure and its resources */
void		csg_serialize(struct serialize_stream *ss, const struct csg *csg);
/* deserialize a csg stream and return the csg struct. If mem is not NULL, alloc fixed size csg on arena.  */
struct csg	csg_deserialize(struct arena *mem, struct serialize_stream *ss, const u32 growable);		

/*
Definitions
===========

csg_primitive
-------------
csg_primitives are predefined geometric primitives; When building geometry
the user begins working with csg_brushes defined by these primitives and
iteratively construct more complex shapes. 

csg_brush
---------
csg_brushes are explicit versions of csg_primitives, or the calculated result 
of a csg_node's tree. csg_nodes that are leaves have their geometries defined
by brushes.

csg_node
--------
A csg_node is a node in a binary tree; If it is a leaf, it is defined by
a csg_brush. Otherwise, its geometry is defined by (csg_op, child1, child2).
When iteratively building a world using csg, it seems reasonable to save
a csg_node as a csg_brush if the geometry is to be used multiple times.

csg_instance
------------
instance of a brush somewhere in the world; always leaf nodes.

dynamic csg world building 
==========================

state change is done through cmd_functions. This way we can also simulate 
csg manipulation in tests and use it for ui state changes.

algorithm
=========

1. find intersection brushes (DBVH and collision tests)

2. get intersection volume vertices:

	For v <- A.vertices
		if (v.inside(B))
			intersection.add(v)
		else if (v.onPlane(B))
			intersection.add(v)
			

	For e <- A.edges
		For p <- B.planes
			if (e.intersects(p))
				intersection.add(e.intersection(p)

3. get intersection volume polygons 
*/

#endif
