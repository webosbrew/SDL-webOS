/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Headless diagnostic for the netlink uevent hotplug monitor.
 *
 * Runs the netlink monitor and the /dev presence poll side by side over the
 * same hotplug activity, and reports whether netlink can replace the poll on
 * this device. No video, no window: it's meant to be run over SSH on a TV.
 *
 * It can only answer that if devices actually come and go while it runs,
 * which is why it reports INCONCLUSIVE rather than a pass when nothing
 * happened. Silence is not evidence that netlink works: an idle system
 * produces no uevents whether or not the socket is delivering them.
 *
 * Usage:
 *   testwebosuevent [--duration SECONDS] [--poll-interval MS] [--sdl]
 *
 * --sdl reports SDL's own joystick device events instead, which checks the
 * backend wiring rather than the monitor underneath it.
 *
 * Plug and unplug a controller (USB or Bluetooth) a few times while it runs.
 * Cycling one faster than the poll interval is the interesting case, since
 * that's what the presence bitmask can't represent.
 *
 * Exit status: 0 netlink usable, 1 fallback required, 2 inconclusive.
 */

/* SDL_internal.h must come first, as in testevdev.c: it installs the dynapi
 * renaming, so the public declarations that follow get renamed along with our
 * calls. Including SDL.h ahead of it leaves calls pointing at SDL_*_REAL with
 * no declaration in scope. */
#include "../src/SDL_internal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

/* Compiled in rather than linked, since these are internal and unexported. */
#include "../src/joystick/webos/dev_presence.c"
#include "../src/joystick/webos/dev_presence.h"
#include "../src/joystick/webos/uevent_monitor.c"
#include "../src/joystick/webos/uevent_monitor.h"

/* The presence bitmask is 32 bits wide, so it structurally cannot represent a
 * device above this index. Netlink has no such limit, and reporting when we
 * cross it is worth doing. */
#define PRESENCE_MAX_INDEX 32

#define TICK_INTERVAL_MS 20

typedef enum
{
    NODE_EVDEV = 0,
    NODE_JS,
    NODE_HIDRAW,
    NODE_KIND_COUNT
} NodeKind;

typedef struct
{
    const char *label;
    const char *prefix;
    SDL_webOSDevicePresenceCheck check;

    /* One monitor per class, which is how a backend uses this: netlink
     * delivers a copy to every bound socket, so they don't compete. */
    SDL_webOSUeventMonitor *monitor;

    Uint32 poll_flags;

    /* When netlink first reported each index since the last poll tick, so we
     * can say how far ahead of the poll it was. */
    Uint32 netlink_touched;
    Uint32 netlink_time[PRESENCE_MAX_INDEX];

    /* How many transitions netlink reported per index since the last poll. A
     * bitmask diff can express at most one, so anything above that is a
     * change the poll structurally cannot recover -- a reconnect, or a whole
     * connect/disconnect cycle, that happened entirely between two scans. */
    Uint8 netlink_transitions[PRESENCE_MAX_INDEX];
} NodeClass;

static NodeClass node_classes[NODE_KIND_COUNT] = {
    { "evdev", "event", SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV, NULL, 0, 0, { 0 }, { 0 } },
    { "js", "js", SDL_WEBOS_DEVICE_PRESENCE_CHECK_JS, NULL, 0, 0, { 0 }, { 0 } },
    { "hidraw", "hidraw", SDL_WEBOS_DEVICE_PRESENCE_CHECK_HIDRAW, NULL, 0, 0, { 0 }, { 0 } },
};

/* Verdict inputs */
static int poll_changes = 0;        /* bitmask transitions the poll saw */
static int poll_changes_missed = 0; /* ... that netlink never reported */
static int netlink_events = 0;      /* add/remove on a device node */
static int netlink_invisible = 0;   /* ... the bitmask diff could not express */
static int beyond_bitmask = 0;      /* nodes the 32-bit mask can't represent */
static Uint32 latency_total = 0;    /* how far netlink led the poll, summed */
static int latency_samples = 0;

static volatile int keep_running = 1;
static Uint32 start_time;

static void OnSignal(int sig)
{
    (void)sig;
    keep_running = 0;
}

static Uint32 Elapsed(void)
{
    return SDL_GetTicks() - start_time;
}

static void Report(const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

static void Report(const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    Uint32 ms = Elapsed();

    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Straight to stdout rather than SDL_Log, so the output stays greppable
     * when this is piped over SSH. */
    printf("[%3u.%03u] %s\n", ms / 1000, ms % 1000, msg);
    fflush(stdout);
}

/* The monitor only reports its own node class, so all that's left is pulling
 * the index off the end of the name. */
static int NodeIndex(const NodeClass *cls, const char *devname)
{
    const char *suffix = devname + SDL_strlen(cls->prefix);
    char *endptr = NULL;
    long value = SDL_strtol(suffix, &endptr, 10);

    if (endptr == NULL || *endptr != '\0' || value < 0) {
        return -1;
    }

    return (int)value;
}

static void HandleUevent(NodeKind kind, const SDL_webOSUevent *event)
{
    NodeClass *cls = &node_classes[kind];
    int index = NodeIndex(cls, event->devname);

    if (index < 0) {
        return;
    }

    netlink_events++;

    Report("netlink  %-6s %s", event->action == SDL_WEBOS_UEVENT_ACTION_ADD ? "add" : "remove",
           event->devnode);

    if (index >= PRESENCE_MAX_INDEX) {
        /* The poll cannot see this device at all, on any timescale. */
        beyond_bitmask++;
        Report("         ^ index %d is outside the 32-bit presence bitmask; polling can never see it", index);
        return;
    }

    if (!(cls->netlink_touched & (1u << index))) {
        cls->netlink_time[index] = SDL_GetTicks();
    }

    cls->netlink_touched |= 1u << index;

    if (cls->netlink_transitions[index] < 255) {
        cls->netlink_transitions[index]++;
    }
}

/* Drains every monitor. `announce` is false while priming, where the monitors
 * report everything already attached and there's nothing to compare against
 * yet. */
static void DrainMonitors(SDL_bool announce)
{
    int kind;

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        SDL_webOSUevent event;

        while (SDL_webOSUeventMonitorPoll(node_classes[kind].monitor, &event)) {
            if (announce) {
                HandleUevent((NodeKind)kind, &event);
            }
        }
    }
}

/* `announce` is false for the initial seeding scan, where every attached
 * device shows up as a change against an empty bitmask. Counting or printing
 * those would report the entire existing device list as arrivals that netlink
 * failed to report. */
static void PollPresence(SDL_bool announce)
{
    int kind;

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        NodeClass *cls = &node_classes[kind];
        Uint32 flags = SDL_webOSGetDevicePresenceFlags(cls->check);
        Uint32 changed = flags ^ cls->poll_flags;
        int index;

        for (index = 0; announce && index < PRESENCE_MAX_INDEX; index++) {
            Uint32 bit = 1u << index;

            if (!(changed & bit)) {
                continue;
            }

            poll_changes++;

            if (cls->netlink_touched & bit) {
                Uint32 lead = SDL_GetTicks() - cls->netlink_time[index];
                latency_total += lead;
                latency_samples++;
                Report("poll     %-6s %s/%d (netlink was %ums ahead)",
                       (flags & bit) ? "add" : "remove", cls->label, index, lead);
            } else {
                poll_changes_missed++;
                Report("poll     %-6s %s/%d  *** netlink did not report this ***",
                       (flags & bit) ? "add" : "remove", cls->label, index);
            }
        }

        /* A bitmask diff carries at most one transition per index, so compare
         * what netlink reported against what the diff could express. Anything
         * over is lost for good: a same-index reconnect (2 transitions, 0
         * expressible) or a full connect/disconnect cycle landing between two
         * scans (3 transitions, 1 expressible). */
        for (index = 0; announce && index < PRESENCE_MAX_INDEX; index++) {
            int seen = cls->netlink_transitions[index];
            int expressible = (changed & (1u << index)) ? 1 : 0;

            if (seen > expressible) {
                netlink_invisible += seen - expressible;
                Report("         %s/%d: netlink saw %d transition(s), the bitmask could express %d",
                       cls->label, index, seen, expressible);
            }
        }

        cls->poll_flags = flags;
        cls->netlink_touched = 0;
        SDL_memset(cls->netlink_transitions, 0, sizeof(cls->netlink_transitions));
    }
}

static int PrintVerdict(SDL_bool monitors_opened)
{
    printf("\n");
    printf("=========================================================\n");
    printf(" netlink events on tracked nodes : %d\n", netlink_events);
    printf(" poll-observed changes           : %d\n", poll_changes);
    printf("   corroborated by netlink       : %d\n", poll_changes - poll_changes_missed);
    printf("   missed by netlink             : %d\n", poll_changes_missed);
    printf(" transitions the poll cannot see : %d\n", netlink_invisible);
    printf(" nodes beyond the 32-bit mask    : %d\n", beyond_bitmask);

    if (latency_samples > 0) {
        printf(" mean netlink lead over poll     : %ums over %d samples\n",
               latency_total / (Uint32)latency_samples, latency_samples);
    }

    printf("---------------------------------------------------------\n");

    if (!monitors_opened) {
        printf(" VERDICT: FALLBACK REQUIRED\n");
        printf("   The netlink socket could not be opened or bound, so this\n");
        printf("   webOS version must keep the presence poll.\n");
        printf("=========================================================\n");
        return 1;
    }

    if (netlink_events == 0 && poll_changes == 0) {
        printf(" VERDICT: INCONCLUSIVE\n");
        printf("   No device appeared or disappeared during the run, so this\n");
        printf("   says nothing about whether netlink delivers. Re-run and\n");
        printf("   plug/unplug a controller while it is running.\n");
        printf("=========================================================\n");
        return 2;
    }

    if (poll_changes_missed > 0) {
        printf(" VERDICT: FALLBACK REQUIRED\n");
        printf("   The poll saw %d change(s) netlink never reported, so the\n", poll_changes_missed);
        printf("   uevent stream is not reaching this process reliably.\n");
        printf("=========================================================\n");
        return 1;
    }

    printf(" VERDICT: NETLINK USABLE\n");
    printf("   Every change the poll detected was reported by netlink first.\n");

    if (netlink_invisible > 0) {
        printf("   netlink reported %d transition(s) more than the bitmask diff\n", netlink_invisible);
        printf("   could express -- reconnects the poll structurally cannot see.\n");
    }

    printf("=========================================================\n");
    return 0;
}

/* Exercises the joystick backend rather than the monitor: brings up
 * SDL_INIT_JOYSTICK and reports the device events SDL itself produces. That's
 * what an application sees, so it's the check that the backend is really
 * wired to the uevent stream and not just that the stream works. */
static int RunSdlMode(Uint32 duration_ms)
{
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL_InitSubSystem(JOYSTICK) failed: %s\n", SDL_GetError());
        return 3;
    }

    Report("SDL joystick subsystem up, %d joystick(s) already present", SDL_NumJoysticks());
    Report("running for %us — plug and unplug a controller now", duration_ms / 1000);

    while (keep_running && Elapsed() < duration_ms) {
        SDL_Event event;

        SDL_PumpEvents();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_JOYDEVICEADDED:
                Report("SDL_JOYDEVICEADDED    device index %d (%s)", event.jdevice.which,
                       SDL_JoystickNameForIndex(event.jdevice.which));
                break;
            case SDL_JOYDEVICEREMOVED:
                Report("SDL_JOYDEVICEREMOVED  instance id %d", event.jdevice.which);
                break;
            default:
                break;
            }
        }

        SDL_Delay(TICK_INTERVAL_MS);
    }

    Report("%d joystick(s) present at exit", SDL_NumJoysticks());
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

    return 0;
}

int main(int argc, char *argv[])
{
    SDL_bool monitors_opened = SDL_TRUE;
    SDL_bool sdl_mode = SDL_FALSE;
    Uint32 duration_ms = 30000;
    Uint32 poll_interval_ms = 3000;
    Uint32 last_poll;
    int kind;
    int i;

    for (i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_ms = (Uint32)SDL_atoi(argv[++i]) * 1000;
        } else if (SDL_strcmp(argv[i], "--poll-interval") == 0 && i + 1 < argc) {
            poll_interval_ms = (Uint32)SDL_atoi(argv[++i]);
        } else if (SDL_strcmp(argv[i], "--sdl") == 0) {
            sdl_mode = SDL_TRUE;
        } else {
            fprintf(stderr, "Usage: %s [--duration SECONDS] [--poll-interval MS] [--sdl]\n", argv[0]);
            return 3;
        }
    }

    if (poll_interval_ms == 0) {
        fprintf(stderr, "--poll-interval must be greater than zero\n");
        return 3;
    }

    signal(SIGINT, OnSignal);
    signal(SIGTERM, OnSignal);

    /* No video, no joystick backend: this exercises the mechanism directly,
     * the same way a backend would. */
    if (SDL_Init(0) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 3;
    }

    SDL_LogSetPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_VERBOSE);

    start_time = SDL_GetTicks();

    if (sdl_mode) {
        int result = RunSdlMode(duration_ms);
        SDL_Quit();
        return result;
    }

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        node_classes[kind].monitor = SDL_webOSUeventMonitorOpen(node_classes[kind].check);

        if (node_classes[kind].monitor == NULL) {
            monitors_opened = SDL_FALSE;
        }
    }

    Report("netlink monitors: %s",
           monitors_opened ? "bound to the kernel uevent group" : "UNAVAILABLE");

    /* The monitors report everything already attached as adds. Discard those
     * and seed the poll to match, so the run starts from a common baseline
     * and only real hotplug activity is compared. */
    DrainMonitors(SDL_FALSE);
    PollPresence(SDL_FALSE);

    last_poll = SDL_GetTicks();

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        Report("already attached: %s %s", node_classes[kind].label,
               node_classes[kind].poll_flags ? "yes" : "none");
    }

    Report("running for %us — plug and unplug a controller now (Ctrl-C to stop early)",
           duration_ms / 1000);

    while (keep_running && Elapsed() < duration_ms) {
        DrainMonitors(SDL_TRUE);

        if (SDL_TICKS_PASSED(SDL_GetTicks(), last_poll + poll_interval_ms)) {
            PollPresence(SDL_TRUE);
            last_poll = SDL_GetTicks();
        }

        SDL_Delay(TICK_INTERVAL_MS);
    }

    /* A final drain and poll, so a change in the last interval still gets
     * cross-checked instead of being dropped on the floor at exit. */
    DrainMonitors(SDL_TRUE);
    PollPresence(SDL_TRUE);

    for (kind = 0; kind < NODE_KIND_COUNT; kind++) {
        SDL_webOSUeventMonitorClose(node_classes[kind].monitor);
        node_classes[kind].monitor = NULL;
    }

    SDL_Quit();

    return PrintVerdict(monitors_opened);
}
