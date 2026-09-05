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

#include "../SDL_sysvideo.h"
#include "SDL_hints.h"
#include "SDL_timer.h"
#include "SDL_waylandwebos_foreign.h"

static void WindowIdAssigned(void *data, struct wl_webos_exported *wl_webos_exported, const char *window_id,
                             uint32_t exported_type);

static struct wl_webos_exported_listener exported_listener = {
    .window_id_assigned = WindowIdAssigned,
};

const char *WaylandWebOS_CreateExportedWindow(_THIS, SDL_webOSExportedWindowType type)
{
    SDL_VideoData *data = _this->driverdata;
    SDL_Window *window;
    SDL_WindowData *window_data;
    webos_foreign_window *foreign_window;

    if (_this->driverdata == NULL) {
        SDL_SetError("Failed creating exported window: No video driver data for video device");
        return NULL;
    }
    if ((window = SDL_GL_GetCurrentWindow()) == NULL) {
        window = _this->windows;
        while (window != NULL) {
            if (window->driverdata != NULL) {
                break;
            }
            window = window->next;
        }
    }

    if (window == NULL) {
        SDL_SetError("Failed creating exported window: No current window");
        return NULL;
    }

    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        if (SDL_RecreateWindow(window, window->flags | SDL_WINDOW_OPENGL) != 0) {
            SDL_SetError("Failed creating exported window: Failed to recreate window with OpenGL");
            return NULL;
        }
    }

    window_data = window->driverdata;
    if (window_data == NULL || window_data->surface == NULL) {
        SDL_SetError("Failed creating exported window: No surface for window");
        return NULL;
    }
    if ((uint32_t)type > SDL_WEBOS_EXPORTED_WINDOW_TYPE_OPAQUE) {
        SDL_SetError("Failed creating exported window: Invalid type");
        return NULL;
    }

    SDL_LockMutex(_this->webos_foreign_lock);
    foreign_window = SDL_calloc(1, sizeof(webos_foreign_window));
    if (foreign_window == NULL) {
        SDL_SetError("Failed creating exported window: Failed allocating memory");
        SDL_UnlockMutex(_this->webos_foreign_lock);
        return NULL;
    }

    if (data->webos_foreign_table->windows != NULL) {
        webos_foreign_window *cur = data->webos_foreign_table->windows;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = foreign_window;
    } else {
        data->webos_foreign_table->windows = foreign_window;
    }
    data->webos_foreign_table->count += 1;
    foreign_window->exported = wl_webos_foreign_export_element(data->webos_foreign, window_data->surface, type);
    wl_webos_exported_add_listener(foreign_window->exported, &exported_listener, foreign_window);
    // This is a pretty bad idea, but LG did it even worse - they create a detached thread per 10 ms
    while (foreign_window->window_id[0] == '\0') {
        SDL_Delay(10);
        WAYLAND_wl_display_dispatch(data->display);
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Created exported window %s", foreign_window->window_id);
    SDL_UnlockMutex(_this->webos_foreign_lock);
    return foreign_window->window_id;
}

SDL_bool WaylandWebOS_SetExportedWindow(_THIS, const char *windowId, SDL_Rect *src, SDL_Rect *dst)
{
    SDL_VideoData *data = _this->driverdata;
    if (data == NULL) {
        SDL_SetError("Failed setting exported window: No video driver data for video device");
        return SDL_FALSE;
    }
    if (windowId == NULL) {
        SDL_SetError("Failed setting exported window: Invalid window id");
        return SDL_FALSE;
    }
    SDL_LockMutex(_this->webos_foreign_lock);
    if (data->webos_foreign_table->count != 0) {
        webos_foreign_window *window = data->webos_foreign_table->windows;
        struct wl_region *src_region = NULL;
        struct wl_region *dst_region = NULL;
        while (window != NULL) {
            if (SDL_strcmp(window->window_id, windowId) == 0) {
                break;
            }
            window = window->next;
        }
        if (window == NULL) {
            SDL_SetError("Failed setting exported window: No exported window with id %s", windowId);
            SDL_UnlockMutex(_this->webos_foreign_lock);
            return SDL_FALSE;
        }

        if (src != NULL) {
            src_region = wl_compositor_create_region(data->compositor);
            wl_region_add(src_region, src->x, src->y, src->w, src->h);
        }
        if (dst != NULL) {
            dst_region = wl_compositor_create_region(data->compositor);
            wl_region_add(dst_region, dst->x, dst->y, dst->w, dst->h);
        }
        wl_webos_exported_set_exported_window(window->exported, src_region, dst_region);
        if (src_region != NULL) {
            wl_region_destroy(src_region);
        }
        if (dst_region != NULL) {
            wl_region_destroy(dst_region);
        }
        SDL_UnlockMutex(_this->webos_foreign_lock);
        return SDL_TRUE;
    } else {
        SDL_SetError("Failed setting exported window: No exported windows");
        SDL_UnlockMutex(_this->webos_foreign_lock);
    }
    return SDL_FALSE;
}

SDL_bool WaylandWebOS_ExportedSetCropRegion(_THIS, const char *windowId, SDL_Rect *org, SDL_Rect *src, SDL_Rect *dst)
{
    SDL_VideoData *data = _this->driverdata;
    if (data == NULL) {
        SDL_SetError("Failed setting exported window: No video driver data for video device");
        return SDL_FALSE;
    }
    if (windowId == NULL) {
        SDL_SetError("Failed setting exported window: Invalid window id");
        return SDL_FALSE;
    }
    SDL_LockMutex(_this->webos_foreign_lock);
    if (data->webos_foreign_table->count != 0) {
        webos_foreign_window *window = data->webos_foreign_table->windows;
        struct wl_region *org_region = NULL;
        struct wl_region *src_region = NULL;
        struct wl_region *dst_region = NULL;
        while (window != NULL) {
            if (SDL_strcmp(window->window_id, windowId) == 0) {
                break;
            }
            window = window->next;
        }
        if (window == NULL) {
            SDL_SetError("Failed setting exported window: No exported window with id %s", windowId);
            SDL_UnlockMutex(_this->webos_foreign_lock);
            return SDL_FALSE;
        }
        if (org != NULL) {
            org_region = wl_compositor_create_region(data->compositor);
            wl_region_add(org_region, org->x, org->y, org->w, org->h);
        }
        if (src != NULL) {
            src_region = wl_compositor_create_region(data->compositor);
            wl_region_add(src_region, src->x, src->y, src->w, src->h);
        }
        if (dst != NULL) {
            dst_region = wl_compositor_create_region(data->compositor);
            wl_region_add(dst_region, dst->x, dst->y, dst->w, dst->h);
        }
        wl_webos_exported_set_crop_region(window->exported, org_region, src_region, dst_region);
        if (org_region != NULL) {
            wl_region_destroy(org_region);
        }
        if (src_region != NULL) {
            wl_region_destroy(src_region);
        }
        if (dst_region != NULL) {
            wl_region_destroy(dst_region);
        }
        SDL_UnlockMutex(_this->webos_foreign_lock);
        return SDL_TRUE;
    } else {
        SDL_SetError("Failed setting exported window: No exported windows");
        SDL_UnlockMutex(_this->webos_foreign_lock);
    }
    return SDL_FALSE;
}

SDL_bool WaylandWebOS_ExportedSetProperty(_THIS, const char *windowId, const char *name, const char *value)
{
    SDL_VideoData *data = _this->driverdata;
    if (data == NULL) {
        SDL_SetError("Failed setting exported window: No video driver data for video device");
        return SDL_FALSE;
    }
    if (windowId == NULL) {
        SDL_SetError("Failed setting exported window: Invalid window id");
        return SDL_FALSE;
    }
    if (name == NULL) {
        SDL_SetError("Failed setting exported window: Invalid property name");
        return SDL_FALSE;
    }
    if (value == NULL) {
        SDL_SetError("Failed setting exported window: Invalid property value");
        return SDL_FALSE;
    }
    SDL_LockMutex(_this->webos_foreign_lock);
    if (data->webos_foreign_table->count != 0) {
        webos_foreign_window *window = data->webos_foreign_table->windows;
        while (window != NULL) {
            if (SDL_strcmp(window->window_id, windowId) == 0) {
                break;
            }
            window = window->next;
        }
        if (window == NULL) {
            SDL_SetError("Failed setting exported window: No exported window with id %s", windowId);
            SDL_UnlockMutex(_this->webos_foreign_lock);
            return SDL_FALSE;
        }
        wl_webos_exported_set_property(window->exported, name, value);
        SDL_UnlockMutex(_this->webos_foreign_lock);
        return SDL_TRUE;
    } else {
        SDL_SetError("Failed setting exported window: No exported windows");
        SDL_UnlockMutex(_this->webos_foreign_lock);
        return SDL_FALSE;
    }
}

void WaylandWebOS_DestroyExportedWindow(_THIS, const char *windowId)
{
    SDL_VideoData *data = _this->driverdata;
    if (windowId == NULL) {
        SDL_SetError("Failed destroying exported window: Invalid window id");
        return;
    }
    if (data == NULL) {
        SDL_SetError("Failed destroying exported window: No video driver data for video device");
        return;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Destroying exported window %s", windowId);
    SDL_LockMutex(_this->webos_foreign_lock);
    if (data->webos_foreign_table->count != 0) {
        webos_foreign_window *prev = NULL;
        webos_foreign_window *window = data->webos_foreign_table->windows;
        while (window != NULL) {
            if (SDL_strcmp(window->window_id, windowId) == 0) {
                break;
            }
            prev = window;
            window = window->next;
        }
        if (window == NULL) {
            SDL_UnlockMutex(_this->webos_foreign_lock);
            SDL_SetError("Failed destroying exported window: No exported window with id %s", windowId);
            return;
        }
        if (prev != NULL) {
            prev->next = window->next;
        } else {
            data->webos_foreign_table->windows = window->next;
        }
        data->webos_foreign_table->count -= 1;
        wl_webos_exported_destroy(window->exported);
        SDL_free(window);
        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Destroyed exported window %s", windowId);
    }
    SDL_UnlockMutex(_this->webos_foreign_lock);
}

void WindowIdAssigned(void *data, struct wl_webos_exported *wl_webos_exported, const char *window_id,
                      uint32_t exported_type)
{
    webos_foreign_window *foreign_window = data;
    (void)wl_webos_exported;
    (void)exported_type;
    SDL_strlcpy(foreign_window->window_id, window_id, sizeof(foreign_window->window_id));
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND_WEBOS */
