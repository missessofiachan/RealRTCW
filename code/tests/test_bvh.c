/*
===========================================================================
Pure Algorithmic Unit Tests: Dynamic AABB Tree / BVH Spatial Logic
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include <stdio.h>
#include <math.h>

#define BVH_FAT_AABB_PAD 2.0f

static ID_INLINE qboolean BVH_AABBOverlap_Test( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 ) {
	if ( mins1[0] > maxs2[0] || maxs1[0] < mins2[0] ) return qfalse;
	if ( mins1[1] > maxs2[1] || maxs1[1] < mins2[1] ) return qfalse;
	if ( mins1[2] > maxs2[2] || maxs1[2] < mins2[2] ) return qfalse;
	return qtrue;
}

static ID_INLINE float BVH_SurfaceArea_Test( const vec3_t mins, const vec3_t maxs ) {
	vec3_t d;
	VectorSubtract( maxs, mins, d );
	return 2.0f * ( d[0] * d[1] + d[1] * d[2] + d[2] * d[0] );
}

int Test_DynamicBVH_Algorithmic( void ) {
	int failures = 0;

	printf( "[TEST] Running Dynamic BVH Algorithmic Unit Tests...\n" );

	// 1. Overlapping Bounding Boxes
	vec3_t b1Mins = { -10.0f, -10.0f, -10.0f };
	vec3_t b1Maxs = {  10.0f,  10.0f,  10.0f };

	vec3_t b2Mins = {   5.0f,   5.0f,   5.0f };
	vec3_t b2Maxs = {  20.0f,  20.0f,  20.0f };

	if ( !BVH_AABBOverlap_Test( b1Mins, b1Maxs, b2Mins, b2Maxs ) ) {
		printf( "  [FAIL] Expected AABB overlap, returned false\n" );
		failures++;
	}

	// 2. Disjoint Bounding Boxes
	vec3_t b3Mins = {  30.0f,  30.0f,  30.0f };
	vec3_t b3Maxs = {  40.0f,  40.0f,  40.0f };

	if ( BVH_AABBOverlap_Test( b1Mins, b1Maxs, b3Mins, b3Maxs ) ) {
		printf( "  [FAIL] Expected disjoint AABBs, returned overlap true\n" );
		failures++;
	}

	// 3. Surface Area Calculation
	vec3_t boxMins = { 0.0f, 0.0f, 0.0f };
	vec3_t boxMaxs = { 2.0f, 3.0f, 4.0f };
	// SA = 2 * (2*3 + 3*4 + 4*2) = 2 * (6 + 12 + 8) = 52.0
	float sa = BVH_SurfaceArea_Test( boxMins, boxMaxs );
	if ( fabsf( sa - 52.0f ) > 0.001f ) {
		printf( "  [FAIL] SurfaceArea expected 52.0, got %f\n", sa );
		failures++;
	}

	// 4. Fat AABB Expansion Bounds Verification
	vec3_t origMins = { -5.0f, -5.0f, 0.0f };
	vec3_t origMaxs = {  5.0f,  5.0f, 10.0f };
	vec3_t fatMins, fatMaxs;

	fatMins[0] = origMins[0] - BVH_FAT_AABB_PAD;
	fatMins[1] = origMins[1] - BVH_FAT_AABB_PAD;
	fatMins[2] = origMins[2] - BVH_FAT_AABB_PAD;

	fatMaxs[0] = origMaxs[0] + BVH_FAT_AABB_PAD;
	fatMaxs[1] = origMaxs[1] + BVH_FAT_AABB_PAD;
	fatMaxs[2] = origMaxs[2] + BVH_FAT_AABB_PAD;

	// Verify padding
	if ( fatMins[0] != -7.0f || fatMaxs[0] != 7.0f ) {
		printf( "  [FAIL] Fat AABB bounds calculation error\n" );
		failures++;
	}

	if ( failures == 0 ) {
		printf( "  [PASS] Dynamic BVH Algorithmic Unit Tests PASSED.\n" );
	}
	return failures;
}
