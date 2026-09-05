
#ifndef SDL_waylandwebos_cursor_h_
#define SDL_waylandwebos_cursor_h_

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS
#include "SDL_surface.h"
#include "SDL_mouse.h"

char WaylandWebOS_GetCursorSize();

SDL_Surface *WaylandWebOS_LoadCursorSurface(const char *type, const char *state);

SDL_Cursor *WaylandWebOS_ObtainHiddenCursor();

#endif /* SDL_VIDEO_DRIVER_WAYLAND_WEBOS */

#endif // SDL_waylandwebos_cursor_h_
