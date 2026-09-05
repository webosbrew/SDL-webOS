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

#if SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_waylanddyn.h"
#include "SDL_waylanddyn_backport.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC

uint32_t FALLBACK_wl_proxy_get_version(struct wl_proxy *proxy)
{
    (void)proxy;
    return 0;
}

static int parse_msg_signature(const char *signature, int *new_id_index)
{
    int count = 0;
    for (; *signature; ++signature) {
        switch (*signature) {
        case 'n':
            *new_id_index = count;
            // Intentional fallthrough
        case 'i':
        case 'u':
        case 'f':
        case 's':
        case 'o':
        case 'a':
        case 'h':
            ++count;
        }
    }
    return count;
}

struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor(struct wl_proxy *proxy, uint32_t opcode,
                                                       const struct wl_interface *interface, ...)
{
    va_list ap;
    void *varargs[20 /*WL_CLOSURE_MAX_ARGS*/];
    int num_args;
    int new_id_index = -1;
    struct wl_interface *proxy_interface;
    struct wl_proxy *id = wl_proxy_create(proxy, interface);
    if (!id) {
        return NULL;
    }

    proxy_interface = (*(struct wl_interface **)proxy);
    if (opcode > proxy_interface->method_count) {
        return NULL;
    }
    num_args = parse_msg_signature(proxy_interface->methods[opcode].signature, &new_id_index);
    if (new_id_index < 0) {
        return NULL;
    }
    memset(varargs, 0, sizeof(varargs));
    va_start(ap, interface);
    for (int i = 0; i < num_args; i++) {
        varargs[i] = va_arg(ap, void *);
    }
    va_end(ap);
    varargs[new_id_index] = id;
    wl_proxy_marshal(proxy, opcode, varargs[0], varargs[1], varargs[2], varargs[3], varargs[4],
                     varargs[5], varargs[6], varargs[7], varargs[8], varargs[9],
                     varargs[10], varargs[11], varargs[12], varargs[13], varargs[14],
                     varargs[15], varargs[16], varargs[17], varargs[18], varargs[19]);
    return id;
}

struct wl_proxy *FALLBACK_wl_proxy_marshal_constructor_versioned(struct wl_proxy *proxy, uint32_t opcode,
                                                                 const struct wl_interface *interface,
                                                                 uint32_t version, ...)
{
    va_list ap;
    void *varargs[20 /*WL_CLOSURE_MAX_ARGS*/];
    int num_args;
    int new_id_index = -1;
    struct wl_interface *proxy_interface;
    struct wl_proxy *id = wl_proxy_create(proxy, interface);
    if (!id) {
        return NULL;
    }

    proxy_interface = (*(struct wl_interface **)proxy);
    if (opcode > proxy_interface->method_count) {
        return NULL;
    }
    num_args = parse_msg_signature(proxy_interface->methods[opcode].signature, &new_id_index);
    if (new_id_index < 0) {
        return NULL;
    }
    memset(varargs, 0, sizeof(varargs));
    va_start(ap, version);
    for (int i = 0; i < num_args; i++) {
        varargs[i] = va_arg(ap, void *);
    }
    va_end(ap);
    varargs[new_id_index] = id;
    wl_proxy_marshal(proxy, opcode, varargs[0], varargs[1], varargs[2], varargs[3], varargs[4],
                     varargs[5], varargs[6], varargs[7], varargs[8], varargs[9],
                     varargs[10], varargs[11], varargs[12], varargs[13], varargs[14],
                     varargs[15], varargs[16], varargs[17], varargs[18], varargs[19]);
    return id;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC */

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
