/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_webos_dev_presence_h_
#define SDL_webos_dev_presence_h_

#include <sys/types.h>

typedef enum SDL_webOSDevicePresenceCheck
{
    SDL_WEBOS_DEVICE_PRESENCE_CHECK_HIDRAW,
    SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV,
    SDL_WEBOS_DEVICE_PRESENCE_CHECK_JS,
} SDL_webOSDevicePresenceCheck;

extern Uint32 SDL_webOSGetDevicePresenceFlags(SDL_webOSDevicePresenceCheck check);

extern bool SDL_webOSIsDeviceIndexPresent(Uint32 flags, int index);

/* Whether a device is actually behind a node. webOS creates the whole node
 * range at boot on some firmware -- 32 /dev/input/event* on both webOS 4.1 and
 * 10.3, of which only a dozen are live -- so the inode proves nothing.
 *
 * Which probe can answer varies by firmware and by jail, so callers pass
 * everything the checks might need and the first one that can decide wins. */
extern bool SDL_webOSIsCharDevicePresent(dev_t devnum, const char *devpath, const char *name);

#endif /* SDL_webos_dev_presence_h_ */
