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

#ifdef SDL_FILESYSTEM_WEBOS

#include "../SDL_sysfilesystem.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A webOS app is the directory containing its appinfo.json. $HOME is set to it
 * when the app manager launches us, but not when a binary is run by hand, so
 * fall back to walking up from the executable. */
static char *WebOS_GetAppPath(void)
{
    const char *home;
    char *path;
    int dirfd;

    home = SDL_getenv("HOME");
    if (home && (dirfd = open(home, O_RDONLY | O_DIRECTORY)) >= 0) {
        const bool found = faccessat(dirfd, "appinfo.json", F_OK | R_OK, 0) == 0;
        close(dirfd);
        if (found) {
            return SDL_strdup(home);
        }
    }

    path = realpath("/proc/self/exe", NULL);
    if (!path) {
        SDL_SetError("Can't get executable path: %s", strerror(errno));
        return NULL;
    }

    while (path[0] != '\0') {
        bool found;
        char *slash = SDL_strrchr(path, '/');

        if (!slash) {
            break;
        }

        // Strip the last component and look there.
        *slash = '\0';

        if ((dirfd = open(path, O_RDONLY | O_DIRECTORY)) < 0) {
            break;
        }
        found = faccessat(dirfd, "appinfo.json", F_OK | R_OK, 0) == 0;
        close(dirfd);

        if (found) {
            char *result = SDL_strdup(path);
            free(path); // This should NOT be SDL_free(): realpath() allocated it
            return result;
        }
    }

    free(path); // This should NOT be SDL_free(): realpath() allocated it
    return NULL;
}

char *SDL_SYS_GetBasePath(void)
{
    char *path = WebOS_GetAppPath();
    char *result;

    if (!path) {
        // Not inside an app directory; the working directory is the best guess.
        path = realpath(".", NULL);
        if (!path) {
            return NULL;
        }
        result = SDL_strdup(path);
        free(path); // This should NOT be SDL_free(): realpath() allocated it
        return result;
    }

    // SDL expects a trailing separator.
    if (SDL_asprintf(&result, "%s/", path) < 0) {
        SDL_free(path);
        return NULL;
    }
    SDL_free(path);
    return result;
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    /* The jail gives an app one writable tree and no per-vendor layout, so
     * preferences live alongside the app itself. */
    (void)org;
    (void)app;
    return SDL_SYS_GetBasePath();
}

char *SDL_SYS_GetExeName(void)
{
    char *path = realpath("/proc/self/exe", NULL);
    char *result;

    if (!path) {
        SDL_SetError("Can't get executable path: %s", strerror(errno));
        return NULL;
    }

    result = SDL_strdup(path);
    free(path); // This should NOT be SDL_free(): realpath() allocated it
    return result;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    /* There is no XDG user-directory layout inside the app jail. */
    (void)folder;
    SDL_SetError("Unsupported on webOS");
    return NULL;
}

#endif // SDL_FILESYSTEM_WEBOS
