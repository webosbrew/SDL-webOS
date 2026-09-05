/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#include "../SDL_sysurl.h"

#include "../../core/webos/SDL_webos_json.h"
#include "../../core/webos/SDL_webos_luna.h"

int SDL_SYS_OpenURL(const char *url)
{
    SDL_bool ret;
    jvalue_ref payload_obj = PBNJSON_jobject_create_var(
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("id"), J_CSTR_TO_JVAL("com.webos.app.browser")),
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("params"), PBNJSON_jobject_create_var(
                                              PBNJSON_jkeyval(J_CSTR_TO_JVAL("target"), PBNJSON_j_cstr_to_jval(url)),
                                              J_END_OBJ_DECL)),
        J_END_OBJ_DECL);
    const char *payload = SDL_webOSJsonStringify(payload_obj);
    ret = SDL_webOSLunaServiceCallSync("luna://com.webos.applicationManager/launch", payload, 1, NULL);
    PBNJSON_j_release(&payload_obj);
    if (!ret) {
        return SDL_SetError("Failed to open URL: %s", url);
    }
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
