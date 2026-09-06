#include "uevent_monitor.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SDL_error.h"
#include "SDL_log.h"

/* Big enough for any single uevent; the kernel caps the payload well below
 * this. A short read would only lose the tail of one message, so oversize. */
#define UEVENT_BUF_SIZE 8192

/* Kernel-originated uevents. Group 2 is the udev-processed stream, which
 * nothing produces here without udev running. */
#define UEVENT_GROUP_KERNEL 1

/* "hidraw" plus digits, with room to spare. Node names are far shorter. */
#define NODE_NAME_SIZE 32

typedef struct
{
    Uint8 action;
    char name[NODE_NAME_SIZE];
} QueuedEvent;

struct SDL_webOSUeventMonitor
{
    int fd;

    const char *base_dir; /* "/dev" or "/dev/input" */
    const char *prefix;   /* "hidraw", "event" or "js" */

    /* Nodes the caller has been told about, so a re-enumeration after a
     * dropped-event burst can report the difference rather than repeating
     * the whole device list. */
    char (*known)[NODE_NAME_SIZE];
    int known_count;
    int known_capacity;

    /* Events produced by enumeration, ahead of anything read from the
     * socket. Drained in order, so startup looks like a burst of adds. */
    QueuedEvent *queue;
    int queue_head;
    int queue_count;
    int queue_capacity;

    /* Storage backing the strings handed out by Poll(). */
    char devname[NODE_NAME_SIZE];
    char devnode[NODE_NAME_SIZE + 16];

    char buf[UEVENT_BUF_SIZE];
};

static int OpenUeventSocket(void);

static SDL_bool ParseUevent(char *buf, size_t len, const char **subsystem, const char **devname,
                            SDL_webOSUeventAction *action);

static const char *TrailingName(const char *path);

static SDL_bool NodeNameMatches(const SDL_webOSUeventMonitor *monitor, const char *name);

static SDL_bool IsKnown(const SDL_webOSUeventMonitor *monitor, const char *name);

static void MarkKnown(SDL_webOSUeventMonitor *monitor, const char *name);

static void MarkUnknown(SDL_webOSUeventMonitor *monitor, const char *name);

static SDL_bool QueueEvent(SDL_webOSUeventMonitor *monitor, SDL_webOSUeventAction action, const char *name);

static void Resynchronize(SDL_webOSUeventMonitor *monitor);

static void EmitEvent(SDL_webOSUeventMonitor *monitor, SDL_webOSUeventAction action, const char *name,
                      SDL_webOSUevent *event);

SDL_webOSUeventMonitor *SDL_webOSUeventMonitorOpen(SDL_webOSDevicePresenceCheck watch)
{
    SDL_webOSUeventMonitor *monitor;
    const char *base_dir;
    const char *prefix;
    int fd;

    switch (watch) {
    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_HIDRAW:
        base_dir = "/dev";
        prefix = "hidraw";
        break;
    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV:
        base_dir = "/dev/input";
        prefix = "event";
        break;
    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_JS:
        base_dir = "/dev/input";
        prefix = "js";
        break;
    default:
        SDL_SetError("Unknown device class %d", (int)watch);
        return NULL;
    }

    fd = OpenUeventSocket();

    if (fd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Unable to open netlink uevent socket, falling back to polling: %s",
                    strerror(errno));
        return NULL;
    }

    monitor = (SDL_webOSUeventMonitor *)SDL_calloc(1, sizeof(*monitor));

    if (monitor == NULL) {
        close(fd);
        SDL_OutOfMemory();
        return NULL;
    }

    monitor->fd = fd;
    monitor->base_dir = base_dir;
    monitor->prefix = prefix;

    /* Bound above, enumerated here: anything that appears in between shows up
     * as a uevent we've already started listening for, and the duplicate is
     * absorbed by the known-node set. The reverse order would lose it. */
    Resynchronize(monitor);

    return monitor;
}

void SDL_webOSUeventMonitorClose(SDL_webOSUeventMonitor *monitor)
{
    if (monitor == NULL) {
        return;
    }

    if (monitor->fd >= 0) {
        close(monitor->fd);
    }

    SDL_free(monitor->known);
    SDL_free(monitor->queue);
    SDL_free(monitor);
}

SDL_bool SDL_webOSUeventMonitorPoll(SDL_webOSUeventMonitor *monitor, SDL_webOSUevent *event)
{
    if (monitor == NULL || monitor->fd < 0 || event == NULL) {
        return SDL_FALSE;
    }

    for (;;) {
        struct sockaddr_nl addr;
        struct iovec iov;
        struct msghdr msg;
        const char *subsystem = NULL;
        const char *devname = NULL;
        SDL_webOSUeventAction action;
        ssize_t bytes;

        /* Anything enumeration produced comes first, so a caller draining in
         * a loop sees existing devices before live ones. */
        if (monitor->queue_count > 0) {
            QueuedEvent *queued = &monitor->queue[monitor->queue_head];

            monitor->queue_head++;
            monitor->queue_count--;

            if (monitor->queue_count == 0) {
                monitor->queue_head = 0;
            }

            EmitEvent(monitor, (SDL_webOSUeventAction)queued->action, queued->name, event);
            return SDL_TRUE;
        }

        iov.iov_base = monitor->buf;
        /* Leave room to terminate the buffer, so parsing can't run past it
         * if the kernel ever hands us an unterminated final field. */
        iov.iov_len = sizeof(monitor->buf) - 1;

        SDL_zero(addr);
        SDL_zero(msg);
        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        bytes = recvmsg(monitor->fd, &msg, MSG_DONTWAIT);

        if (bytes <= 0) {
            if (bytes < 0 && errno == EINTR) {
                continue;
            }

            if (bytes < 0 && errno == ENOBUFS) {
                /* The kernel discarded broadcasts because our buffer filled
                 * up. What it dropped is gone, so rebuild from the device
                 * tree and report the difference; that supersedes anything
                 * still queued in the socket, and returning to the top of the
                 * loop hands the caller the resulting events. */
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                            "Dropped uevents (socket buffer overflow), re-enumerating %s/%s*",
                            monitor->base_dir, monitor->prefix);
                Resynchronize(monitor);
                continue;
            }

            /* EAGAIN/EWOULDBLOCK: drained, which is the usual way out. */
            return SDL_FALSE;
        }

        /* Only the kernel may hotplug devices. Any other process can bind a
         * netlink socket and send us a unicast message, so drop anything
         * that isn't from portid 0 and addressed to a multicast group. */
        if (msg.msg_namelen != sizeof(addr) || addr.nl_pid != 0 || addr.nl_groups == 0) {
            continue;
        }

        /* The tail is gone, and a cut-off field would parse as a complete
         * one, so drop the whole message rather than half-read it. */
        if (msg.msg_flags & MSG_TRUNC) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Discarding truncated uevent (%d bytes)", (int)bytes);
            continue;
        }

        monitor->buf[bytes] = '\0';

        if (!ParseUevent(monitor->buf, (size_t)bytes, &subsystem, &devname, &action)) {
            continue;
        }

        if (devname == NULL || !NodeNameMatches(monitor, devname)) {
            continue;
        }

        /* Collapse anything that doesn't change what the caller believes.
         * The kernel can emit more than one event for a node, and after a
         * re-enumeration we may still have the original uevent queued. */
        if (action == SDL_WEBOS_UEVENT_ACTION_ADD) {
            if (IsKnown(monitor, devname)) {
                continue;
            }
            MarkKnown(monitor, devname);
        } else {
            if (!IsKnown(monitor, devname)) {
                continue;
            }
            MarkUnknown(monitor, devname);
        }

        EmitEvent(monitor, action, devname, event);
        return SDL_TRUE;
    }
}

/* Lists the nodes currently present and queues the difference against what
 * the caller has already been told, as add/remove events. Used for the
 * initial enumeration (where everything is new) and to recover from dropped
 * events (where usually nothing is). */
static void Resynchronize(SDL_webOSUeventMonitor *monitor)
{
    char (*present)[NODE_NAME_SIZE] = NULL;
    int present_count = 0;
    int present_capacity = 0;
    DIR *dir;
    struct dirent *entry;
    int i;

    dir = opendir(monitor->base_dir);

    if (dir == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to enumerate %s: %s",
                    monitor->base_dir, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char path[NODE_NAME_SIZE + 16];

        if (!NodeNameMatches(monitor, entry->d_name)) {
            continue;
        }

        SDL_snprintf(path, sizeof(path), "%s/%s", monitor->base_dir, entry->d_name);

        if (stat(path, &st) != 0 || !S_ISCHR(st.st_mode)) {
            continue;
        }

        /* The inode alone proves nothing: webOS ships every node in this
         * range from boot, live or not. Seeding a dead one into the known
         * set would collapse the real add when a device finally appears on
         * that index, and nothing would re-announce it. */
        if (!SDL_webOSIsCharDevicePresent(st.st_rdev, path, entry->d_name)) {
            continue;
        }

        if (present_count == present_capacity) {
            int capacity = present_capacity ? present_capacity * 2 : 8;
            void *resized = SDL_realloc(present, (size_t)capacity * NODE_NAME_SIZE);

            if (resized == NULL) {
                /* A short listing would read as devices having gone away, so
                 * abandon the resync rather than emit removes for nodes that
                 * are still there. */
                closedir(dir);
                SDL_free(present);
                return;
            }

            present = (char(*)[NODE_NAME_SIZE])resized;
            present_capacity = capacity;
        }

        SDL_strlcpy(present[present_count], entry->d_name, NODE_NAME_SIZE);
        present_count++;
    }

    closedir(dir);

    /* Gone: known but no longer on disk. Walk backwards, since removing from
     * the known set swaps the tail into the current slot. */
    for (i = monitor->known_count - 1; i >= 0; i--) {
        SDL_bool still_there = SDL_FALSE;
        int j;

        for (j = 0; j < present_count; j++) {
            if (SDL_strcmp(monitor->known[i], present[j]) == 0) {
                still_there = SDL_TRUE;
                break;
            }
        }

        if (!still_there) {
            char name[NODE_NAME_SIZE];

            SDL_strlcpy(name, monitor->known[i], sizeof(name));
            if (QueueEvent(monitor, SDL_WEBOS_UEVENT_ACTION_REMOVE, name)) {
                MarkUnknown(monitor, name);
            }
        }
    }

    /* New: on disk but not yet reported. */
    for (i = 0; i < present_count; i++) {
        if (!IsKnown(monitor, present[i])) {
            if (QueueEvent(monitor, SDL_WEBOS_UEVENT_ACTION_ADD, present[i])) {
                MarkKnown(monitor, present[i]);
            }
        }
    }

    SDL_free(present);
}

static void EmitEvent(SDL_webOSUeventMonitor *monitor, SDL_webOSUeventAction action, const char *name,
                      SDL_webOSUevent *event)
{
    SDL_strlcpy(monitor->devname, name, sizeof(monitor->devname));
    SDL_snprintf(monitor->devnode, sizeof(monitor->devnode), "%s/%s", monitor->base_dir, name);

    event->action = action;
    event->devname = monitor->devname;
    event->devnode = monitor->devnode;
}

/* True for exactly the monitor's node class: its prefix followed by digits,
 * so an "event" monitor doesn't pick up "mice", and a "js" monitor doesn't
 * pick up anything else that happens to start with those letters. */
static SDL_bool NodeNameMatches(const SDL_webOSUeventMonitor *monitor, const char *name)
{
    size_t prefix_len = SDL_strlen(monitor->prefix);
    const char *suffix;

    if (SDL_strncmp(name, monitor->prefix, prefix_len) != 0) {
        return SDL_FALSE;
    }

    suffix = name + prefix_len;

    if (*suffix == '\0' || SDL_strlen(name) >= NODE_NAME_SIZE) {
        return SDL_FALSE;
    }

    for (; *suffix != '\0'; suffix++) {
        if (*suffix < '0' || *suffix > '9') {
            return SDL_FALSE;
        }
    }

    return SDL_TRUE;
}

static SDL_bool IsKnown(const SDL_webOSUeventMonitor *monitor, const char *name)
{
    int i;

    for (i = 0; i < monitor->known_count; i++) {
        if (SDL_strcmp(monitor->known[i], name) == 0) {
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

static void MarkKnown(SDL_webOSUeventMonitor *monitor, const char *name)
{
    if (IsKnown(monitor, name)) {
        return;
    }

    if (monitor->known_count == monitor->known_capacity) {
        int capacity = monitor->known_capacity ? monitor->known_capacity * 2 : 8;
        void *resized = SDL_realloc(monitor->known, (size_t)capacity * NODE_NAME_SIZE);

        if (resized == NULL) {
            return;
        }

        monitor->known = (char(*)[NODE_NAME_SIZE])resized;
        monitor->known_capacity = capacity;
    }

    SDL_strlcpy(monitor->known[monitor->known_count], name, NODE_NAME_SIZE);
    monitor->known_count++;
}

static void MarkUnknown(SDL_webOSUeventMonitor *monitor, const char *name)
{
    int i;

    for (i = 0; i < monitor->known_count; i++) {
        if (SDL_strcmp(monitor->known[i], name) == 0) {
            SDL_strlcpy(monitor->known[i], monitor->known[monitor->known_count - 1], NODE_NAME_SIZE);
            monitor->known_count--;
            return;
        }
    }
}

static SDL_bool QueueEvent(SDL_webOSUeventMonitor *monitor, SDL_webOSUeventAction action, const char *name)
{
    QueuedEvent *slot;

    if (monitor->queue_head + monitor->queue_count == monitor->queue_capacity) {
        if (monitor->queue_head > 0) {
            /* Reclaim the drained prefix before growing. */
            SDL_memmove(monitor->queue, &monitor->queue[monitor->queue_head],
                        (size_t)monitor->queue_count * sizeof(*monitor->queue));
            monitor->queue_head = 0;
        } else {
            int capacity = monitor->queue_capacity ? monitor->queue_capacity * 2 : 8;
            void *resized = SDL_realloc(monitor->queue, (size_t)capacity * sizeof(*monitor->queue));

            if (resized == NULL) {
                return SDL_FALSE;
            }

            monitor->queue = (QueuedEvent *)resized;
            monitor->queue_capacity = capacity;
        }
    }

    slot = &monitor->queue[monitor->queue_head + monitor->queue_count];
    slot->action = (Uint8)action;
    SDL_strlcpy(slot->name, name, sizeof(slot->name));
    monitor->queue_count++;

    return SDL_TRUE;
}

static int OpenUeventSocket(void)
{
    struct sockaddr_nl addr;
    int fd;
    int rcvbuf = 1024 * 1024;

    fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);

    if (fd < 0 && (errno == EINVAL || errno == EPROTONOSUPPORT)) {
        /* Older kernels reject the socket type flags; set them separately. */
        fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

        if (fd >= 0) {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
                fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                close(fd);
                return -1;
            }
        }
    }

    if (fd < 0) {
        return -1;
    }

    /* A burst of uevents (a wireless receiver enumerating several interfaces)
     * can outrun us between detect ticks, and an overflowing netlink socket
     * drops messages. Best effort; overflow is handled either way. */
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    SDL_zero(addr);
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = UEVENT_GROUP_KERNEL;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

/* A kernel uevent is "ACTION@DEVPATH" followed by NUL-separated KEY=VALUE
 * fields, e.g.
 *
 *   "add@/devices/.../input/input21/event14\0"
 *   "ACTION=add\0DEVPATH=/devices/.../event14\0SUBSYSTEM=input\0"
 *   "DEVNAME=input/event14\0MAJOR=13\0MINOR=78\0..."
 *
 * Returns SDL_FALSE for anything we can't turn into an add/remove, which
 * includes the "change"/"bind"/"move" actions we have no use for. */
static SDL_bool ParseUevent(char *buf, size_t len, const char **subsystem, const char **devname,
                            SDL_webOSUeventAction *action)
{
    const char *devpath = NULL;
    const char *node = NULL;
    SDL_bool have_action = SDL_FALSE;
    size_t pos;

    /* libudev's own broadcasts carry a magic prefix instead of the header
     * line above. We bind the kernel group so they shouldn't reach us. */
    if (len >= 8 && SDL_memcmp(buf, "libudev", 8) == 0) {
        return SDL_FALSE;
    }

    *subsystem = NULL;
    *devname = NULL;

    /* Skip the header line; ACTION= and DEVPATH= repeat it as proper fields. */
    pos = SDL_strlen(buf) + 1;

    while (pos < len) {
        const char *field = &buf[pos];
        size_t field_len = SDL_strlen(field);

        if (SDL_strncmp(field, "ACTION=", 7) == 0) {
            const char *value = field + 7;
            if (SDL_strcmp(value, "add") == 0) {
                *action = SDL_WEBOS_UEVENT_ACTION_ADD;
            } else if (SDL_strcmp(value, "remove") == 0) {
                *action = SDL_WEBOS_UEVENT_ACTION_REMOVE;
            } else {
                return SDL_FALSE;
            }
            have_action = SDL_TRUE;
        } else if (SDL_strncmp(field, "SUBSYSTEM=", 10) == 0) {
            *subsystem = field + 10;
        } else if (SDL_strncmp(field, "DEVPATH=", 8) == 0) {
            devpath = field + 8;
        } else if (SDL_strncmp(field, "DEVNAME=", 8) == 0) {
            /* Present whenever the event describes an actual device node,
             * and more trustworthy than the sysfs path, which for some
             * subsystems ends in the parent rather than the node. */
            node = field + 8;
        }

        pos += field_len + 1;
    }

    if (!have_action) {
        return SDL_FALSE;
    }

    /* DEVNAME arrives relative to /dev and may be nested ("input/event14"),
     * so reduce either source to the trailing component. A remove event on
     * an older kernel can omit DEVNAME, hence the DEVPATH fallback. */
    if (node != NULL) {
        *devname = TrailingName(node);
    } else if (devpath != NULL) {
        *devname = TrailingName(devpath);
    }

    return SDL_TRUE;
}

static const char *TrailingName(const char *path)
{
    const char *slash = SDL_strrchr(path, '/');
    const char *name = slash != NULL ? slash + 1 : path;

    return *name != '\0' ? name : NULL;
}
