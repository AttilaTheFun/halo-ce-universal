/*
GL_FUNCTIONS.C

Run-time resolution of the OpenGL entry points listed in gl.h.
*/

#include "platform.h"
#define GL_FUNCTIONS_DEFINE
#include "gl.h"

#include <SDL3/SDL.h>

#define GL_DEFINE_FUNCTION(name) __typeof__(&name) halo_##name;
GL_FUNCTIONS(GL_DEFINE_FUNCTION)
#ifdef HALO_GLES
GL_OPTIONAL_FUNCTIONS(GL_DEFINE_FUNCTION)
#endif

int gl_functions_load(void)
{
	int success = TRUE;

#define GL_LOAD_FUNCTION(name) \
	halo_##name = (__typeof__(halo_##name))SDL_GL_GetProcAddress(#name); \
	if (!halo_##name) \
	{ \
		platform_log("OpenGL function %s is unavailable", #name); \
		success = FALSE; \
	}
	GL_FUNCTIONS(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION
#if defined(HALO_GLES) && !defined(HALO_WEB)
	/* a missing one is no failure: the renderer uses each only where the
	context lists its extension (gl_initialize, d3d8_gl.c). WebGL has none of
	these, and its table is not asked */
#define GL_LOAD_OPTIONAL_FUNCTION(name) \
	halo_##name = (__typeof__(halo_##name))SDL_GL_GetProcAddress(#name);
	GL_OPTIONAL_FUNCTIONS(GL_LOAD_OPTIONAL_FUNCTION)
#undef GL_LOAD_OPTIONAL_FUNCTION
#endif
	return success;
}
