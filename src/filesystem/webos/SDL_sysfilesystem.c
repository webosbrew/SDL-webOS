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

#ifdef __WEBOS__

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include "SDL_error.h"
#include "SDL_filesystem.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static char* GetWebOSAppPath();

char *SDL_GetBasePath(void)
{
    char *appPath = GetWebOSAppPath();
    if (appPath != NULL) {
        return appPath;
    }
    return realpath(".", NULL);
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void) org;
    (void) app;
    return SDL_GetBasePath();
}

static char* GetWebOSAppPath()
{
    int dirfd;
    const char *home;
    char *path;
    if ((home = getenv("HOME")) != NULL && (dirfd = open(home, O_RDONLY | O_DIRECTORY)) >= 0) {
        SDL_bool appinfoFound = faccessat(dirfd, "appinfo.json", F_OK | R_OK, 0) == 0;
        close(dirfd);
        if (appinfoFound) {
            return SDL_strdup(home);
        }
    }
    path = realpath("/proc/self/exe", NULL);
    if (path == NULL) {
        SDL_SetError("Can't get executable path: %s", strerror(errno));
        return NULL;
    }
    while (path[0] != '\0') {
        SDL_bool appinfoFound;
        char *slash = strrchr(path, '/');
        if (slash == NULL) {
            break;
        }
        // Strip the last path component
        *slash = '\0';
        if ((dirfd = open(path, O_RDONLY | O_DIRECTORY)) < 0) {
            break;
        }
        // Check if the current path have appinfo.json
        appinfoFound = faccessat(dirfd, "appinfo.json", F_OK | R_OK, 0) == 0;
        close(dirfd);
        if (appinfoFound) {
            return SDL_strdup(path);
        }
    }
    free(path);
    return NULL;
}

#endif /* __WEBOS__ */

/* vi: set ts=4 sw=4 expandtab: */
