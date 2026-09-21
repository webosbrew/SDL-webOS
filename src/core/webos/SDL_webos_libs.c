/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_PLATFORM_WEBOS

#include "SDL_webos_libs.h"

#define SDL_HELPERS_SYM(rc, fn, params)     SDL_DYNHELPERSFN_##fn HELPERS_##fn;
#define SDL_HELPERS_SYM_OPT(rc, fn, params) SDL_DYNHELPERSFN_##fn HELPERS_##fn;
#include "SDL_webos_helpers_sym.h"

#define SDL_PBNJSON_SYM(rc, fn, params)     SDL_DYNPBNJSONFN_##fn PBNJSON_##fn;
#define SDL_PBNJSON_SYM_OPT(rc, fn, params) SDL_DYNPBNJSONFN_##fn PBNJSON_##fn;
#include "SDL_webos_pbnjson_sym.h"

static SDL_SharedObject *LibHelpersHandle = NULL;
static SDL_SharedObject *LibPbnjsonHandle = NULL;

static SDL_FunctionPointer WebOSGetSym(SDL_SharedObject *object, const char *name, bool required, bool *valid)
{
    SDL_FunctionPointer sym = SDL_LoadFunction(object, name);
    if (required && sym == NULL) {
        *valid = false;
    }
    return sym;
}

static void UnloadHelpers(void)
{
#define SDL_HELPERS_SYM(rc, fn, params)     HELPERS_##fn = NULL;
#define SDL_HELPERS_SYM_OPT(rc, fn, params) HELPERS_##fn = NULL;
#include "SDL_webos_helpers_sym.h"
    if (LibHelpersHandle != NULL) {
        SDL_UnloadObject(LibHelpersHandle);
    }
    LibHelpersHandle = NULL;
}

/* libhelpers carries the luna service calls: app registration, the screensaver
 * and power-state subscriptions, and the app-state broadcast the media
 * pipeline listens on. Every caller checks its entry points for NULL, so a set
 * without it loses those features rather than failing to start SDL at all. */
static void LoadHelpers(void)
{
    bool valid = true;
    LibHelpersHandle = SDL_LoadObject("libhelpers.so.2");
    if (LibHelpersHandle == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM,
                    "webOS: libhelpers.so.2 is unavailable (%s); luna service calls are disabled",
                    SDL_GetError());
        // Not an error condition any more, so don't leave one set behind us.
        SDL_ClearError();
        return;
    }
#define SDL_HELPERS_SYM(rc, fn, params)     HELPERS_##fn = (SDL_DYNHELPERSFN_##fn)WebOSGetSym(LibHelpersHandle, #fn, true, &valid);
#define SDL_HELPERS_SYM_OPT(rc, fn, params) HELPERS_##fn = (SDL_DYNHELPERSFN_##fn)WebOSGetSym(LibHelpersHandle, #fn, false, &valid);
#include "SDL_webos_helpers_sym.h"
    if (!valid) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM,
                    "webOS: libhelpers.so.2 is missing required symbols; luna service calls are disabled");
        UnloadHelpers();
    }
}

static bool LoadPbnjson(void)
{
    bool valid = true;
    LibPbnjsonHandle = SDL_LoadObject("libpbnjson_c.so.2");
    if (LibPbnjsonHandle == NULL) {
        SDL_webOSUnloadLibraries();
        return SDL_SetError("Failed to load libpbnjson_c");
    }
#define SDL_PBNJSON_SYM(rc, fn, params)     PBNJSON_##fn = (SDL_DYNPBNJSONFN_##fn)WebOSGetSym(LibPbnjsonHandle, #fn, true, &valid);
#define SDL_PBNJSON_SYM_OPT(rc, fn, params) PBNJSON_##fn = (SDL_DYNPBNJSONFN_##fn)WebOSGetSym(LibPbnjsonHandle, #fn, false, &valid);
#include "SDL_webos_pbnjson_sym.h"
    if (!valid) {
        SDL_webOSUnloadLibraries();
        return SDL_SetError("Failed to load libpbnjson_c");
    }
    return true;
}

bool SDL_webOSLoadLibraries(void)
{
    // Never fatal: SDL runs without it, minus the luna-backed features.
    LoadHelpers();
    return LoadPbnjson();
}

void SDL_webOSUnloadLibraries(void)
{
    UnloadHelpers();

#define SDL_PBNJSON_SYM(rc, fn, params)     PBNJSON_##fn = NULL;
#define SDL_PBNJSON_SYM_OPT(rc, fn, params) PBNJSON_##fn = NULL;
#include "SDL_webos_pbnjson_sym.h"
    if (LibPbnjsonHandle != NULL) {
        SDL_UnloadObject(LibPbnjsonHandle);
    }
    LibPbnjsonHandle = NULL;
}

#endif // SDL_PLATFORM_WEBOS
