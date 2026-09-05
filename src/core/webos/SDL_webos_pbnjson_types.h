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

#ifndef SDL_pbnjson_types_h_
#define SDL_pbnjson_types_h_

#include "SDL_stdinc.h"

typedef struct jvalue* jvalue_ref;
typedef struct jdomparser *jdomparser_ref;
typedef struct jschema* jschema_ref;

/**
 * @brief A structure to store a string and its length. This allows it to be friendly with Unicode encodings other
 * than UTF-8 but more importantly allows some no-copy optimizations.
 */
typedef struct {
	/// pointer to string
	const char *m_str;
	/// this should be set to the number of characters in m_str not including any terminating nulls.
	size_t m_len;
} raw_buffer;

/**
 * A structure representing a key/value pair in a JSON object.
 */
typedef struct {
	/// string containing name of the key
	jvalue_ref key;
	/// contains JSON value
	jvalue_ref value;
} jobject_key_value;

/// @brief opaque reference to the parser context
///
/// opaque reference to the parser context
typedef struct __JSAXContext* JSAXContextRef;

/// @brief callback function which will be invoked when there is an error while parsing the input. The callback should return true if the error is fixed and parsing can be continued
///
/// callback function that is called when there is an error while parsing the input. The callback should return true if the error is fixed and parsing can be continued
typedef int (*jerror_parsing)(void *ctxt, JSAXContextRef parseCtxt);

/// @brief callback function which will be invoked when JSON does not match a schema
///
/// callback function which will be invoked when JSON does not match a schema
typedef int (*jerror_schema)(void *ctxt, JSAXContextRef parseCtxt);

/// @brief callback function which will be invoked when general error occurs
///
/// callback function which will be invoked when general error occurs
typedef int (*jerror_misc)(void *ctxt, JSAXContextRef parseCtxt);

/**
 * @brief Structure contains set of callbacks which will be invoked if errors occur during JSON processing.
 * All fields are optional to initialize. If a callback is not specified(is NULL) it will not be called.
 *
 * Structure contains set of callbacks which will be invoked if errors occur during JSON processing.
 * All fields are optional to initialize. If a callback is not specified(is NULL) it will not be called.
 */
typedef struct JErrorCallbacks
{
	/// there was an error parsing the input
	jerror_parsing m_parser;
	/// there was an error validating the input against the schema
	jerror_schema m_schema;
	/// some other error occured while parsing
	jerror_misc m_unknown;
	/// user-specified data. Data that will be passed to callback function
	void *m_ctxt;
} *JErrorCallbacksRef;

typedef struct JSchemaResolver* JSchemaResolverRef;

/**
 * @brief JSON schema wrapper. Contains schema, resolver, error callbacks. The structure is used during JSON validation against schema
 */
typedef struct JSchemaInfo {
	/// The schema to use when parsing
	jschema_ref m_schema;
	/// The errors handlers when parsing fails
	JErrorCallbacksRef m_errHandler;
	/// The pbnjson schema resolver to invoke when an external JSON reference is encountered. Resolver should provide part of a schema,
	/// that is referenced.
	JSchemaResolverRef m_resolver;
	// future compatability - 1 slots for extra data structure pointers/callbacks.  The 2nd slot is for some undefined
	// pointer to extend this structure further if it is ever needed.
	/// Padding for future binary compatibility
	void *m_padding[2];
} JSchemaInfo, *JSchemaInfoRef;


/**
 * Convenience type representing a bit-wise combination of ::JDOMOptimization values.
 */
typedef unsigned int JDOMOptimizationFlags;


/**
 * \typedef ConversionResultFlags
 * Set of conversion result flags
 */
typedef unsigned int ConversionResultFlags;

#endif // SDL_pbnjson_types_h_

/* vi: set ts=4 sw=4 expandtab: */
