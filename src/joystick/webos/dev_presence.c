#include "dev_presence.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

typedef enum
{
    NODE_LIVENESS_UNKNOWN = -1,
    NODE_LIVENESS_ABSENT = 0,
    NODE_LIVENESS_PRESENT = 1
} NodeLiveness;

static NodeLiveness CheckSysDevChar(dev_t devnum);

static NodeLiveness CheckSysClassInput(const char *name);

static NodeLiveness CheckOpen(const char *devpath);

static int is_hidraw(const struct dirent *dir);

static int is_evdev(const struct dirent *dir);

static int is_jsdev(const struct dirent *dir);

Uint32 SDL_webOSGetDevicePresenceFlags(SDL_webOSDevicePresenceCheck check)
{
    Uint32 flags = 0;
    struct dirent **dev_list;
    int dev_dir_fd;
    int dev_count;
    const char *base_dir;
    int (*selector)(const struct dirent *);
    int prefix_len;

    switch (check) {

    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_HIDRAW:
        base_dir = "/dev";
        selector = is_hidraw;
        prefix_len = 6;
        break;
    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_EVDEV:
        base_dir = "/dev/input";
        selector = is_evdev;
        prefix_len = 5;
        break;
    case SDL_WEBOS_DEVICE_PRESENCE_CHECK_JS:
        base_dir = "/dev/input";
        selector = is_jsdev;
        prefix_len = 2;
        break;
    default:
        return 0;
    }

    dev_dir_fd = open(base_dir, O_RDONLY | O_DIRECTORY);

    if (dev_dir_fd < 0) {
        return 0;
    }

    dev_count = scandir(base_dir, &dev_list, selector, alphasort);

    if (dev_count < 0) {
        close(dev_dir_fd);
        return 0;
    }

    for (int dev_idx = 0; dev_idx < dev_count; dev_idx++) {
        struct stat dev_st;
        int ret;
        int dev_num;
        char *endptr = NULL;
        char dev_path[sizeof("/dev/input/") + NAME_MAX];
        ret = fstatat(dev_dir_fd, dev_list[dev_idx]->d_name, &dev_st, 0);
        if (ret != 0 || !S_ISCHR(dev_st.st_mode)) {
            free(dev_list[dev_idx]); /* SHOULD NOT be freed with SDL_free() */
            continue;
        }
        dev_num = strtol(dev_list[dev_idx]->d_name + prefix_len, &endptr, 10);
        if (endptr == NULL || *endptr != '\0') {
            free(dev_list[dev_idx]); /* SHOULD NOT be freed with SDL_free() */
            continue;
        }
        snprintf(dev_path, sizeof(dev_path), "%s/%s", base_dir, dev_list[dev_idx]->d_name);
        if (SDL_webOSIsCharDevicePresent(dev_st.st_rdev, dev_path, dev_list[dev_idx]->d_name)) {
            flags |= 1 << dev_num;
        }
        free(dev_list[dev_idx]); /* SHOULD NOT be freed with SDL_free() */
    }
    free(dev_list); /* SHOULD NOT be freed with SDL_free() */

    close(dev_dir_fd);

    return flags;
}

extern SDL_bool SDL_webOSIsDeviceIndexPresent(Uint32 flags, int index)
{
    return (flags & (1 << index)) != 0;
}

SDL_bool SDL_webOSIsCharDevicePresent(dev_t devnum, const char *devpath, const char *name)
{
    NodeLiveness liveness = CheckSysDevChar(devnum);

    if (liveness == NODE_LIVENESS_UNKNOWN) {
        liveness = CheckSysClassInput(name);
    }

    if (liveness == NODE_LIVENESS_UNKNOWN) {
        liveness = CheckOpen(devpath);
    }

    /* Nothing could answer. Treat the node as real rather than hide a device
     * that is genuinely there: a false present costs one failed open in the
     * caller, a false absent loses the device entirely. */
    return liveness != NODE_LIVENESS_ABSENT;
}

/* Covers every device class, but the app jail mounts /sys/dev only in devmode
 * (jail_native_devmode.conf); the production native and native_game jails have
 * no /sys/dev at all. */
static NodeLiveness CheckSysDevChar(dev_t devnum)
{
    char path[64];
    struct stat st;

    if (stat("/sys/dev/char", &st) != 0) {
        return NODE_LIVENESS_UNKNOWN;
    }

    snprintf(path, sizeof(path), "/sys/dev/char/%u:%u", major(devnum), minor(devnum));

    return stat(path, &st) == 0 ? NODE_LIVENESS_PRESENT : NODE_LIVENESS_ABSENT;
}

/* Mounted in every jail config and lists only live nodes, but there is no
 * /sys/class/hidraw to match it, so this answers for evdev and js only. */
static NodeLiveness CheckSysClassInput(const char *name)
{
    char path[128];
    struct stat st;

    if (name == NULL || stat("/sys/class/input", &st) != 0) {
        return NODE_LIVENESS_UNKNOWN;
    }

    snprintf(path, sizeof(path), "/sys/class/input/%s", name);

    return stat(path, &st) == 0 ? NODE_LIVENESS_PRESENT : NODE_LIVENESS_ABSENT;
}

/* Last resort, and the only one that answers for hidraw in a production jail:
 * an orphan node fails with ENODEV or ENXIO, where a live one either opens or
 * refuses us for a reason that still proves something is behind it. */
static NodeLiveness CheckOpen(const char *devpath)
{
    int fd;

    if (devpath == NULL) {
        return NODE_LIVENESS_UNKNOWN;
    }

    fd = open(devpath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);

    if (fd >= 0) {
        close(fd);
        return NODE_LIVENESS_PRESENT;
    }

    switch (errno) {
    case ENODEV:
    case ENXIO:
    case ENOENT:
        return NODE_LIVENESS_ABSENT;
    case EACCES:
    case EPERM:
    case EBUSY:
        return NODE_LIVENESS_PRESENT;
    default:
        return NODE_LIVENESS_UNKNOWN;
    }
}

int is_hidraw(const struct dirent *dir)
{
    return dir->d_type == DT_CHR && strncmp(dir->d_name, "hidraw", 6) == 0;
}

int is_evdev(const struct dirent *dir)
{
    return dir->d_type == DT_CHR && strncmp(dir->d_name, "event", 5) == 0;
}

int is_jsdev(const struct dirent *dir)
{
    return dir->d_type == DT_CHR && strncmp(dir->d_name, "js", 2) == 0;
}
