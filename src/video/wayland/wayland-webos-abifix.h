#ifndef WAYLAND_WEBOS_ABIFIX_H
#define WAYLAND_WEBOS_ABIFIX_H

struct wl_webos_abifix_method_entry {
    const char *name;
    int opcode;
};

struct wl_webos_abifix {
    const struct wl_interface *interface;
    int method_count;
    struct wl_webos_abifix_method_entry *methods;
};

extern struct wl_webos_abifix wl_webos_input_manager_abifix;

#endif /* WAYLAND_WEBOS_ABIFIX_H */
