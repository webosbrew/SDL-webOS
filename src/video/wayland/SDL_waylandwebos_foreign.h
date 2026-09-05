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

#ifndef SDL_waylandwebos_foreign_h_
#define SDL_waylandwebos_foreign_h_

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_system.h"
#include "SDL_waylandwindow.h"
#include "webos-foreign-client-protocol.h"

typedef struct webos_foreign_window
{
    struct wl_webos_exported *exported;
    char window_id[32];
    struct webos_foreign_window *next;
} webos_foreign_window;

extern const char *WaylandWebOS_CreateExportedWindow(_THIS, SDL_webOSExportedWindowType type);

extern SDL_bool WaylandWebOS_SetExportedWindow(_THIS, const char *windowId, SDL_Rect *src, SDL_Rect *dst);

extern SDL_bool WaylandWebOS_ExportedSetCropRegion(_THIS, const char *windowId, SDL_Rect *org, SDL_Rect *src, SDL_Rect *dst);

extern SDL_bool WaylandWebOS_ExportedSetProperty(_THIS, const char *windowId, const char *name, const char *value);

extern void WaylandWebOS_DestroyExportedWindow(_THIS, const char *windowId);

#endif /* SDL_VIDEO_DRIVER_WAYLAND_WEBOS */

#endif /* SDL_waylandwebos_foreign_h_ */
