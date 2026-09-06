#include "../../SDL_internal.h"

#ifdef __WEBOS__

#include "SDL_system.h"
#include "SDL_webos_json.h"
#include "SDL_webos_luna.h"
#include "../../video/SDL_sysvideo.h"
#include "../../events/SDL_mouse_c.h"

SDL_bool SDL_webOSGetPanelResolution(int *width, int *height) {
    char *response = NULL;
    SDL_bool result = SDL_FALSE;
    if (SDL_webOSLunaServiceCallSync("luna://com.webos.service.panelcontroller/getPanelResolution", "{}", 1,
                                     &response) && response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, 1)) != NULL) {
            PBNJSON_jnumber_get_i32(PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("width")), width);
            PBNJSON_jnumber_get_i32(PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("height")), height);
            result = SDL_TRUE;
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
    if (!result && SDL_webOSLunaServiceCallSync("luna://com.webos.service.tv.systemproperty/getSystemInfo",
                                     "{\"keys\": [\"UHD\"]}", 1, &response) && response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, 1)) != NULL) {
            jvalue_ref uhd = PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("UHD"));
            int is_uhd = 0;
            if (PBNJSON_jis_string(uhd)) {
                raw_buffer uhd_buf = PBNJSON_jstring_get_fast(uhd);
                is_uhd = SDL_strncmp(uhd_buf.m_str, "true", uhd_buf.m_len) == 0;
            } else {
                PBNJSON_jboolean_get(uhd, &is_uhd);
            }
            if (is_uhd) {
                *width = 3840;
                *height = 2160;
            } else {
                *width = 1920;
                *height = 1080;
            }
            result = SDL_TRUE;
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
    return result;
}

SDL_bool SDL_webOSGetRefreshRate(int *rate) {
    const char *uri = "luna://com.webos.service.config/getConfigs";
    const char *payload = "{\"configNames\":[\"tv.hw.SoCOutputFrameRate\",\"tv.hw.supportFrc\"]}";
    char *response = NULL;
    SDL_bool result = SDL_FALSE;
    if (SDL_webOSLunaServiceCallSync(uri, payload, 1, &response) && response != NULL) {
        jdomparser_ref parser = NULL;
        jvalue_ref parsed;
        if ((parsed = SDL_webOSJsonParse(response, &parser, 1)) != NULL) {
            jvalue_ref configs = PBNJSON_jobject_get(parsed, J_CSTR_TO_BUF("configs"));
            if (PBNJSON_jis_object(configs)) {
                const char *keys[] = {"tv.hw.SoCOutputFrameRate", "tv.hw.supportFrc", NULL};
                for (int i = 0; keys[i] != NULL && !result; i++) {
                    jvalue_ref config = PBNJSON_jobject_get(configs, PBNJSON_j_cstr_to_buffer(keys[i]));
                    switch (i) {
                        case 0: {
                            char value[16];
                            int value_num;
                            raw_buffer config_buf = PBNJSON_jstring_get_fast(config);
                            if (config_buf.m_str == NULL) {
                                continue;
                            }
                            SDL_zeroa(value);
                            SDL_memcpy(value, config_buf.m_str, SDL_min(config_buf.m_len, 15));
                            value_num = SDL_strtol(value, NULL, 10);
                            if (value_num > 0) {
                                *rate = value_num;
                            }
                            result = SDL_TRUE;
                            break;
                        }
                        case 1: {
                            int support_frc = 0;
                            PBNJSON_jboolean_get(config, &support_frc);
                            if (support_frc) {
                                *rate = 120;
                            } else {
                                *rate = 60;
                            }
                            result = SDL_TRUE;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            PBNJSON_jdomparser_release(&parser);
        }
        SDL_free(response);
    }
    return result;
}

#endif // __WEBOS__
