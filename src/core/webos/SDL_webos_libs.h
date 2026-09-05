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
#include "../../SDL_internal.h"

#ifdef __WEBOS__

#ifndef SDL_webos_libs_h_
#define SDL_webos_libs_h_

#include "SDL_surface.h"

extern int SDL_webOSLoadLibraries();

extern void SDL_webOSUnloadLibraries();

#define SDL_HELPERS_SYM(rc, fn, params)         \
    typedef rc(*SDL_DYNHELPERSFN_##fn) params;  \
    extern SDL_DYNHELPERSFN_##fn HELPERS_##fn;
#define SDL_HELPERS_SYM_OPT(rc, fn, params)     \
    typedef rc(*SDL_DYNHELPERSFN_##fn) params;  \
    extern SDL_DYNHELPERSFN_##fn HELPERS_##fn;
#include "SDL_webos_helpers_sym.h"


#define SDL_PBNJSON_SYM(rc, fn, params)         \
    typedef rc(*SDL_DYNPBNJSONFN_##fn) params;  \
    extern SDL_DYNPBNJSONFN_##fn PBNJSON_##fn;
#define SDL_PBNJSON_SYM_OPT(rc, fn, params)     \
    typedef rc(*SDL_DYNPBNJSONFN_##fn) params;  \
    extern SDL_DYNPBNJSONFN_##fn PBNJSON_##fn;
#include "SDL_webos_pbnjson_sym.h"
#include "SDL_webos_pbnjson_inlines.h"

#endif /* SDL_webos_libs_h_ */

#endif /* __WEBOS__ */

/* vi: set ts=4 sw=4 expandtab: */
