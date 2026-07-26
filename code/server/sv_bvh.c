/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (“RTCW SP Source Code”).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

// sv_bvh.c -- Dynamic AABB Tree / BVH implementation for dynamic entities

#include "server.h"
#include "../mimalloc/include/mimalloc.h"

#define BVH_NULL_NODE (-1)
#define BVH_FAT_AABB_PAD 2.0f

typedef struct bvhNode_s {
	vec3_t mins;
	vec3_t maxs;
	int parent;
	int child1;
	int child2;
	int height;
	int entityNum;
} bvhNode_t;

static bvhNode_t *bvh_nodes = NULL;
static int bvh_nodeCount = 0;
static int bvh_nodeCapacity = 0;
static int bvh_rootNode = BVH_NULL_NODE;
static int bvh_freeList = BVH_NULL_NODE;

// Index mapping entNum -> bvhNodeIndex
static int bvh_entityNodes[MAX_GENTITIES];

cvar_t *sv_enableDynamicBVH;
cvar_t *sv_bvhDebug;

static int BVH_AllocateNode( void ) {
	int nodeIndex;

	if ( bvh_freeList != BVH_NULL_NODE ) {
		nodeIndex = bvh_freeList;
		bvh_freeList = bvh_nodes[nodeIndex].parent;
	} else {
		if ( bvh_nodeCount >= bvh_nodeCapacity ) {
			int newCap = ( bvh_nodeCapacity == 0 ) ? ( MAX_GENTITIES * 2 ) : ( bvh_nodeCapacity * 2 );
			bvhNode_t *newNodes = (bvhNode_t *)mi_malloc( sizeof( bvhNode_t ) * newCap );
			if ( !newNodes ) {
				Com_Error( ERR_FATAL, "BVH_AllocateNode: Out of memory allocating %d nodes", newCap );
			}
			if ( bvh_nodes ) {
				Com_Memcpy( newNodes, bvh_nodes, sizeof( bvhNode_t ) * bvh_nodeCapacity );
				mi_free( bvh_nodes );
			}
			bvh_nodes = newNodes;
			bvh_nodeCapacity = newCap;
		}
		nodeIndex = bvh_nodeCount++;
	}

	Com_Memset( &bvh_nodes[nodeIndex], 0, sizeof( bvhNode_t ) );
	bvh_nodes[nodeIndex].parent = BVH_NULL_NODE;
	bvh_nodes[nodeIndex].child1 = BVH_NULL_NODE;
	bvh_nodes[nodeIndex].child2 = BVH_NULL_NODE;
	bvh_nodes[nodeIndex].height = 0;
	bvh_nodes[nodeIndex].entityNum = ENTITYNUM_NONE;

	return nodeIndex;
}

static void BVH_FreeNode( int nodeIndex ) {
	bvh_nodes[nodeIndex].parent = bvh_freeList;
	bvh_nodes[nodeIndex].height = -1;
	bvh_freeList = nodeIndex;
}

static ID_INLINE qboolean BVH_IsLeaf( int nodeIndex ) {
	return ( bvh_nodes[nodeIndex].child1 == BVH_NULL_NODE );
}

static ID_INLINE float BVH_SurfaceArea( const vec3_t mins, const vec3_t maxs ) {
	vec3_t d;
	VectorSubtract( maxs, mins, d );
	return 2.0f * ( d[0] * d[1] + d[1] * d[2] + d[2] * d[0] );
}

static void BVH_CombineAABB( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2, vec3_t outMins, vec3_t outMaxs ) {
	outMins[0] = fminf( mins1[0], mins2[0] );
	outMins[1] = fminf( mins1[1], mins2[1] );
	outMins[2] = fminf( mins1[2], mins2[2] );

	outMaxs[0] = fmaxf( maxs1[0], maxs2[0] );
	outMaxs[1] = fmaxf( maxs1[1], maxs2[1] );
	outMaxs[2] = fmaxf( maxs1[2], maxs2[2] );
}

static ID_INLINE qboolean BVH_AABBOverlap( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 ) {
	if ( mins1[0] > maxs2[0] || maxs1[0] < mins2[0] ) return qfalse;
	if ( mins1[1] > maxs2[1] || maxs1[1] < mins2[1] ) return qfalse;
	if ( mins1[2] > maxs2[2] || maxs1[2] < mins2[2] ) return qfalse;
	return qtrue;
}

static void BVH_Refit( int nodeIndex ) {
	while ( nodeIndex != BVH_NULL_NODE ) {
		int c1 = bvh_nodes[nodeIndex].child1;
		int c2 = bvh_nodes[nodeIndex].child2;

		bvh_nodes[nodeIndex].height = 1 + ( ( bvh_nodes[c1].height > bvh_nodes[c2].height ) ? bvh_nodes[c1].height : bvh_nodes[c2].height );
		BVH_CombineAABB( bvh_nodes[c1].mins, bvh_nodes[c1].maxs, bvh_nodes[c2].mins, bvh_nodes[c2].maxs, bvh_nodes[nodeIndex].mins, bvh_nodes[nodeIndex].maxs );

		nodeIndex = bvh_nodes[nodeIndex].parent;
	}
}

static void BVH_InsertLeaf( int leafIndex ) {
	int index;
	if ( bvh_rootNode == BVH_NULL_NODE ) {
		bvh_rootNode = leafIndex;
		bvh_nodes[bvh_rootNode].parent = BVH_NULL_NODE;
		return;
	}

	// Find the best sibling for the new leaf
	index = bvh_rootNode;
	while ( !BVH_IsLeaf( index ) ) {
		int child1 = bvh_nodes[index].child1;
		int child2 = bvh_nodes[index].child2;

		float area = BVH_SurfaceArea( bvh_nodes[index].mins, bvh_nodes[index].maxs );

		vec3_t combinedMins, combinedMaxs;
		BVH_CombineAABB( bvh_nodes[index].mins, bvh_nodes[index].maxs, bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, combinedMins, combinedMaxs );
		float combinedArea = BVH_SurfaceArea( combinedMins, combinedMaxs );

		float cost = 2.0f * combinedArea;
		float inheritanceCost = 2.0f * ( combinedArea - area );

		float cost1, cost2;
		if ( BVH_IsLeaf( child1 ) ) {
			BVH_CombineAABB( bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, bvh_nodes[child1].mins, bvh_nodes[child1].maxs, combinedMins, combinedMaxs );
			cost1 = BVH_SurfaceArea( combinedMins, combinedMaxs ) + inheritanceCost;
		} else {
			BVH_CombineAABB( bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, bvh_nodes[child1].mins, bvh_nodes[child1].maxs, combinedMins, combinedMaxs );
			float oldArea = BVH_SurfaceArea( bvh_nodes[child1].mins, bvh_nodes[child1].maxs );
			float newArea = BVH_SurfaceArea( combinedMins, combinedMaxs );
			cost1 = ( newArea - oldArea ) + inheritanceCost;
		}

		if ( BVH_IsLeaf( child2 ) ) {
			BVH_CombineAABB( bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, bvh_nodes[child2].mins, bvh_nodes[child2].maxs, combinedMins, combinedMaxs );
			cost2 = BVH_SurfaceArea( combinedMins, combinedMaxs ) + inheritanceCost;
		} else {
			BVH_CombineAABB( bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, bvh_nodes[child2].mins, bvh_nodes[child2].maxs, combinedMins, combinedMaxs );
			float oldArea = BVH_SurfaceArea( bvh_nodes[child2].mins, bvh_nodes[child2].maxs );
			float newArea = BVH_SurfaceArea( combinedMins, combinedMaxs );
			cost2 = ( newArea - oldArea ) + inheritanceCost;
		}

		if ( cost < cost1 && cost < cost2 ) break;

		if ( cost1 < cost2 ) {
			index = child1;
		} else {
			index = child2;
		}
	}

	int sibling = index;
	int oldParent = bvh_nodes[sibling].parent;
	int newParent = BVH_AllocateNode();

	bvh_nodes[newParent].parent = oldParent;
	BVH_CombineAABB( bvh_nodes[leafIndex].mins, bvh_nodes[leafIndex].maxs, bvh_nodes[sibling].mins, bvh_nodes[sibling].maxs, bvh_nodes[newParent].mins, bvh_nodes[newParent].maxs );
	bvh_nodes[newParent].height = bvh_nodes[sibling].height + 1;

	if ( oldParent != BVH_NULL_NODE ) {
		if ( bvh_nodes[oldParent].child1 == sibling ) {
			bvh_nodes[oldParent].child1 = newParent;
		} else {
			bvh_nodes[oldParent].child2 = newParent;
		}
		bvh_nodes[newParent].child1 = sibling;
		bvh_nodes[newParent].child2 = leafIndex;
		bvh_nodes[sibling].parent = newParent;
		bvh_nodes[leafIndex].parent = newParent;
	} else {
		bvh_nodes[newParent].child1 = sibling;
		bvh_nodes[newParent].child2 = leafIndex;
		bvh_nodes[sibling].parent = newParent;
		bvh_nodes[leafIndex].parent = newParent;
		bvh_rootNode = newParent;
	}

	BVH_Refit( bvh_nodes[leafIndex].parent );
}

static void BVH_RemoveLeaf( int leafIndex ) {
	if ( leafIndex == bvh_rootNode ) {
		bvh_rootNode = BVH_NULL_NODE;
		return;
	}

	int parent = bvh_nodes[leafIndex].parent;
	int grandParent = bvh_nodes[parent].parent;
	int sibling = ( bvh_nodes[parent].child1 == leafIndex ) ? bvh_nodes[parent].child2 : bvh_nodes[parent].child1;

	if ( grandParent != BVH_NULL_NODE ) {
		if ( bvh_nodes[grandParent].child1 == parent ) {
			bvh_nodes[grandParent].child1 = sibling;
		} else {
			bvh_nodes[grandParent].child2 = sibling;
		}
		bvh_nodes[sibling].parent = grandParent;
		BVH_FreeNode( parent );
		BVH_Refit( grandParent );
	} else {
		bvh_rootNode = sibling;
		bvh_nodes[sibling].parent = BVH_NULL_NODE;
		BVH_FreeNode( parent );
	}
}

void SV_BVH_Init( void ) {
	int i;
	sv_enableDynamicBVH = Cvar_Get( "sv_enableDynamicBVH", "1", CVAR_ARCHIVE );
	sv_bvhDebug = Cvar_Get( "sv_bvhDebug", "0", CVAR_CHEAT );

	bvh_rootNode = BVH_NULL_NODE;
	bvh_freeList = BVH_NULL_NODE;
	bvh_nodeCount = 0;

	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		bvh_entityNodes[i] = BVH_NULL_NODE;
	}
}

void SV_BVH_Shutdown( void ) {
	if ( bvh_nodes ) {
		mi_free( bvh_nodes );
		bvh_nodes = NULL;
	}
	bvh_nodeCount = 0;
	bvh_nodeCapacity = 0;
	bvh_rootNode = BVH_NULL_NODE;
	bvh_freeList = BVH_NULL_NODE;
}

void SV_BVH_Clear( void ) {
	int i;
	bvh_rootNode = BVH_NULL_NODE;
	bvh_freeList = BVH_NULL_NODE;
	bvh_nodeCount = 0;
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		bvh_entityNodes[i] = BVH_NULL_NODE;
	}
}

void SV_BVH_InsertEntity( sharedEntity_t *gEnt ) {
	int entNum;
	int leafIndex;

	if ( !gEnt || !sv_enableDynamicBVH || !sv_enableDynamicBVH->integer ) return;

	entNum = gEnt->s.number;
	if ( entNum < 0 || entNum >= MAX_GENTITIES ) return;

	if ( bvh_entityNodes[entNum] != BVH_NULL_NODE ) {
		SV_BVH_RemoveEntity( gEnt );
	}

	leafIndex = BVH_AllocateNode();
	bvh_nodes[leafIndex].entityNum = entNum;

	// Fat AABB expansion
	bvh_nodes[leafIndex].mins[0] = gEnt->r.absmin[0] - BVH_FAT_AABB_PAD;
	bvh_nodes[leafIndex].mins[1] = gEnt->r.absmin[1] - BVH_FAT_AABB_PAD;
	bvh_nodes[leafIndex].mins[2] = gEnt->r.absmin[2] - BVH_FAT_AABB_PAD;

	bvh_nodes[leafIndex].maxs[0] = gEnt->r.absmax[0] + BVH_FAT_AABB_PAD;
	bvh_nodes[leafIndex].maxs[1] = gEnt->r.absmax[1] + BVH_FAT_AABB_PAD;
	bvh_nodes[leafIndex].maxs[2] = gEnt->r.absmax[2] + BVH_FAT_AABB_PAD;

	bvh_entityNodes[entNum] = leafIndex;
	BVH_InsertLeaf( leafIndex );
}

void SV_BVH_RemoveEntity( sharedEntity_t *gEnt ) {
	int entNum;
	int leafIndex;

	if ( !gEnt ) return;

	entNum = gEnt->s.number;
	if ( entNum < 0 || entNum >= MAX_GENTITIES ) return;

	leafIndex = bvh_entityNodes[entNum];
	if ( leafIndex == BVH_NULL_NODE ) return;

	BVH_RemoveLeaf( leafIndex );
	BVH_FreeNode( leafIndex );
	bvh_entityNodes[entNum] = BVH_NULL_NODE;
}

void SV_BVH_UpdateEntity( sharedEntity_t *gEnt ) {
	int entNum;
	int leafIndex;

	if ( !gEnt || !sv_enableDynamicBVH || !sv_enableDynamicBVH->integer ) return;

	entNum = gEnt->s.number;
	if ( entNum < 0 || entNum >= MAX_GENTITIES ) return;

	leafIndex = bvh_entityNodes[entNum];
	if ( leafIndex == BVH_NULL_NODE ) {
		SV_BVH_InsertEntity( gEnt );
		return;
	}

	// Re-insert if outside fat AABB bounds
	if ( gEnt->r.absmin[0] < bvh_nodes[leafIndex].mins[0] ||
		 gEnt->r.absmin[1] < bvh_nodes[leafIndex].mins[1] ||
		 gEnt->r.absmin[2] < bvh_nodes[leafIndex].mins[2] ||
		 gEnt->r.absmax[0] > bvh_nodes[leafIndex].maxs[0] ||
		 gEnt->r.absmax[1] > bvh_nodes[leafIndex].maxs[1] ||
		 gEnt->r.absmax[2] > bvh_nodes[leafIndex].maxs[2] ) {
		SV_BVH_RemoveEntity( gEnt );
		SV_BVH_InsertEntity( gEnt );
	}
}

int SV_BVH_QueryArea( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount ) {
	int stack[256];
	int stackCount = 0;
	int hitCount = 0;

	if ( bvh_rootNode == BVH_NULL_NODE || !sv_enableDynamicBVH || !sv_enableDynamicBVH->integer ) {
		return 0;
	}

	stack[stackCount++] = bvh_rootNode;

	while ( stackCount > 0 ) {
		int nodeIndex = stack[--stackCount];
		bvhNode_t *node = &bvh_nodes[nodeIndex];

		if ( !BVH_AABBOverlap( mins, maxs, node->mins, node->maxs ) ) {
			continue;
		}

		if ( BVH_IsLeaf( nodeIndex ) ) {
			if ( hitCount < maxcount ) {
				entityList[hitCount++] = node->entityNum;
			} else {
				break;
			}
		} else {
			if ( stackCount < 254 ) {
				stack[stackCount++] = node->child1;
				stack[stackCount++] = node->child2;
			}
		}
	}

	return hitCount;
}
