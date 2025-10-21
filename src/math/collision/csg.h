#ifndef __KAS_CSG_H__
#define __KAS_CSG_H__

#include "kas_common.h"
#include "kas_string.h"
#include "allocator.h"
#include "array_list.h"
#include "quaternion.h"
#include "vector.h"

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

struct csg_brush
{
	struct array_list_intrusive_node header;	/* node header, MUST BE AT THE TOP OF STRUCT! */

	utf8			id;			/* unique brush name 	*/
	u32			key;			/* id hash 		*/

	enum csg_primitive	primitive;		/* primitive type 	*/

};

struct csg_instance
{
	struct array_list_intrusive_node header;	/* node header, MUST BE AT THE TOP OF STRUCT! */

	utf8			brush;			/* brush 			*/
	utf8			id;			/* unique instance name		*/
	u32			key;			/* id hash 			*/ 
	u32			node;			/* csg_node (leaf) index 	*/

	quat			rotation;		/* normalized quaternion 	*/
	vec3			position;
};

struct csg_node
{
	//TODO pool_index_macro
	//TODO place below into a binary_tree_macro
	u32 		parent;
	u32		left;
	u32		right;

	enum csg_op	op;
};

struct csg
{
	u32 tmp;
};

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
