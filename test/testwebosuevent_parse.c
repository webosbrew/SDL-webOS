/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Unit tests for the netlink uevent parser.
 *
 * The companion testwebosuevent needs real hardware to say anything, so this
 * covers the message parsing on its own, with synthetic uevents captured from
 * a DualShock 4 on webOS 10. Runs anywhere, no devices required.
 */

#include "../src/SDL_internal.h"

#include <stdio.h>
#include <string.h>

#include "SDL_stdinc.h"

/* Compiled in rather than linked: ParseUevent is static, and this follows the
 * same approach testevdev.c takes with SDL_evdev_capabilities.c. */
#include "../src/joystick/webos/uevent_monitor.c"
#include "../src/joystick/webos/uevent_monitor.h"

static int failures;

/* ParseUevent now hands back the raw fields; the monitor turns those into a
 * SDL_webOSUevent once it knows the node class it's watching. Reassemble one
 * here so the cases below stay readable. */
static SDL_bool Parse(char *buf, size_t len, SDL_webOSUevent *event)
{
    const char *subsystem = NULL;
    const char *devname = NULL;
    SDL_webOSUeventAction action;

    SDL_zerop(event);

    if (!ParseUevent(buf, len, &subsystem, &devname, &action)) {
        return SDL_FALSE;
    }

    event->action = action;
    event->devname = devname;

    return SDL_TRUE;
}

/* Assembles the NUL-separated field list the kernel actually sends. */
static size_t BuildUevent(char *buf, const char *const *fields, int count)
{
    size_t pos = 0;
    int i;

    for (i = 0; i < count; i++) {
        size_t len = SDL_strlen(fields[i]);
        SDL_memcpy(buf + pos, fields[i], len + 1);
        pos += len + 1;
    }

    buf[pos] = '\0';

    return pos;
}

static void Check(const char *name, int passed, const char *detail)
{
    printf("%-46s %s\n", name, passed ? "PASS" : "FAIL");

    if (!passed) {
        printf("%-46s      got: %s\n", "", detail ? detail : "(null)");
        failures++;
    }
}

int main(int argc, char *argv[])
{
    char buf[8192];
    SDL_webOSUevent ev;
    size_t len;

    (void)argc;
    (void)argv;

    /* A typical evdev add, as captured on the C5. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/0003:054C:09CC.0034/input/input21/event14",
            "ACTION=add",
            "DEVPATH=/devices/platform/soc/usb/0003:054C:09CC.0034/input/input21/event14",
            "SUBSYSTEM=input",
            "DEVNAME=input/event14",
            "MAJOR=13",
            "MINOR=78",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("evdev add -> event14",
              Parse(buf, len, &ev) &&
                  ev.action == SDL_WEBOS_UEVENT_ACTION_ADD &&
                  SDL_strcmp(ev.devname, "event14") == 0,
              ev.devname);
    }

    /* Remove with no DEVNAME, which older kernels can send: the trailing
     * component of DEVPATH has to carry it instead. */
    {
        static const char *const fields[] = {
            "remove@/devices/platform/soc/usb/input/input20/js7",
            "ACTION=remove",
            "DEVPATH=/devices/platform/soc/usb/input/input20/js7",
            "SUBSYSTEM=input",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("remove without DEVNAME -> DEVPATH fallback",
              Parse(buf, len, &ev) &&
                  ev.action == SDL_WEBOS_UEVENT_ACTION_REMOVE &&
                  SDL_strcmp(ev.devname, "js7") == 0,
              ev.devname);
    }

    /* hidraw, which is the subsystem the HIDAPI side watches. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/0003:054C:09CC.0034/hidraw/hidraw0",
            "ACTION=add",
            "DEVPATH=/devices/platform/soc/usb/0003:054C:09CC.0034/hidraw/hidraw0",
            "SUBSYSTEM=hidraw",
            "DEVNAME=hidraw0",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("hidraw add -> hidraw0",
              Parse(buf, len, &ev) &&
                  SDL_strcmp(ev.devname, "hidraw0") == 0,
              ev.devname);
    }

    /* Actions other than add/remove must be dropped rather than misread as
     * an arrival, which would re-add a device on every "change". */
    {
        static const char *const fields[] = {
            "change@/devices/platform/soc/usb/input/input3/event3",
            "ACTION=change",
            "DEVPATH=/devices/platform/soc/usb/input/input3/event3",
            "SUBSYSTEM=input",
            "DEVNAME=input/event3",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("change action rejected", !Parse(buf, len, &ev), "accepted");
    }

    /* bind/unbind arrive for these same devices on modern kernels. */
    {
        static const char *const fields[] = {
            "bind@/devices/platform/soc/usb/input/input3/event3",
            "ACTION=bind",
            "SUBSYSTEM=input",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("bind action rejected", !Parse(buf, len, &ev), "accepted");
    }

    /* A libudev-format message must not be read as a kernel one. */
    {
        SDL_memcpy(buf, "libudev\0", 8);
        SDL_memcpy(buf + 8, "ACTION=add\0", 11);
        Check("libudev magic rejected", !Parse(buf, 19, &ev), "accepted");
    }

    {
        static const char *const fields[] = {
            "@/devices/platform/soc/usb/input/input3/event3",
            "SUBSYSTEM=input",
            "DEVNAME=input/event3",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("missing ACTION rejected", !Parse(buf, len, &ev), "accepted");
    }

    /* Bus- and class-level events are valid but describe no node. There must
     * be no devname invented from the trailing slash. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/input/input21/",
            "ACTION=add",
            "DEVPATH=/devices/platform/soc/usb/input/input21/",
            "SUBSYSTEM=input",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("trailing-slash DEVPATH -> NULL devname",
              Parse(buf, len, &ev) && ev.devname == NULL,
              ev.devname);
    }

    /* Above index 31, which the 32-bit presence bitmask cannot represent at
     * all. Netlink has no such limit and must still report it. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/input/input40/event40",
            "ACTION=add",
            "DEVPATH=/devices/platform/soc/usb/input/input40/event40",
            "SUBSYSTEM=input",
            "DEVNAME=input/event40",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        Check("event40 parsed (beyond bitmask range)",
              Parse(buf, len, &ev) && SDL_strcmp(ev.devname, "event40") == 0,
              ev.devname);
    }

    printf("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES PRESENT");

    return failures != 0;
}
