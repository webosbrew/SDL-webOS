#include "../../SDL_internal.h"

#ifndef SDL_webos_uevent_monitor_h_
#define SDL_webos_uevent_monitor_h_

#include "dev_presence.h"

/* Hotplug notifications straight from the kernel, via a
 * NETLINK_KOBJECT_UEVENT socket.
 *
 * There's no libudev in the webOS app jail, so the joystick and hidapi
 * backends otherwise fall back to rescanning /dev every 3 seconds and
 * diffing a presence bitmask. That's slow to react, and it can't see a
 * device that disconnects and reconnects on the same index between two
 * scans, because the bitmask comes out unchanged. Measured on hardware, a
 * whole connect/disconnect cycle can land between two scans and leave no
 * trace at all.
 *
 * The kernel broadcasts add/remove as uevents to anyone bound to group 1,
 * with no privilege and no libudev needed. Verified working inside the app
 * jail on webOS 3.4 (kernel 3.10), 4 (4.4) and 10 (5.4).
 *
 * Usage is meant to be no harder than the poll it replaces: open a monitor
 * for the node class you care about, then drain it whenever you would have
 * polled. Everything else -- enumerating what was already attached,
 * filtering to the nodes you asked for, and recovering if the kernel drops
 * events -- happens inside.
 *
 *     monitor = SDL_webOSUeventMonitorOpen(SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV);
 *
 *     while (SDL_webOSUeventMonitorPoll(monitor, &event)) {
 *         if (event.action == SDL_WEBOS_UEVENT_ACTION_ADD) {
 *             MaybeAddDevice(event.devnode);
 *         } else {
 *             MaybeRemoveDevice(event.devnode);
 *         }
 *     }
 *
 * Each subsystem should open its own monitor. Netlink broadcasts a copy to
 * every bound socket, so two monitors don't compete; sharing one would mean
 * whichever side drained it first consumed the other's events.
 */

typedef enum SDL_webOSUeventAction
{
    SDL_WEBOS_UEVENT_ACTION_ADD,
    SDL_WEBOS_UEVENT_ACTION_REMOVE,
} SDL_webOSUeventAction;

typedef struct SDL_webOSUeventMonitor SDL_webOSUeventMonitor;

typedef struct SDL_webOSUevent
{
    SDL_webOSUeventAction action;

    /* Node name, e.g. "event14", "js7", "hidraw0". */
    const char *devname;

    /* Full path, e.g. "/dev/input/event14". Ready to hand to
     * MaybeAddDevice()/MaybeRemoveDevice() without any string building. */
    const char *devnode;
} SDL_webOSUevent;

/* Opens a monitor for one class of device node. Binds the socket first and
 * only then enumerates what's already attached, so a device appearing in
 * between isn't missed; those existing devices come back out of Poll() as
 * ordinary add events, so callers need no separate startup scan.
 *
 * Returns NULL if the socket can't be created or bound, which callers must
 * treat as a normal outcome and handle by keeping the presence-flag poll. */
extern SDL_webOSUeventMonitor *SDL_webOSUeventMonitorOpen(SDL_webOSDevicePresenceCheck watch);

extern void SDL_webOSUeventMonitorClose(SDL_webOSUeventMonitor *monitor);

/* Reads one pending event, without blocking. Returns SDL_FALSE once there's
 * nothing left; call it in a loop, since a burst can queue several.
 *
 * Only reports the node class the monitor was opened for. Strings in `event`
 * stay valid until the next call on the same monitor.
 *
 * Netlink is lossy under pressure: rather than blocking the sender, the
 * kernel discards broadcasts and reports ENOBUFS once. That's handled here
 * rather than exposed -- the monitor re-enumerates and reports the
 * difference against what it last told the caller, so the caller still sees
 * a correct add/remove stream and never has to know it happened. It matters
 * because a webOS app can be backgrounded or suspended, and a process that
 * isn't draining while devices come and go is exactly how the buffer fills. */
extern SDL_bool SDL_webOSUeventMonitorPoll(SDL_webOSUeventMonitor *monitor, SDL_webOSUevent *event);

#endif /* SDL_webos_uevent_monitor_h_ */
