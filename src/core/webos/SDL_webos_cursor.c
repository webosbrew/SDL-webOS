#include "../../SDL_internal.h"

#include "SDL_system.h"
#include "../../events/SDL_mouse_c.h"

SDL_bool SDL_webOSCursorVisibility(SDL_bool visible) {
    SDL_Mouse *mouse = SDL_GetMouse();
    if (mouse == NULL) {
        SDL_SetError("Failed to set cursor visibility: No mouse");
        return SDL_FALSE;
    }
    if (mouse->WebOSSetCursorVisibility == NULL) {
        SDL_SetError("Failed to set cursor visibility: Mouse does not support cursor visibility");
        return SDL_FALSE;
    }
    return mouse->WebOSSetCursorVisibility(visible);
}
