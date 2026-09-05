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

/* Contributed by Mariotaku <mariotaku.lee@gmail.com> */

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_WAYLAND_WEBOS

#include "../../SDL_hints_c.h"
#include "../../core/webos/SDL_webos_libs.h"
#include "../../events/SDL_events_c.h"
#include "SDL_hints.h"
#include "SDL_waylandwebos.h"
#include "SDL_waylandwebos_foreign.h"
#include "webos-shell-client-protocol.h"
#include "webos-input-manager-client-protocol.h"
#include "webos-foreign-client-protocol.h"
#include "starfish-client-protocol.h"
#include "text-client-protocol.h"

static const char *webos_window_hints[] = {
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE,
    SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META,
    SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON,
    SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE,
    SDL_HINT_WEBOS_CURSOR_FREQUENCY,
    SDL_HINT_WEBOS_CURSOR_SLEEP_TIME,
    NULL,
};

static void webos_shell_handle_state(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, uint32_t state);

static void webos_shell_handle_position(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, int32_t x, int32_t y);

static void webos_shell_handle_close(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface);

static void webos_shell_handle_exposed(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, struct wl_array *rectangles);

static void webos_shell_handle_state_about_to_change(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, uint32_t state);

const static struct wl_webos_shell_surface_listener webos_shell_surface_listener = {
    .state_changed = webos_shell_handle_state,
    .position_changed = webos_shell_handle_position,
    .close = webos_shell_handle_close,
    .exposed = webos_shell_handle_exposed,
    .state_about_to_change = webos_shell_handle_state_about_to_change,
};

static void WindowHintsCallback(void *userdata, const char *name, const char *oldValue, const char *newValue);

void WaylandWebOS_VideoInit(_THIS)
{
    const char *hintValue = NULL;
    _this->webos_foreign_lock = SDL_CreateMutex();

    if ((hintValue = SDL_GetHint(SDL_HINT_WEBOS_CURSOR_SLEEP_TIME)) != NULL) {
        _this->webos_cursor_sleep_time = strtol(hintValue, NULL, 10);
    } else {
        _this->webos_cursor_sleep_time = 300000;
    }

    for (int i = 0; webos_window_hints[i] != NULL; i++) {
        SDL_AddHintCallback(webos_window_hints[i], WindowHintsCallback, _this);
    }
}

void WaylandWebOS_VideoCleanUp(_THIS)
{
    SDL_VideoData *data = _this->driverdata;

    for (int i = 0; webos_window_hints[i] != NULL; i++) {
        SDL_DelHintCallback(webos_window_hints[i], WindowHintsCallback, _this);
    }

    SDL_LockMutex(_this->webos_foreign_lock);
    if (data->webos_foreign_table != NULL) {
        webos_foreign_window *window = data->webos_foreign_table->windows;
        while (window != NULL) {
            _this->WebOSDestroyExportedWindow(_this, window->window_id);
            window = data->webos_foreign_table->windows;
        }
        SDL_free(data->webos_foreign_table);
        data->webos_foreign_table = NULL;
    }
    SDL_UnlockMutex(_this->webos_foreign_lock);
    SDL_DestroyMutex(_this->webos_foreign_lock);

    if (data->webos_foreign) {
        wl_webos_foreign_destroy(data->webos_foreign);
        data->webos_foreign = NULL;
    }

    if (data->webos_input_manager) {
        wl_webos_input_manager_destroy(data->webos_input_manager);
        data->webos_input_manager = NULL;
    }

    if (data->starfish_pointer) {
        wl_starfish_pointer_destroy(data->starfish_pointer);
        data->starfish_pointer = NULL;
    }

    if (data->text_model_factory) {
        text_model_factory_destroy(data->text_model_factory);
        data->text_model_factory = NULL;
    }

    if (data->webos_screen_keyboard_data != NULL) {
        SDL_free(data->webos_screen_keyboard_data);
    }
}

int WaylandWebOS_SetupSurface(_THIS, SDL_WindowData *data)
{
    const char *appId;
    const char *hintValue;
    appId = SDL_getenv("APPID");
    if (appId == NULL) {
        return SDL_SetError("APPID environment variable is not set");
    }
    wl_webos_shell_surface_add_listener(data->shell_surface.webos.webos, &webos_shell_surface_listener, data);
    wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "appId", appId);
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_EXIT", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_HOME", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_META", "true");
    }
    if (!SDL_GetHintBoolean(SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON, SDL_TRUE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_RIBBON", "false");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE, SDL_FALSE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "restore_cursor_position", "true");
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_CLOUDGAME_ACTIVE, SDL_TRUE)) {
        wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "cloudgame_active", "true");
    }
    if ((hintValue = SDL_GetHint(SDL_HINT_WEBOS_CURSOR_FREQUENCY)) != NULL) {
        if (SDL_strtol(hintValue, NULL, 10) > 0) {
            wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "cursor_fps", hintValue);
        }
    }
    if ((hintValue = SDL_GetHint(SDL_HINT_WEBOS_WINDOW_CLASS)) != NULL) {
        if (SDL_strncmp(hintValue, "1", 1) == 0) {
            wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_WINDOW_CLASS", hintValue);
        }
    }
    wl_webos_shell_surface_set_property(data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_FORCESTRETCH", "true");
    return 0;
}

static void webos_shell_handle_state(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, uint32_t state)
{
    SDL_WindowData *d = data;
    if (state == WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
#ifdef SDL_WEBOS_HAVE_LIBHELPER
        HELPERS_HProcessAppState(3, NULL);
#endif
        SDL_SendWindowEvent(d->sdlwindow, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
        SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
        SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
    } else if (state == WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN) {
        if (d->webos_shell_state < WL_WEBOS_SHELL_SURFACE_STATE_MAXIMIZED) {
            SDL_SendWindowEvent(d->sdlwindow, SDL_WINDOWEVENT_RESTORED, 0, 0);
            SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);
            SDL_SendAppEvent(SDL_APP_DIDENTERFOREGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
            HELPERS_HProcessAppState(0, NULL);
#endif
        }
    }
    d->webos_shell_state = state;
}

static void webos_shell_handle_position(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, int32_t x, int32_t y)
{
    SDL_WindowData *d = data;
    SDL_SendWindowEvent(d->sdlwindow, SDL_WINDOWEVENT_MOVED, x, y);
}

static void webos_shell_handle_close(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface)
{
    SDL_WindowData *d = data;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_SendWindowEvent(d->sdlwindow, SDL_WINDOWEVENT_CLOSE, 0, 0);
}

static void webos_shell_handle_exposed(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, struct wl_array *rectangles)
{
}

static void webos_shell_handle_state_about_to_change(void *data, struct wl_webos_shell_surface *wl_webos_shell_surface, uint32_t state)
{
    if (state != WL_WEBOS_SHELL_SURFACE_STATE_MINIMIZED) {
        return;
    }
    SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
#ifdef SDL_WEBOS_HAVE_LIBHELPER
    HELPERS_HProcessAppState(1, NULL);
#endif
}

static void WindowHintsCallback(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    SDL_VideoDevice *_this = userdata;
    SDL_Window *windows = _this->windows;
    SDL_WindowData *win_data;
    if (windows == NULL) {
        return;
    }
    win_data = windows->driverdata;
    if (win_data == NULL) {
        return;
    }
    if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_BACK",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_EXIT) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_EXIT",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_HOME",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_GUIDE) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_GUIDE",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_META) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_KEYS_META",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_ACCESS_POLICY_RIBBON) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "_WEBOS_ACCESS_POLICY_RIBBON",
                                            SDL_GetStringBoolean(newValue, SDL_TRUE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CURSOR_CALIBRATION_DISABLE) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "restore_cursor_position",
                                            SDL_GetStringBoolean(newValue, SDL_FALSE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CLOUDGAME_ACTIVE) == 0) {
        wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "cloudgame_active",
                                            SDL_GetStringBoolean(newValue, SDL_TRUE) ? "true" : "false");
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CURSOR_FREQUENCY) == 0) {
        if (SDL_strtol(newValue, NULL, 10) > 0) {
            wl_webos_shell_surface_set_property(win_data->shell_surface.webos.webos, "cursor_fps", newValue);
        }
    } else if (SDL_strcmp(name, SDL_HINT_WEBOS_CURSOR_SLEEP_TIME) == 0) {
        if (newValue != NULL) {
            _this->webos_cursor_sleep_time = strtol(newValue, NULL, 10);
        } else {
            _this->webos_cursor_sleep_time = 300000;
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_WEBOS */
