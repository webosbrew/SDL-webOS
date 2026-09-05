#include "../../SDL_internal.h"

#ifdef __WEBOS__
#include "../../events/SDL_events_c.h"
#include "../../video/SDL_sysvideo.h"
#include "SDL_error.h"
#include "SDL_hints.h"
#include "SDL_system.h"
#include "SDL_webos_init.h"
#include "SDL_webos_json.h"
#include "SDL_webos_libs.h"
#include "SDL_webos_luna.h"

static SDL_bool s_appRegistered = SDL_FALSE;
static int s_nativeLifeCycleInterfaceVersion = 0;
static LSHandle *s_LSHandle = NULL;
static SDL_webOSPowerState s_powerState = SDL_WEBOS_POWER_STATE_UNKNOWN;

static int getNativeLifeCycleInterfaceVersion(const char *appId);

static SDL_bool registerApp(const char *appId, int interfaceVersion);
static int lifecycleCallbackVersion1(LSHandle *sh, LSMessage *reply, HContext *ctx);
static int lifecycleCallbackVersion2(LSHandle *sh, LSMessage *reply, HContext *ctx);

static SDL_bool registerScreenSaverRequest(const char *appId);
static int screenSaverRequestCallback(LSHandle *sh, LSMessage *reply, HContext *ctx);

static SDL_bool registerPowerState();
static int powerStateCallback(LSHandle *sh, LSMessage *reply, HContext *ctx);

static SDL_bool turnOnScreen();

static HContext s_AppLifecycleContext = {
    .multiple = 1,
    .pub = 1,
};

static HContext s_ScreenSaverRequestContext = {
    .multiple = 1,
    .pub = 1,
};

static HContext s_PowerStateContext = {
    .multiple = 1,
    .pub = 1,
};

SDL_bool SDL_webOSSetLSHandle(LSHandle *handle)
{
    s_LSHandle = handle;
    return HELPERS_HNDLSetLSHandle != NULL;
}

void SDL_webOSInitLSHandle()
{
    if (HELPERS_HNDLSetLSHandle != NULL && s_LSHandle != NULL) {
        HELPERS_HNDLSetLSHandle(s_LSHandle);
    }
}

SDL_bool SDL_webOSAppRegistered()
{
    return s_appRegistered;
}

int SDL_webOSRegisterApp()
{
    if (s_appRegistered) {
        return 0;
    }
    if (SDL_GetHintBoolean(SDL_HINT_WEBOS_REGISTER_APP, SDL_TRUE)) {
        const char *appId = SDL_getenv("APPID");
        if (appId == NULL) {
            return SDL_SetError("APPID environment variable is not set");
        }
        s_nativeLifeCycleInterfaceVersion = getNativeLifeCycleInterfaceVersion(appId);
        if (s_nativeLifeCycleInterfaceVersion == -1) {
            char errbuf[1024];
            SDL_GetErrorMsg(errbuf, 1024);
            return SDL_SetError("Failed to get nativeLifeCycleInterfaceVersion: %s", errbuf);
        }
        if (!registerApp(appId, s_nativeLifeCycleInterfaceVersion)) {
            return -1;
        }
        if (!registerScreenSaverRequest(appId)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to register screen saver request");
        }
        if (!registerPowerState()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to register power state");
        }
    }
    s_appRegistered = SDL_TRUE;
    return 0;
}

void SDL_webOSUnregisterApp()
{
    if (!s_appRegistered || !HELPERS_HUnregisterServiceCallback) {
        s_appRegistered = SDL_FALSE;
        return;
    }
    if (s_AppLifecycleContext.callback != NULL) {
        HELPERS_HUnregisterServiceCallback(&s_AppLifecycleContext);
        s_AppLifecycleContext.callback = NULL;
    }
    if (s_ScreenSaverRequestContext.callback != NULL) {
        HELPERS_HUnregisterServiceCallback(&s_ScreenSaverRequestContext);
        if (s_ScreenSaverRequestContext.userdata) {
            SDL_free(s_ScreenSaverRequestContext.userdata);
        }
        s_ScreenSaverRequestContext.callback = NULL;
    }
    if (s_PowerStateContext.callback != NULL) {
        HELPERS_HUnregisterServiceCallback(&s_PowerStateContext);
        s_PowerStateContext.callback = NULL;
    }
    s_appRegistered = SDL_FALSE;
}

SDL_webOSPowerState SDL_webOSGetPowerState()
{
    return s_powerState;
}

void SDL_webOSTurnOnScreen()
{
    if (s_powerState == SDL_WEBOS_POWER_STATE_ACTIVE) {
        return;
    }
    if (!turnOnScreen()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to turn on screen");
    }
}

int getNativeLifeCycleInterfaceVersion(const char *appId)
{
    char payload[200];
    char *output = NULL;
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL, appInfo = NULL, versionValue = NULL;
    int version = 0;

    snprintf(payload, sizeof(payload), "{\"id\":\"%s\"}", appId);
    if (!SDL_webOSLunaServiceCallSync("luna://com.webos.applicationManager/getAppInfo", payload, 1, &output)) {
        return SDL_SetError("Failed to call luna://com.webos.applicationManager/getAppInfo");
    }
    if (output == NULL) {
        return SDL_SetError("Call to luna://com.webos.applicationManager/getAppInfo didn't return any output");
    }
    parsed = SDL_webOSJsonParse(output, &parser, 1);
    if (parsed == NULL) {
        free(output);
        return SDL_SetError("Failed to parse output of luna://com.webos.applicationManager/getAppInfo");
    }

    if (PBNJSON_jobject_get_exists(parsed, J_CSTR_TO_BUF("appInfo"), &appInfo)) {
        if (PBNJSON_jobject_get_exists(appInfo, J_CSTR_TO_BUF("nativeLifeCycleInterfaceVersion"), &versionValue)) {
            PBNJSON_jnumber_get_i32(versionValue, &version);
        }
    }
    if (version == 0) {
        version = 1;
    }
    PBNJSON_jdomparser_release(&parser);
    free(output);
    return version;
}

static SDL_bool registerApp(const char *appId, int interfaceVersion)
{
    const char *uri;
    char payload[200];
    int callRet;

    snprintf(payload, sizeof(payload), "{\"id\":\"%s\"}", appId);
    if (interfaceVersion == 1) {
        s_AppLifecycleContext.callback = lifecycleCallbackVersion1;
        uri = "luna://com.webos.applicationManager/registerNativeApp";
    } else if (interfaceVersion == 2) {
        s_AppLifecycleContext.callback = lifecycleCallbackVersion2;
        uri = "luna://com.webos.applicationManager/registerApp";
    } else {
        SDL_SetError("Unsupported nativeLifeCycleInterfaceVersion: %d", interfaceVersion);
        return SDL_FALSE;
    }
    if ((callRet = HELPERS_HLunaServiceCall(uri, payload, &s_AppLifecycleContext)) != 0) {
        SDL_SetError("Failed to call %s: (%d) %s", uri, callRet, HELPERS_HGetError(callRet));
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static int lifecycleCallbackVersion1(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL;
    jvalue_ref message = NULL;
    SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "lifecycleEvent(v1): %s", HELPERS_HLunaServiceMessage(reply));
    if ((parsed = SDL_webOSJsonParse(HELPERS_HLunaServiceMessage(reply), &parser, 1)) == NULL) {
        return 0;
    }
    if (PBNJSON_jobject_get_exists(parsed, J_CSTR_TO_BUF("message"), &message)) {
        raw_buffer message_buf = PBNJSON_jstring_get_fast(message);
        if (message_buf.m_str) {
            if (SDL_strncmp(message_buf.m_str, "relaunch", message_buf.m_len) == 0) {
                SDL_VideoDevice *device = SDL_GetVideoDevice();
                if (device->windows) {
                    SDL_RaiseWindow(device->windows);
                }
            }
        }
    }
    PBNJSON_jdomparser_release(&parser);
    return 1;
}
static int lifecycleCallbackVersion2(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL;
    jvalue_ref message = NULL;
    SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "lifecycleEvent(v2): %s", HELPERS_HLunaServiceMessage(reply));
    if ((parsed = SDL_webOSJsonParse(HELPERS_HLunaServiceMessage(reply), &parser, 1)) == NULL) {
        return 0;
    }
    if (PBNJSON_jobject_get_exists(parsed, J_CSTR_TO_BUF("event"), &message)) {
        raw_buffer message_buf = PBNJSON_jstring_get_fast(message);
        if (message_buf.m_str) {
            if (SDL_strncmp(message_buf.m_str, "relaunch", message_buf.m_len) == 0) {
                SDL_VideoDevice *device = SDL_GetVideoDevice();
                if (device->windows) {
                    SDL_RaiseWindow(device->windows);
                }
            } else if (SDL_strncmp(message_buf.m_str, "close", message_buf.m_len) == 0) {
                SDL_SendQuit();
            }
        }
    }
    PBNJSON_jdomparser_release(&parser);
    return 1;
}

static SDL_bool registerScreenSaverRequest(const char *appId)
{
    jvalue_ref payload;
    char *client_name;
    SDL_bool result;

    client_name = SDL_malloc(64);
    SDL_snprintf(client_name, 64, "%s.wakelock", appId);
    payload = PBNJSON_jobject_create_var(
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("subscribe"), PBNJSON_jboolean_create(1)),
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("clientName"), PBNJSON_j_cstr_to_jval(client_name)),
        NULL);
    s_ScreenSaverRequestContext.userdata = client_name;
    s_ScreenSaverRequestContext.callback = screenSaverRequestCallback;

    result = HELPERS_HLunaServiceCall("luna://com.webos.service.tvpower/power/registerScreenSaverRequest",
                                      SDL_webOSJsonStringify(payload), &s_ScreenSaverRequestContext) == 0;

    PBNJSON_j_release(&payload);
    return result;
}

static int screenSaverRequestCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    SDL_VideoDevice *device;
    const char *message;
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL;
    jvalue_ref timestamp = NULL;
    jvalue_ref response;

    (void) sh;
    (void) ctx;

    device = SDL_GetVideoDevice();
    if (device == NULL) {
        return 1;
    }

    message = HELPERS_HLunaServiceMessage(reply);
    SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Screen saver request: %s", message);
    if ((parsed = SDL_webOSJsonParse(message, &parser, 1)) == NULL) {
        return 0;
    }

    if (!PBNJSON_jobject_get_exists(parsed, J_CSTR_TO_BUF("timestamp"), &timestamp)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_SYSTEM, "Skip invalid screensaver request (no timestamp)");
        PBNJSON_jdomparser_release(&parser);
        return 1;
    }
    response = PBNJSON_jobject_create_var(
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("clientName"), PBNJSON_j_cstr_to_jval((const char *)ctx->userdata)),
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("ack"), PBNJSON_jboolean_create(!device->suspend_screensaver)),
        PBNJSON_jkeyval(J_CSTR_TO_JVAL("timestamp"), timestamp),
        NULL);

    SDL_webOSLunaServiceJustCall("luna://com.webos.service.tvpower/power/responseScreenSaverRequest",
                                 SDL_webOSJsonStringify(response), 1);
    PBNJSON_j_release(&response);
    PBNJSON_jdomparser_release(&parser);
    return 1;
}

static SDL_bool registerPowerState()
{
    s_PowerStateContext.callback = powerStateCallback;
    return HELPERS_HLunaServiceCall("luna://com.webos.service.tvpower/power/getPowerState", "{\"subscribe\":true}",
                                    &s_PowerStateContext) == 0;
}

static int powerStateCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    jdomparser_ref parser = NULL;
    jvalue_ref parsed = NULL;
    jvalue_ref state = NULL;
    if ((parsed = SDL_webOSJsonParse(HELPERS_HLunaServiceMessage(reply), &parser, 1)) == NULL) {
        return 0;
    }
    if (PBNJSON_jobject_get_exists(parsed, J_CSTR_TO_BUF("state"), &state)) {
        raw_buffer state_buf = PBNJSON_jstring_get_fast(state);
        if (state_buf.m_str) {
            if (SDL_strncmp(state_buf.m_str, "Active", state_buf.m_len) == 0) {
                s_powerState = SDL_WEBOS_POWER_STATE_ACTIVE;
            } else if (SDL_strncmp(state_buf.m_str, "Screen Saver", state_buf.m_len) == 0) {
                s_powerState = SDL_WEBOS_POWER_STATE_SCREEN_SAVER;
            } else if (SDL_strncmp(state_buf.m_str, "Screen Off", state_buf.m_len) == 0) {
                s_powerState = SDL_WEBOS_POWER_STATE_POWER_OFF;
            } else {
                s_powerState = SDL_WEBOS_POWER_STATE_UNKNOWN;
            }
        }
    }
    PBNJSON_jdomparser_release(&parser);
    return 1;
}

static SDL_bool turnOnScreen()
{
    char *output = NULL;
    if (!SDL_webOSLunaServiceCallSync("luna://com.webos.service.tvpower/power/turnOnScreen", "{}", 1, &output)) {
        return SDL_FALSE;
    }
    free(output);
    return SDL_TRUE;
}

#endif // __WEBOS__
