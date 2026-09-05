/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* *INDENT-OFF* */ /* clang-format off */

#include "SDL_webos_pbnjson_types.h"

#ifndef SDL_PBNJSON_SYM
#define SDL_PBNJSON_SYM(rc,fn,params)
#endif
#ifndef SDL_PBNJSON_SYM_OPT
#define SDL_PBNJSON_SYM_OPT(rc,fn,params)
#endif

SDL_PBNJSON_SYM(jvalue_ref, jobject_create_var, (jobject_key_value item, ...))
SDL_PBNJSON_SYM(jvalue_ref, jobject_get, (jvalue_ref obj, raw_buffer key))
SDL_PBNJSON_SYM(int, jobject_get_exists, (jvalue_ref obj, raw_buffer key, jvalue_ref *value))
SDL_PBNJSON_SYM(int, jis_object, (jvalue_ref val))
SDL_PBNJSON_SYM(int, jis_number, (jvalue_ref val))
SDL_PBNJSON_SYM(int, jis_string, (jvalue_ref val))
SDL_PBNJSON_SYM(int, jis_null, (jvalue_ref val))
SDL_PBNJSON_SYM(jvalue_ref, jinvalid, (void))
SDL_PBNJSON_SYM(jvalue_ref, jboolean_create, (int val))
SDL_PBNJSON_SYM(ConversionResultFlags, jboolean_get, (jvalue_ref val, int *boolean))
SDL_PBNJSON_SYM(ConversionResultFlags, jnumber_get_i32, (jvalue_ref val, int32_t *number))
SDL_PBNJSON_SYM(jvalue_ref, jstring_create_nocopy, (raw_buffer val))
SDL_PBNJSON_SYM(raw_buffer, jstring_get_fast, (jvalue_ref str))
SDL_PBNJSON_SYM(void, j_release, (jvalue_ref *val))
SDL_PBNJSON_SYM(const char*, jvalue_tostring_simple, (jvalue_ref val))
SDL_PBNJSON_SYM_OPT(const char*, jvalue_stringify, (jvalue_ref val))

SDL_PBNJSON_SYM(jschema_ref, jschema_all, ())
SDL_PBNJSON_SYM(void, jschema_info_init, (JSchemaInfoRef schemaInfo, jschema_ref schema, JSchemaResolverRef resolver, JErrorCallbacksRef errHandler))

SDL_PBNJSON_SYM(jdomparser_ref, jdomparser_create, (JSchemaInfoRef schemaInfo, JDOMOptimizationFlags optimizationMode))
SDL_PBNJSON_SYM(int, jdomparser_feed, (jdomparser_ref parser, const char *buf, int buf_len))
SDL_PBNJSON_SYM(int, jdomparser_end, (jdomparser_ref parser))
SDL_PBNJSON_SYM(void, jdomparser_release, (jdomparser_ref *parser))
SDL_PBNJSON_SYM(jvalue_ref, jdomparser_get_result, (jdomparser_ref parser))

#undef SDL_PBNJSON_SYM
#undef SDL_PBNJSON_SYM_OPT

/* *INDENT-ON* */ /* clang-format on */

/* vi: set ts=4 sw=4 expandtab: */
