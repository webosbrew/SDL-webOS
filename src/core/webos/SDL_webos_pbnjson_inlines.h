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

#include <assert.h>

#ifdef __WEBOS__

#ifndef SDL_webos_pbnjson_inlines_h_
#define SDL_webos_pbnjson_inlines_h_

/**
 * @brief Convenience method to construct a jobject_key_value structure.
 *
 * Convenience method to construct a jobject_key_value structure.
 *
 * NOTE: Undefined behaviour if the key is NULL or not a string.
 *
 * NOTE TO IMPLEMENTOR: THIS MUST HAVE HIDDEN VISIBILITY TO INSURE THERE ARE NO CONFLICTS
 *     IN THE SYMBOL TABLE. IN THEORY, static inline SHOULD SUFFICE.
 *
 * @param key json string with a key
 * @param value json object/array/value
 * @return new json object with key/value pair
 */
static inline jobject_key_value PBNJSON_jkeyval(jvalue_ref key, jvalue_ref value)
{
    assert(key != NULL);
    assert(PBNJSON_jis_string(key));
    return (jobject_key_value){ (key), (value) };
}

/**
 * @brief Convenience inline function that casts the C-string constant/literal to a raw_buffer
 *
 * Convenience inline function that casts the C-string constant/literal to a raw_buffer
 * structure that is used for JSON strings.
 *
 * @param cstring A C string, that ends with 0
 * @return A raw_buffer struct containing { cstring, strlen(cstring) }
 */
static inline raw_buffer PBNJSON_j_cstr_to_buffer(const char *cstring)
{
    return ((raw_buffer){ cstring, strlen(cstring) });
}

/**
 * @brief Convenience inline function that creates a JSON string from a C-string constant/literal.
 *
 * Convenience inline function that creates a JSON string from a C-string constant/literal.
 *
 * @param cstring Must be a valid C-string with a lifetime longer than the resultant JSON value will have.
 *                 Safest to use with string literals or string constants (e.g. constant for the life of the program).
 */
static inline jvalue_ref PBNJSON_j_cstr_to_jval(const char *cstring)
{
    return PBNJSON_jstring_create_nocopy(PBNJSON_j_cstr_to_buffer(cstring));
}

/**
 * @brief Convenience macro to convert an arbitrary string to a JSON string. It can be used when you know length of the string
 *
 * Convenience macro to convert an arbitrary string to a JSON string. It can be used when you know length of the string
 *
 * @param string pointer to a string
 * @param length length of the string
 */
static inline raw_buffer PBNJSON_j_str_to_buffer(const char *string, size_t length)
{
    return ((raw_buffer){ (string), (length) });
}

/**
 * @brief Last argument in method jobject_create_var
 *
 */
#define J_END_OBJ_DECL ((jobject_key_value){ NULL, NULL })

/**
 * @brief Creat a buffer from a C-string literal or char array.
 *
 * Optimized version for creating a buffer from a C-string literal or char array.
 *
 * @param string A constant string literal or char array (for which the compiler knows the size)
 *               <br>NOTE: Do not use this with variables unless you understand the lifetime requirements for the string.
 *               <br>NOTE: Assumes that it is equivalent to a NULL-terminated UTF-8-compatible string.
 * @return A raw_buffer structure
 */
#define J_CSTR_TO_BUF(string) PBNJSON_j_str_to_buffer(string, sizeof(string) - 1)

/**
 * @brief  Converts a C-string literal to a jvalue_ref.
 *
 * Converts a C-string literal to a jvalue_ref.
 *
 * @param string Refer to J_CSTR_TO_BUF - same requirements
 * @return A JSON string reference
 *
 * @see J_CSTR_TO_BUF(string)
 * @see pj_cstr_to_jval
 */
#define J_CSTR_TO_JVAL(string) PBNJSON_jstring_create_nocopy(PBNJSON_j_str_to_buffer(string, sizeof(string) - 1))

/**
 * @brief Convenience method to determines whether or not the object contains a key.
 *
 * Convenience method to determines whether or not the object contains a key.
 *
 * @param obj json object to look in
 * @param key json string with key name
 * @return True if obj is an object and it has a key/value pair matching the specified key.
 */
static inline int PBNJSON_jobject_containskey(jvalue_ref obj, raw_buffer key)
{
    return PBNJSON_jobject_get_exists(obj, key, NULL);
}

#endif /* SDL_webos_pbnjson_inlines_h_ */

#endif /* __WEBOS__ */

/* vi: set ts=4 sw=4 expandtab: */
