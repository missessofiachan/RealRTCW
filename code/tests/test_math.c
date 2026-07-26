/*
===========================================================================
Pure Algorithmic Unit Tests: SIMD Math & Vector Utilities
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

int Test_SIMD_Math( void ) {
	int failures = 0;
	vec3_t v1 = { 3.0f, 4.0f, 0.0f };
	vec3_t v2 = { 1.0f, 2.0f, 3.0f };
	vec3_t norm;
	float len;

	printf( "[TEST] Running SIMD Math & Vector Unit Tests...\n" );

	// 1. Vector Length & DistanceSquared
	len = VectorLength( v1 );
	if ( fabsf( len - 5.0f ) > 0.0001f ) {
		printf( "  [FAIL] VectorLength expected 5.0, got %f\n", len );
		failures++;
	}

	if ( DistanceSquared( v1, v2 ) != ( ( 3-1 )*( 3-1 ) + ( 4-2 )*( 4-2 ) + ( 0-3 )*( 0-3 ) ) ) {
		printf( "  [FAIL] DistanceSquared calculation mismatch\n" );
		failures++;
	}

	// 2. Vector Normalization (SIMD / Fast RSqrt)
	VectorCopy( v1, norm );
	len = VectorNormalize( norm );
	if ( fabsf( len - 5.0f ) > 0.001f || fabsf( VectorLength( norm ) - 1.0f ) > 0.001f ) {
		printf( "  [FAIL] VectorNormalize output length error: %f\n", VectorLength( norm ) );
		failures++;
	}

	// 3. Zero Vector Normalization Safety
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	len = VectorNormalize( zero );
	if ( len != 0.0f ) {
		printf( "  [FAIL] VectorNormalize zero vector handling failed\n" );
		failures++;
	}

	// 4. Dot Product & Cross Product
	float dot = DotProduct( v1, v2 );
	if ( fabsf( dot - 11.0f ) > 0.0001f ) {
		printf( "  [FAIL] DotProduct expected 11.0, got %f\n", dot );
		failures++;
	}

	vec3_t cross;
	CrossProduct( v1, v2, cross );
	if ( cross[0] != 12.0f || cross[1] != -9.0f || cross[2] != 2.0f ) {
		printf( "  [FAIL] CrossProduct output mismatch: (%f, %f, %f)\n", cross[0], cross[1], cross[2] );
		failures++;
	}

	if ( failures == 0 ) {
		printf( "  [PASS] SIMD Math Unit Tests PASSED.\n" );
	}
	return failures;
}
