/*
===========================================================================
Pure Algorithmic Unit Tests: VFS Path Sanitization & Filtering
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Standalone verification of O(N) path normalization algorithm
static void Test_NormalizePath( const char *inPath, char *outPath, size_t outSize ) {
	char cleanBuffer[MAX_QPATH];
	int i = 0, j = 0;

	if ( !inPath || !outPath || outSize == 0 ) return;

	// Strip leading slashes / relative prefixes
	while ( inPath[i] == '/' || inPath[i] == '\\' || ( inPath[i] == '.' && ( inPath[i+1] == '/' || inPath[i+1] == '\\' ) ) ) {
		if ( inPath[i] == '.' ) i += 2;
		else i++;
	}

	while ( inPath[i] != '\0' && j < (int)sizeof( cleanBuffer ) - 1 ) {
		char c = inPath[i++];
		if ( c == '\\' ) c = '/';

		// Collapse double slashes
		if ( c == '/' && j > 0 && cleanBuffer[j - 1] == '/' ) {
			continue;
		}
		cleanBuffer[j++] = tolower( c );
	}
	cleanBuffer[j] = '\0';

	Q_strncpyz( outPath, cleanBuffer, outSize );
}

int Test_VFS_PathSanitizer( void ) {
	int failures = 0;
	char result[MAX_QPATH];

	printf( "[TEST] Running VFS Path Sanitizer Unit Tests...\n" );

	// 1. Double Slash & Backslash Normalization
	Test_NormalizePath( "maps\\\\escape1//textures/wall.tga", result, sizeof( result ) );
	if ( strcmp( result, "maps/escape1/textures/wall.tga" ) != 0 ) {
		printf( "  [FAIL] Expected 'maps/escape1/textures/wall.tga', got '%s'\n", result );
		failures++;
	}

	// 2. Relative Prefix Stripping
	Test_NormalizePath( "././sound/weapons/luger/fire.wav", result, sizeof( result ) );
	if ( strcmp( result, "sound/weapons/luger/fire.wav" ) != 0 ) {
		printf( "  [FAIL] Expected 'sound/weapons/luger/fire.wav', got '%s'\n", result );
		failures++;
	}

	// 3. Uppercase to Lowercase Normalization
	Test_NormalizePath( "MAIN/MODS/Z_TEST.PK3", result, sizeof( result ) );
	if ( strcmp( result, "main/mods/z_test.pk3" ) != 0 ) {
		printf( "  [FAIL] Expected 'main/mods/z_test.pk3', got '%s'\n", result );
		failures++;
	}

	if ( failures == 0 ) {
		printf( "  [PASS] VFS Path Sanitizer Unit Tests PASSED.\n" );
	}
	return failures;
}
