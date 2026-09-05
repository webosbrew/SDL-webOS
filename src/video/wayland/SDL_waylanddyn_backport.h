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

#ifndef SDL_waylanddyn_backport_h_
#define SDL_waylanddyn_backport_h_

struct wl_interface;
struct wl_proxy;

extern uint32_t FALLBACK_wl_proxy_get_version(struct wl_proxy *proxy);

extern struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor(struct wl_proxy *proxy, uint32_t opcode,
                                                              const struct wl_interface *interface, ...);

extern struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode,
                                                                        const struct wl_interface *interface,
                                                                        uint32_t version, ...);

#endif /* SDL_waylanddyn_backport_h_ */
