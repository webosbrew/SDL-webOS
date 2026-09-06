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

#include "../../SDL_internal.h"
#include "../SDL_syslocale.h"

#include "../../core/webos/SDL_webos_json.h"
#include "../../core/webos/SDL_webos_luna.h"

/* Contributed by Mariotaku <mariotaku.lee@gmail.com> */

void SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    const char *uri = "luna://com.webos.settingsservice/getSystemSettings";
    const char *payload = "{\"key\":\"localeInfo\"}";
    char *response = NULL;
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL;
    jvalue_ref locale = NULL;
    if (!SDL_webOSLunaServiceCallSync(uri, payload, 1, &response) || !response) {
        return;
    }
    if ((parsed = SDL_webOSJsonParse(response, &parser, 1)) == NULL) {
        SDL_free(response);
        return;
    }

    locale = PBNJSON_jobject_get_nested(parsed, "settings", "localeInfo", "locales", "UI", NULL);
    if (PBNJSON_jis_string(locale)) {
        raw_buffer locale_buf = PBNJSON_jstring_get_fast(locale);
        if (locale_buf.m_len < buflen) {
            for (size_t i = 0; i < locale_buf.m_len; i++) {
                char ch = locale_buf.m_str[i];
                if (ch == '-') {
                    buf[i] = '_';
                } else {
                    buf[i] = ch;
                }
            }
            buf[locale_buf.m_len] = '\0';
        }
    }
    PBNJSON_jdomparser_release(&parser);
    SDL_free(response);
}

/* vi: set ts=4 sw=4 expandtab: */
