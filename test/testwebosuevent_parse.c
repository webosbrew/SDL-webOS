/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2023-2026 Mariotaku <git@mariotaku.me>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Unit tests for the netlink uevent monitor.
 *
 * The companion testwebosuevent needs real hardware to say anything, so this
 * covers what can be checked without any: the message parser, against
 * synthetic uevents captured from a DualShock 4 on webOS 10, and the
 * re-enumeration path, driven against a directory of node symlinks with the
 * monitor's allocations made to fail on demand. Runs anywhere, no devices
 * required.
 *
 * The socket read itself is not covered. Only the kernel may send from portid
 * 0 to a multicast group, and the monitor rejects everything else, so there is
 * no way to feed it a message from inside the process.
 */

/* Hack #1: avoid inclusion of SDL_main.h by SDL_internal.h */
#define SDL_main_h_

/* Hack #2: avoid dynapi renaming (must be done before #include <SDL3/SDL.h>) */
#include "../src/dynapi/SDL_dynapi.h"
#ifdef SDL_DYNAMIC_API
#undef SDL_DYNAMIC_API
#endif
#define SDL_DYNAMIC_API 0

#ifdef HAVE_BUILD_CONFIG
#include "../src/SDL_internal.h"
#endif

/* Hack #3: undo Hack #1 */
#ifdef SDL_main_h_
#undef SDL_main_h_
#endif
#ifdef SDL_MAIN_NOIMPL
#undef SDL_MAIN_NOIMPL
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Compiled in rather than linked: ParseUevent is static, and this follows the
 * same approach testevdev.c takes with SDL_evdev_capabilities.c. */
#include "../src/core/webos/uevent_monitor.c"
#include "../src/core/webos/uevent_monitor.h"

static int failures;

/* dev_presence.c is left out and answered here instead: the enumeration cases
 * below build their nodes out of symlinks to /dev/null, and the real probes
 * would go looking for them under /sys. */
bool SDL_webOSIsCharDevicePresent(dev_t devnum, const char *devpath, const char *name)
{
    (void)devnum;
    (void)devpath;
    (void)name;

    return true;
}

/* ParseUevent now hands back the raw fields; the monitor turns those into a
 * SDL_webOSUevent once it knows the node class it's watching. Reassemble one
 * here so the cases below stay readable. */
static bool Parse(char *buf, size_t len, SDL_webOSUevent *event)
{
    const char *subsystem = NULL;
    const char *devname = NULL;
    SDL_webOSUeventAction action;

    SDL_zerop(event);

    if (!ParseUevent(buf, len, &subsystem, &devname, &action)) {
        return false;
    }

    event->action = action;
    event->devname = devname;

    return true;
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

/* One-shot allocation failure, so the monitor's out-of-memory paths can be
 * reached without an out-of-memory machine. Counts down over every SDL
 * allocation, fails the one it lands on, and disarms itself. */
static SDL_malloc_func real_malloc;
static SDL_calloc_func real_calloc;
static SDL_realloc_func real_realloc;
static SDL_free_func real_free;

static int allocations_until_failure = -1;

static bool ShouldFail(void)
{
    if (allocations_until_failure < 0) {
        return false;
    }

    if (allocations_until_failure > 0) {
        allocations_until_failure--;
        return false;
    }

    allocations_until_failure = -1;

    return true;
}

static void *SDLCALL FailingMalloc(size_t size)
{
    return ShouldFail() ? NULL : real_malloc(size);
}

static void *SDLCALL FailingCalloc(size_t nmemb, size_t size)
{
    return ShouldFail() ? NULL : real_calloc(nmemb, size);
}

static void *SDLCALL FailingRealloc(void *mem, size_t size)
{
    return ShouldFail() ? NULL : real_realloc(mem, size);
}

static void SDLCALL FailingFree(void *mem)
{
    real_free(mem);
}

/* Log capture, for the cases that assert the monitor says something rather
 * than failing in silence. */
static char last_log[256];

static void SDLCALL CaptureLog(void *userdata, int category, SDL_LogPriority priority,
                               const char *message)
{
    (void)userdata;
    (void)category;
    (void)priority;

    SDL_strlcpy(last_log, message, sizeof(last_log));
}

/* A directory of symlinks to /dev/null, which stat as the character devices
 * the enumeration is looking for. */
static char node_dir[128];

static bool MakeNodeDir(void)
{
    const char *tmp = SDL_getenv("TMPDIR");

    SDL_snprintf(node_dir, sizeof(node_dir), "%s/testwebosuevent.XXXXXX", tmp ? tmp : "/tmp");

    return mkdtemp(node_dir) != NULL;
}

static void AddNode(const char *name)
{
    char path[192];

    SDL_snprintf(path, sizeof(path), "%s/%s", node_dir, name);
    symlink("/dev/null", path);
}

static void RemoveNode(const char *name)
{
    char path[192];

    SDL_snprintf(path, sizeof(path), "%s/%s", node_dir, name);
    unlink(path);
}

static void RemoveNodeDir(void)
{
    DIR *dir = opendir(node_dir);
    struct dirent *entry;

    if (dir != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            RemoveNode(entry->d_name);
        }
        closedir(dir);
    }

    rmdir(node_dir);
}

/* A monitor over that directory, with a file where the socket would be: the
 * queue is drained before anything is read, and the read then fails, which is
 * the same "nothing left" the caller sees on an idle socket. */
static SDL_webOSUeventMonitor monitor;

static void OpenMonitor(void)
{
    SDL_zero(monitor);

    monitor.fd = open("/dev/null", O_RDONLY);
    monitor.base_dir = node_dir;
    monitor.prefix = "event";
}

static void CloseMonitor(void)
{
    if (monitor.fd >= 0) {
        close(monitor.fd);
    }

    SDL_free(monitor.known);
    SDL_free(monitor.queue);
    SDL_zero(monitor);
}

/* Drains into "add:event0 remove:event1", so a case can state the sequence it
 * expects in one string. Enumeration order follows readdir, so only use this
 * where at most one node changed. */
static void Drain(char *out, size_t size)
{
    SDL_webOSUevent event;
    size_t pos = 0;

    out[0] = '\0';

    while (SDL_webOSUeventMonitorPoll(&monitor, &event) && pos + 64 < size) {
        pos += (size_t)SDL_snprintf(out + pos, size - pos, "%s%s:%s", pos > 0 ? " " : "",
                                    event.action == SDL_WEBOS_UEVENT_ACTION_ADD ? "add" : "remove",
                                    event.devname);
    }
}

static int DrainCount(void)
{
    SDL_webOSUevent event;
    int count = 0;

    while (SDL_webOSUeventMonitorPoll(&monitor, &event)) {
        count++;
    }

    return count;
}

static void RunParserTests(void)
{
    char buf[8192];
    SDL_webOSUevent ev;
    size_t len;

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

    /* Why Poll() has to throw away a message the kernel marked MSG_TRUNC
     * rather than parse what arrived: the cut lands inside DEVNAME, what is
     * left of it is still a well-formed field, and event14 comes back out as
     * event1. Nothing later can tell the difference. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/input/input21/event14",
            "ACTION=add",
            "SUBSYSTEM=input",
            "DEVNAME=input/event14",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        len -= 2;
        buf[len] = '\0';
        Check("cut inside DEVNAME reads as another node",
              Parse(buf, len, &ev) && SDL_strcmp(ev.devname, "event1") == 0,
              ev.devname);
    }

    /* Cut inside the header line instead, which leaves no fields at all. The
     * walk starts past the end of the buffer and must not read there. */
    {
        static const char *const fields[] = {
            "add@/devices/platform/soc/usb/input/input21/event14",
            "ACTION=add",
            "SUBSYSTEM=input",
        };
        len = BuildUevent(buf, fields, SDL_arraysize(fields));
        len = 20;
        buf[len] = '\0';
        Check("cut inside the header rejected", !Parse(buf, len, &ev), "accepted");
    }
}

static void RunEnumerationTests(void)
{
    char events[512];
    int drained;

    /* Everything already there comes out as an add, which is how a caller
     * gets the existing device list without a separate startup scan. */
    OpenMonitor();
    AddNode("event0");
    Resynchronize(&monitor);
    Drain(events, sizeof(events));
    Check("existing node enumerated as an add", SDL_strcmp(events, "add:event0") == 0, events);

    /* And a node that has gone while we weren't draining comes out as a
     * remove on the next re-enumeration. */
    RemoveNode("event0");
    Resynchronize(&monitor);
    Drain(events, sizeof(events));
    Check("vanished node enumerated as a remove", SDL_strcmp(events, "remove:event0") == 0, events);
    CloseMonitor();

    /* A remove that cannot be queued must leave the node in the known set.
     * Forgetting it there loses the device for good: no event was delivered,
     * and every later enumeration agrees it is already gone.
     *
     * The node is seeded rather than enumerated so that the queue is still
     * unallocated, which is what makes the first allocation of the resync the
     * one that queues the remove. */
    OpenMonitor();
    MarkKnown(&monitor, "event0");
    allocations_until_failure = 0;
    Resynchronize(&monitor);
    allocations_until_failure = -1;
    drained = DrainCount();
    Check("failed remove leaves the node known",
          drained == 0 && monitor.known_count == 1 &&
              SDL_strcmp(monitor.known[0], "event0") == 0,
          "forgotten");

    /* ... and the retry then reports it, which is the point of keeping it. */
    Resynchronize(&monitor);
    Drain(events, sizeof(events));
    Check("the next enumeration reports it", SDL_strcmp(events, "remove:event0") == 0, events);
    CloseMonitor();

    /* A listing that ran out of memory part way through is not evidence that
     * the nodes it never reached have gone. Reporting the difference against
     * a short listing would remove ten live devices. */
    {
        char name[NODE_NAME_SIZE];
        int i;

        OpenMonitor();

        for (i = 0; i < 10; i++) {
            SDL_snprintf(name, sizeof(name), "event%d", i);
            AddNode(name);
        }

        Resynchronize(&monitor);
        Check("ten nodes enumerated", DrainCount() == 10 && monitor.known_count == 10, "short");

        /* The first allocation takes the listing to eight entries; the second
         * grows it, and is the one that fails. */
        allocations_until_failure = 1;
        Resynchronize(&monitor);
        allocations_until_failure = -1;
        drained = DrainCount();
        Check("truncated listing removes nothing",
              drained == 0 && monitor.known_count == 10, "removed");

        CloseMonitor();

        for (i = 0; i < 10; i++) {
            SDL_snprintf(name, sizeof(name), "event%d", i);
            RemoveNode(name);
        }
    }

    /* A directory that cannot be listed must say so. Returning quietly is
     * indistinguishable from an empty one, and that was silent for as long as
     * it took to read the code. */
    OpenMonitor();
    monitor.base_dir = "/nonexistent/testwebosuevent";
    last_log[0] = '\0';
    SDL_SetLogOutputFunction(CaptureLog, NULL);
    Resynchronize(&monitor);
    SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), NULL);
    Check("unlistable directory is reported",
          SDL_strstr(last_log, "/nonexistent/testwebosuevent") != NULL, last_log);
    CloseMonitor();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_GetOriginalMemoryFunctions(&real_malloc, &real_calloc, &real_realloc, &real_free);
    SDL_SetMemoryFunctions(FailingMalloc, FailingCalloc, FailingRealloc, FailingFree);

    /* The monitor reports through SDL_LogWarn, which is below the default
     * threshold for anything but the application category. */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_VERBOSE);

    RunParserTests();

    if (MakeNodeDir()) {
        RunEnumerationTests();
        RemoveNodeDir();
    } else {
        printf("%-46s %s\n", "enumeration tests", "SKIP (no writable temporary directory)");
    }

    printf("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES PRESENT");

    return failures != 0;
}
