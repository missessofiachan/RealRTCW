/*
===========================================================================
Pure Algorithmic Unit Test Runner for RealRTCW Engine
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	vprintf( fmt, argptr );
	va_end( argptr );
}

void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	vprintf( fmt, argptr );
	va_end( argptr );
	exit( code );
}

extern int Test_SIMD_Math( void );
extern int Test_VFS_PathSanitizer( void );
extern int Test_DynamicBVH_Algorithmic( void );

int main( int argc, char **argv ) {
	int totalFailures = 0;

	printf( "=======================================================\n" );
	printf( "  RealRTCW Engine Pure Algorithmic Unit Test Suite    \n" );
	printf( "=======================================================\n\n" );

	totalFailures += Test_SIMD_Math();
	totalFailures += Test_VFS_PathSanitizer();
	totalFailures += Test_DynamicBVH_Algorithmic();

	printf( "\n=======================================================\n" );
	if ( totalFailures == 0 ) {
		printf( "  ALL ENGINE ALGORITHMIC UNIT TESTS PASSED SUCCESSFULLY! \n" );
		printf( "=======================================================\n" );
		return 0;
	} else {
		printf( "  UNIT TESTS COMPLETED WITH %d FAILURE(S)!\n", totalFailures );
		printf( "=======================================================\n" );
		return 1;
	}
}
