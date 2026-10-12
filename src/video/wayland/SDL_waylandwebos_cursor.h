/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_waylandwebos_cursor_h_
#define SDL_waylandwebos_cursor_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

/* webOS has no cursor theme. The compositor draws its own images, kept as PNGs
 * under /usr/share/im and sized by the pointerSize accessibility setting.
 * Returns NULL for a cursor LG has no image for. */
extern SDL_Surface *WaylandWebOS_LoadSystemCursorSurface(SDL_SystemCursor id);

/* Hides or shows the compositor's own pointer, which works whether or not the
 * pointer has entered one of our surfaces. Returns false if the input manager
 * is unavailable; the caller reports the error. */
extern bool WaylandWebOS_SetCursorVisibility(bool visible);

/* The same, but only when it changes what was last asked. SDL redraws the cursor
 * on every focus change, and a request that repeats the current state, sent as
 * LG's screensaver takes focus, ends the screensaver at once. */
extern bool WaylandWebOS_UpdateCursorVisibility(bool visible);

extern void WaylandWebOS_FiniCursor(void);

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#endif // SDL_waylandwebos_cursor_h_
