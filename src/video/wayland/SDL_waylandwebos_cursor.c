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

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "SDL_waylandwebos_cursor.h"
#include "SDL_waylandwebos_abifix.h"
#include "SDL_waylandvideo.h"

#include "../SDL_sysvideo.h"

#ifdef SDL_WEBOS_HAVE_LIBHELPER
#include "../../core/webos/SDL_webos_json.h"
#include "../../core/webos/SDL_webos_luna.h"
#endif

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

static bool cursor_hidden = false;
static char cursor_size = 0;

static char WaylandWebOS_GetCursorSize(void)
{
#ifdef SDL_WEBOS_HAVE_LIBHELPER
    const char *uri = "luna://com.webos.settingsservice/getSystemSettings";
    const char *payload = "{\"keys\":[\"pointerSize\"],\"category\":\"option\",\"subscribe\":false}";
    char *response = NULL;

    if (cursor_size) {
        return cursor_size;
    }

    cursor_size = 'M';
    if (SDL_webOSLunaServiceCallSync(uri, payload, 1, &response) && response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, true)) != NULL) {
            jvalue_ref size = PBNJSON_jobject_get_nested(parsed, "settings", "pointerSize", NULL);
            if (PBNJSON_jis_string(size)) {
                raw_buffer buf = PBNJSON_jstring_get_fast(size);
                if (SDL_strncmp(buf.m_str, "small", buf.m_len) == 0) {
                    cursor_size = 'S';
                } else if (SDL_strncmp(buf.m_str, "large", buf.m_len) == 0) {
                    cursor_size = 'L';
                }
            }
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
#else
    cursor_size = 'M';
#endif
    return cursor_size;
}

static SDL_Surface *WaylandWebOS_LoadCursorSurface(const char *type, const char *state)
{
    const char size = WaylandWebOS_GetCursorSize();
    SDL_IOStream *src;
    char path[64];

    SDL_snprintf(path, sizeof(path), "/usr/share/im/cursorType%ssz%cst%s.png", type, size, state);
    src = SDL_IOFromFile(path, "rb");
    if (!src) {
        SDL_snprintf(path, sizeof(path), "/usr/share/im/fhd/cursorType%ssz%cst%s.png", type, size, state);
        src = SDL_IOFromFile(path, "rb");
    }
    if (!src) {
        return NULL;
    }

    return SDL_LoadPNG_IO(src, true);
}

SDL_Surface *WaylandWebOS_LoadSystemCursorSurface(SDL_SystemCursor id)
{
    const char *type;

    switch (id) {
    case SDL_SYSTEM_CURSOR_DEFAULT:
        type = "A";
        break;
    case SDL_SYSTEM_CURSOR_TEXT:
        type = "TEXT";
        break;
    case SDL_SYSTEM_CURSOR_POINTER:
        type = "POINT";
        break;
    case SDL_SYSTEM_CURSOR_NOT_ALLOWED:
        type = "Disable";
        break;
    case SDL_SYSTEM_CURSOR_NWSE_RESIZE:
    case SDL_SYSTEM_CURSOR_NESW_RESIZE:
    case SDL_SYSTEM_CURSOR_EW_RESIZE:
    case SDL_SYSTEM_CURSOR_NS_RESIZE:
    case SDL_SYSTEM_CURSOR_MOVE:
    case SDL_SYSTEM_CURSOR_NW_RESIZE:
    case SDL_SYSTEM_CURSOR_N_RESIZE:
    case SDL_SYSTEM_CURSOR_NE_RESIZE:
    case SDL_SYSTEM_CURSOR_E_RESIZE:
    case SDL_SYSTEM_CURSOR_SE_RESIZE:
    case SDL_SYSTEM_CURSOR_S_RESIZE:
    case SDL_SYSTEM_CURSOR_SW_RESIZE:
    case SDL_SYSTEM_CURSOR_W_RESIZE:
        type = "HOLD";
        break;
    default:
        return NULL;
    }

    return WaylandWebOS_LoadCursorSurface(type, "N");
}

bool WaylandWebOS_SetCursorVisibility(bool visible)
{
    SDL_VideoDevice *video = SDL_GetVideoDevice();
    SDL_VideoData *data = video ? video->internal : NULL;

    if (!data || !WaylandWebOS_SetInputManagerCursorVisibility(data->webos_input_manager, visible)) {
        return false;
    }

    cursor_hidden = !visible;

    return true;
}

void WaylandWebOS_FiniCursor(void)
{
    // Don't leave the compositor's pointer hidden for whatever runs next.
    if (cursor_hidden) {
        WaylandWebOS_SetCursorVisibility(true);
    }

    // Re-read the accessibility setting on the next video init.
    cursor_size = 0;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
