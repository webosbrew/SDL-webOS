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

#ifndef SDL_webos_helpers_types_
#define SDL_webos_helpers_types_
typedef struct LSHandle LSHandle;
typedef struct LSMessage LSMessage;
typedef unsigned long LSMessageToken;
typedef struct HContext HContext;

/**
* @brief Callback function called on incoming message.
*
* @param  sh             service handle(#LSHandle)
* @param  reply          incoming message
* @param  ctx            helper context
*
* @return true if message is handled.
*/
typedef int (*HLSFilterFunc) (LSHandle *sh, LSMessage *reply, HContext *ctx);

struct HContext {
    /**
     * @brief Callback function called on incoming message.
     */
    HLSFilterFunc callback;
    void* userdata;
    void* unknown;
    /**
     * @brief Whether the call is multiple (like subscription), or one-shot.
     */
    int multiple;
    /**
     * @brief Whether the call is a public call or private call
     */
    int pub;
    LSMessageToken ret_token;
};

#endif // SDL_webos_helpers_types_

#ifndef SDL_HELPERS_SYM
#define SDL_HELPERS_SYM(rc,fn,params)
#endif // SDL_HELPERS_SYM
#ifndef SDL_HELPERS_SYM_OPT
#define SDL_HELPERS_SYM_OPT(rc,fn,params)
#endif // SDL_HELPERS_SYM_OPT

SDL_HELPERS_SYM(int, HLunaServiceCall, (const char *uri, const char *payload, HContext *context))
SDL_HELPERS_SYM(int, HUnregisterServiceCallback, (HContext *context))
SDL_HELPERS_SYM(const char*, HLunaServiceMessage, (LSMessage *msg))
SDL_HELPERS_SYM(void, HProcessAppState, (int state, void* userdata))
SDL_HELPERS_SYM(void, HRegisterAppState, (int state, void (*callback)(int state, void *userdata)))
SDL_HELPERS_SYM_OPT(void, HNDLSetLSHandle, (LSHandle *handle))
SDL_HELPERS_SYM_OPT(const char*, HGetError, (int error))

#undef SDL_HELPERS_SYM
#undef SDL_HELPERS_SYM_OPT

/* *INDENT-ON* */ /* clang-format on */

/* vi: set ts=4 sw=4 expandtab: */
