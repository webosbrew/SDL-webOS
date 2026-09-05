#include "../../SDL_internal.h"

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

extern SDL_bool SDL_webOSIsDeviceIndexPresent(Uint32 flags, int index);

/* Whether a device is actually behind a node. webOS creates the whole node
 * range at boot on some firmware -- 32 /dev/input/event* on both webOS 4.1 and
 * 10.3, of which only a dozen are live -- so the inode proves nothing.
 *
 * Which probe can answer varies by firmware and by jail, so callers pass
 * everything the checks might need and the first one that can decide wins. */
extern SDL_bool SDL_webOSIsCharDevicePresent(dev_t devnum, const char *devpath, const char *name);

#endif /* SDL_webos_dev_presence_h_ */
