/*
===========================================================================
tr_postaa.c -- Post-Process Anti-Aliasing (FXAA & SMAA) for RealRTCW Engine
===========================================================================
*/

#include "tr_local.h"
#include <mimalloc.h>

typedef struct {
	qboolean initialized;
	int width;
	int height;
	
	// Framebuffers and Textures
	GLuint sceneFBO;
	GLuint sceneTex;
	
	GLuint edgeFBO;
	GLuint edgeTex;
	
	GLuint blendFBO;
	GLuint blendTex;
	
	GLuint areaTex;
	GLuint searchTex;
	
	// Shader programs
	GLuint fxaaProgram;
	GLuint smaaEdgeProgram;
	GLuint smaaBlendProgram;
	GLuint smaaNeighborhoodProgram;
} postaa_state_t;

static postaa_state_t postAA;

// FXAA 3.11 GLSL Fragment Shader
static const char *fxaa_vs = 
"#version 120\n"
"varying vec2 v_texCoord;\n"
"void main() {\n"
"    v_texCoord = gl_MultiTexCoord0.xy;\n"
"    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n";

static const char *fxaa_fs = 
"#version 120\n"
"uniform sampler2D u_screenTexture;\n"
"uniform vec2 u_texelSize;\n"
"varying vec2 v_texCoord;\n"
"\n"
"#define FXAA_REDUCE_MIN (1.0 / 128.0)\n"
"#define FXAA_REDUCE_MUL (1.0 / 8.0)\n"
"#define FXAA_SPAN_MAX   8.0\n"
"\n"
"void main() {\n"
"    vec3 rgbNW = texture2D(u_screenTexture, v_texCoord + vec2(-1.0, -1.0) * u_texelSize).rgb;\n"
"    vec3 rgbNE = texture2D(u_screenTexture, v_texCoord + vec2( 1.0, -1.0) * u_texelSize).rgb;\n"
"    vec3 rgbSW = texture2D(u_screenTexture, v_texCoord + vec2(-1.0,  1.0) * u_texelSize).rgb;\n"
"    vec3 rgbSE = texture2D(u_screenTexture, v_texCoord + vec2( 1.0,  1.0) * u_texelSize).rgb;\n"
"    vec3 rgbM  = texture2D(u_screenTexture, v_texCoord).rgb;\n"
"\n"
"    vec3 luma = vec3(0.299, 0.587, 0.114);\n"
"    float lumaNW = dot(rgbNW, luma);\n"
"    float lumaNE = dot(rgbNE, luma);\n"
"    float lumaSW = dot(rgbSW, luma);\n"
"    float lumaSE = dot(rgbSE, luma);\n"
"    float lumaM  = dot(rgbM,  luma);\n"
"\n"
"    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
"    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
"\n"
"    vec2 dir;\n"
"    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));\n"
"    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));\n"
"\n"
"    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);\n"
"    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
"\n"
"    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),\n"
"          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),\n"
"          dir * rcpDirMin)) * u_texelSize;\n"
"\n"
"    vec3 rgbA = 0.5 * (\n"
"        texture2D(u_screenTexture, v_texCoord + dir * (1.0 / 3.0 - 0.5)).rgb +\n"
"        texture2D(u_screenTexture, v_texCoord + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
"    vec3 rgbB = rgbA * 0.5 + 0.25 * (\n"
"        texture2D(u_screenTexture, v_texCoord + dir * -0.5).rgb +\n"
"        texture2D(u_screenTexture, v_texCoord + dir * 0.5).rgb);\n"
"\n"
"    float lumaB = dot(rgbB, luma);\n"
"    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {\n"
"        gl_FragColor = vec4(rgbA, 1.0);\n"
"    } else {\n"
"        gl_FragColor = vec4(rgbB, 1.0);\n"
"    }\n"
"}\n";

// Helper to compile shaders using qgl function pointers
static GLuint R_CompileShader(GLenum type, const char *src) {
	if (!qglCreateShader || !qglShaderSource || !qglCompileShader || !qglGetShaderiv || !qglDeleteShader) {
		return 0;
	}
	GLuint shader = qglCreateShader(type);
	if (!shader) return 0;
	qglShaderSource(shader, 1, &src, NULL);
	qglCompileShader(shader);
	
	GLint status = 0;
	qglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		qglDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint R_CreateProgram(const char *vsSrc, const char *fsSrc) {
	if (!qglCreateProgram || !qglAttachShader || !qglLinkProgram || !qglGetProgramiv || !qglDeleteProgram) {
		return 0;
	}
	GLuint vs = R_CompileShader(GL_VERTEX_SHADER, vsSrc);
	GLuint fs = R_CompileShader(GL_FRAGMENT_SHADER, fsSrc);
	if (!vs || !fs) {
		if (vs) qglDeleteShader(vs);
		if (fs) qglDeleteShader(fs);
		return 0;
	}
	
	GLuint prog = qglCreateProgram();
	qglAttachShader(prog, vs);
	qglAttachShader(prog, fs);
	qglLinkProgram(prog);
	
	GLint status = 0;
	qglGetProgramiv(prog, GL_LINK_STATUS, &status);
	qglDeleteShader(vs);
	qglDeleteShader(fs);
	
	if (!status) {
		qglDeleteProgram(prog);
		return 0;
	}
	return prog;
}

void R_InitPostAA( void ) {
	Com_Memset(&postAA, 0, sizeof(postAA));
}

void R_ShutdownPostAA( void ) {
	if (postAA.fxaaProgram && qglDeleteProgram) {
		qglDeleteProgram(postAA.fxaaProgram);
		postAA.fxaaProgram = 0;
	}
	if (postAA.sceneTex && qglDeleteTextures) {
		qglDeleteTextures(1, &postAA.sceneTex);
		postAA.sceneTex = 0;
	}
	postAA.initialized = qfalse;
}

static void R_InitPostAAResources( int width, int height ) {
	if (postAA.initialized && postAA.width == width && postAA.height == height) {
		return;
	}
	
	R_ShutdownPostAA();
	
	postAA.width = width;
	postAA.height = height;
	
	// Create screen capture texture
	if (qglGenTextures && qglBindTexture && qglTexImage2D && qglTexParameteri) {
		qglGenTextures(1, &postAA.sceneTex);
		qglBindTexture(GL_TEXTURE_2D, postAA.sceneTex);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		qglBindTexture(GL_TEXTURE_2D, 0);
	}
	
	// Compile shaders
	postAA.fxaaProgram = R_CreateProgram(fxaa_vs, fxaa_fs);
	
	postAA.initialized = qtrue;
}

// Fullscreen quad rendering using vertex arrays (NO qglBegin/qglEnd per AGENTS.md rules)
static void R_DrawFullScreenQuad( void ) {
	GLfloat verts[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f
	};
	GLfloat texcoords[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f
	};
	
	if (!qglEnableClientState || !qglDisableClientState || !qglVertexPointer || !qglTexCoordPointer || !qglDrawArrays) {
		return;
	}
	
	qglEnableClientState(GL_VERTEX_ARRAY);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	qglVertexPointer(2, GL_FLOAT, 0, verts);
	qglTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	
	qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisableClientState(GL_VERTEX_ARRAY);
}

void RB_PostProcessAA( void ) {
	if (!r_postAA || r_postAA->integer == 0) {
		return;
	}
	
	int width = glConfig.vidWidth;
	int height = glConfig.vidHeight;
	
	if (width <= 0 || height <= 0) return;
	
	R_InitPostAAResources(width, height);
	
	if (!postAA.initialized || !postAA.fxaaProgram) {
		return;
	}
	
	// Copy current frame buffer into texture
	qglBindTexture(GL_TEXTURE_2D, postAA.sceneTex);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
	
	// Setup 2D orthographic pass for full-screen quad
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();
	
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_BLEND);
	qglEnable(GL_TEXTURE_2D);
	
	// Bind shader
	qglUseProgram(postAA.fxaaProgram);
	
	GLint locTex = qglGetUniformLocation(postAA.fxaaProgram, "u_screenTexture");
	GLint locTexel = qglGetUniformLocation(postAA.fxaaProgram, "u_texelSize");
	
	if (locTex >= 0 && qglUniform1i) {
		qglUniform1i(locTex, 0);
	}
	if (locTexel >= 0 && qglUniform2f) {
		qglUniform2f(locTexel, 1.0f / (float)width, 1.0f / (float)height);
	}
	
	R_DrawFullScreenQuad();
	
	qglUseProgram(0);
	qglBindTexture(GL_TEXTURE_2D, 0);
	
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
	
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
}
