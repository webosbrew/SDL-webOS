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

#include "SDL_waylandwebos_abifix.h"

/* Contributed by Mariotaku <mariotaku.lee@gmail.com> */

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_loadso.h"

#include "wayland-webos-abifix.h"
#include <wayland-util.h>

static struct wl_webos_abifix_method_entry wl_webos_input_manager_requests[2] = {
    { "set_cursor_visibility", -1 },
    { "get_webos_seat", -1 },
};

struct wl_webos_abifix wl_webos_input_manager_abifix = {
    .method_count = 2,
    .methods = wl_webos_input_manager_requests
};

static void *s_webOSClientLib = NULL;

static void LoadMapping(struct wl_webos_abifix *apifix);

extern int WaylandWebOS_AbiFixInit()
{
    if (s_webOSClientLib != NULL) {
        return 0;
    }
    s_webOSClientLib = SDL_LoadObject("libwayland-webos-client.so.1");
    return s_webOSClientLib == NULL ? -1 : 0;
}

extern void WaylandWebOS_AbiFixFini()
{
    wl_webos_input_manager_abifix.interface = NULL;
    if (s_webOSClientLib != NULL) {
        SDL_UnloadObject(s_webOSClientLib);
        s_webOSClientLib = NULL;
    }
}

extern const struct wl_interface *WaylandWebOS_AbiFixGetInterface(const char *name)
{
    char sym_name[256];
    struct wl_webos_abifix *abifix = NULL;
    if (SDL_strcmp(name, "wl_webos_input_manager") == 0) {
        abifix = &wl_webos_input_manager_abifix;
    } else {
        return NULL;
    }
    if (abifix->interface == NULL) {
        SDL_snprintf(sym_name, sizeof(sym_name), "%s_interface", name);
        abifix->interface = (const struct wl_interface *)SDL_LoadFunction(s_webOSClientLib, sym_name);
        SDL_assert(abifix->interface != NULL);
        LoadMapping(abifix);
    }
    return abifix->interface;
}

static void LoadMapping(struct wl_webos_abifix *apifix)
{
    for (int i = 0; i < apifix->method_count; ++i) {
        const char *name = apifix->methods[i].name;
        for (int j = 0; j < apifix->interface->method_count; ++j) {
            if (SDL_strcmp(name, apifix->interface->methods[j].name) == 0) {
                apifix->methods[i].opcode = j;
                break;
            }
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_WEBOS */
