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

#include "SDL_waylandwebos.h"

#include "../../events/SDL_events_c.h"

#include "webos-shell-client-protocol.h"

#ifdef SDL_WEBOS_HAVE_LIBHELPER
#include "../../core/webos/SDL_webos_libs.h"
#endif

// Contributed by Mariotaku <mariotaku.lee@gmail.com>

static void webos_shell_handle_state(void *data, struct wl_webos_shell_surface *shell_surface, uint32_t state)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    if (state == WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
#ifdef SDL_WEBOS_HAVE_LIBHELPER
        if (HELPERS_HProcessAppState != NULL) {
            HELPERS_HProcessAppState(3, NULL);
        }
#endif
        SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
        SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
        SDL_SendAppEvent(SDL_EVENT_DID_ENTER_BACKGROUND);
    } else if (state == WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN) {
        // Only a genuine return to the foreground, not a fullscreen re-assert.
        if (d->webos_shell_state < WL_WEBOS_SHELL_SURFACE_STATE_MAXIMIZED) {
            SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_RESTORED, 0, 0);
            SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_FOREGROUND);
            SDL_SendAppEvent(SDL_EVENT_DID_ENTER_FOREGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
            if (HELPERS_HProcessAppState != NULL) {
                HELPERS_HProcessAppState(0, NULL);
            }
#endif
        }
    }

    d->webos_shell_state = state;
}

static void webos_shell_handle_position(void *data, struct wl_webos_shell_surface *shell_surface, int32_t x, int32_t y)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_MOVED, x, y);
}

static void webos_shell_handle_close(void *data, struct wl_webos_shell_surface *shell_surface)
{
    SDL_WindowData *d = (SDL_WindowData *)data;

    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_SendWindowEvent(d->sdlwindow, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
}

static void webos_shell_handle_exposed(void *data, struct wl_webos_shell_surface *shell_surface, struct wl_array *rectangles)
{
}

static void webos_shell_handle_state_about_to_change(void *data, struct wl_webos_shell_surface *shell_surface, uint32_t state)
{
    if (state != WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
        return;
    }

    /* Sent before the state actually changes, so the app gets a chance to save
     * before it is backgrounded. */
    SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
    if (HELPERS_HProcessAppState != NULL) {
        HELPERS_HProcessAppState(1, NULL);
    }
#endif
}

static const struct wl_webos_shell_surface_listener webos_shell_surface_listener = {
    webos_shell_handle_state,
    webos_shell_handle_position,
    webos_shell_handle_close,
    webos_shell_handle_exposed,
    webos_shell_handle_state_about_to_change
};

/* The hints the compositor re-reads from a live surface. Each maps to one
 * property, and each is written explicitly either way, so an app can hand a
 * key back to the system as well as take it. */
static const char *const webos_window_hints[] = {
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META,
    SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYACTION_LAUNCH_APP,
    SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE,
    SDL_HINT_WEBOS_CLOUDGAME_ACTIVE,
    SDL_HINT_WEBOS_CURSOR_FREQUENCY
};

static void ApplyWindowHint(struct wl_webos_shell_surface *surface, const char *name)
{
    const char *value;

    if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_BACK",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_EXIT",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_HOME",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYS_META",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_RIBBON",
                                            SDL_GetHintBoolean(name, true) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYACTION_LAUNCH_APP) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_KEYACTION_LAUNCH_APP",
                                            SDL_GetHintBoolean(name, true) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE) == 0) {
        wl_webos_shell_surface_set_property(surface, "restore_cursor_position",
                                            SDL_GetHintBoolean(name, false) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CLOUDGAME_ACTIVE) == 0) {
        wl_webos_shell_surface_set_property(surface, "cloudgame_active",
                                            SDL_GetHintBoolean(name, true) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CURSOR_FREQUENCY) == 0) {
        /* No sensible way to say "back to the default rate", so a cleared or
         * invalid hint leaves whatever the compositor already has. */
        value = SDL_GetHint(name);
        if (value && SDL_atoi(value) > 0) {
            wl_webos_shell_surface_set_property(surface, "cursor_fps", value);
        }
    }
}

static void SDLCALL WindowHintCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    SDL_VideoDevice *_this = (SDL_VideoDevice *)userdata;
    SDL_Window *window;

    for (window = _this->windows; window; window = window->next) {
        SDL_WindowData *data = window->internal;

        if (data && data->shell_surface_type == WAYLAND_SHELL_SURFACE_TYPE_CUSTOM &&
            data->shell_surface.webos.webos) {
            ApplyWindowHint(data->shell_surface.webos.webos, name);
        }
    }
}

void WaylandWebOS_InitHints(SDL_VideoDevice *_this)
{
    int i;

    for (i = 0; i < SDL_arraysize(webos_window_hints); ++i) {
        SDL_AddHintCallback(webos_window_hints[i], WindowHintCallback, _this);
    }
}

void WaylandWebOS_QuitHints(SDL_VideoDevice *_this)
{
    int i;

    for (i = 0; i < SDL_arraysize(webos_window_hints); ++i) {
        SDL_RemoveHintCallback(webos_window_hints[i], WindowHintCallback, _this);
    }
}

bool WaylandWebOS_SetupSurface(SDL_VideoDevice *_this, SDL_WindowData *data)
{
    struct wl_webos_shell_surface *surface = data->shell_surface.webos.webos;
    const char *appid = SDL_getenv("APPID");
    const char *hint;
    int i;

    if (!surface) {
        return SDL_SetError("No webOS shell surface to set up");
    }

    if (!appid) {
        return SDL_SetError("APPID environment variable is not set");
    }

    wl_webos_shell_surface_add_listener(surface, &webos_shell_surface_listener, data);
    wl_webos_shell_surface_set_property(surface, "appId", appid);

    /* For example _WEBOS_WINDOW_TYPE_OVERLAY, which keeps live TV playing
     * behind the window. Only read here: the type is fixed at creation. */
    hint = SDL_GetHint(SDL_HINT_WEBOS_WINDOW_TYPE);
    if (hint && SDL_strncmp(hint, "_WEBOS_WINDOW_TYPE_", 19) == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_WINDOW_TYPE", hint);
    }

    for (i = 0; i < SDL_arraysize(webos_window_hints); ++i) {
        ApplyWindowHint(surface, webos_window_hints[i]);
    }

    hint = SDL_GetHint(SDL_HINT_WEBOS_WINDOW_CLASS);
    if (hint && SDL_strcmp(hint, "1") == 0) {
        wl_webos_shell_surface_set_property(surface, "_WEBOS_WINDOW_CLASS", hint);
    }

    // The compositor scales the surface to the screen rather than letterboxing.
    wl_webos_shell_surface_set_property(surface, "_WEBOS_ACCESS_POLICY_FORCESTRETCH", "true");

    if (data->sdlwindow->title) {
        wl_webos_shell_surface_set_property(surface, "title", data->sdlwindow->title);
    }

    return true;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND_WEBOS
