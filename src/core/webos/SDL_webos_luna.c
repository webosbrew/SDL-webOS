#include "../../SDL_internal.h"

#ifdef __WEBOS__
#include "SDL_hints.h"
#include "SDL_mutex.h"
#include "SDL_webos_libs.h"
#include "SDL_webos_luna.h"

typedef struct SyncUserdata
{
    SDL_mutex *mutex;
    SDL_cond *cond;
    SDL_bool finished;
    char **output;
} SyncUserdata;

static int syncCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx);

static int justCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx);

extern SDL_bool SDL_webOSLunaServiceJustCall(const char* uri, const char* payload, int pub)
{
    HContext *response_context = SDL_calloc(1, sizeof(HContext));
    response_context->multiple = 0;
    response_context->pub = pub;
    response_context->callback = justCallCallback;
    return HELPERS_HLunaServiceCall(uri, payload, response_context) == 0;
}

SDL_bool SDL_webOSLunaServiceCallSync(const char *uri, const char *payload, int pub, char **output)
{
    SyncUserdata userdata = {
        .mutex = SDL_CreateMutex(),
        .cond = SDL_CreateCond(),
        .output = output,
    };
    HContext context = {
        .multiple = 0,
        .pub = pub ? 1 : 0,
        .callback = syncCallCallback,
        .userdata = &userdata,
    };
    int callRet;

    if (!HELPERS_HLunaServiceCall) {
        SDL_SetError("webOS libraries are not initialized");
        return SDL_FALSE;
    }
    if (SDL_GetHintBoolean("SDL_WEBOS_DISABLE_LUNA_CALLS", SDL_FALSE)) {
        SDL_DestroyMutex(userdata.mutex);
        SDL_DestroyCond(userdata.cond);
        SDL_SetError("Disabled by SDL_WEBOS_DISABLE_LUNA_CALLS");
        return SDL_FALSE;
    }

    if ((callRet = HELPERS_HLunaServiceCall(uri, payload, &context)) != 0) {
        SDL_DestroyMutex(userdata.mutex);
        SDL_DestroyCond(userdata.cond);
        SDL_SetError("Failed to call %s: (%d) %s", uri, callRet, HELPERS_HGetError(callRet));
        return SDL_FALSE;
    }
    SDL_LockMutex(userdata.mutex);
    while (!userdata.finished) {
        SDL_CondWait(userdata.cond, userdata.mutex);
    }
    SDL_UnlockMutex(userdata.mutex);

    SDL_DestroyMutex(userdata.mutex);
    SDL_DestroyCond(userdata.cond);
    SDL_ClearError();
    return SDL_TRUE;
}

static int syncCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    SyncUserdata *userdata = ctx->userdata;
    (void)sh;
    SDL_LockMutex(userdata->mutex);
    userdata->finished = SDL_TRUE;
    if (userdata->output) {
        *userdata->output = SDL_strdup(HELPERS_HLunaServiceMessage(reply));
    }
    SDL_CondSignal(userdata->cond);
    SDL_UnlockMutex(userdata->mutex);
    return 0;
}

static int justCallCallback(LSHandle *sh, LSMessage *reply, HContext *ctx)
{
    (void) sh;
    (void) reply;
    SDL_free(ctx);
    return 1;
}

#endif // __WEBOS__
