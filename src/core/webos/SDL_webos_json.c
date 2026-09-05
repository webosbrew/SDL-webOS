#include "../../SDL_internal.h"

#ifdef __WEBOS__
#include "SDL_webos_json.h"

jvalue_ref SDL_webOSJsonParse(const char *json, jdomparser_ref *parser, SDL_bool failOnNegative)
{
    jvalue_ref result;
    jdomparser_ref new_parser = NULL;
    JSchemaInfo schemaInfo;
    PBNJSON_jschema_info_init(&schemaInfo, PBNJSON_jschema_all(), NULL, NULL);
    new_parser = PBNJSON_jdomparser_create(&schemaInfo, 0);
    if (new_parser == NULL) {
        return NULL;
    }
    PBNJSON_jdomparser_feed(new_parser, json, (int)strlen(json));
    PBNJSON_jdomparser_end(new_parser);
    result = PBNJSON_jdomparser_get_result(new_parser);
    if (result == NULL) {
        PBNJSON_jdomparser_release(&new_parser);
        return NULL;
    }
    if (failOnNegative) {
        int returnValue = 0;
        PBNJSON_jboolean_get(PBNJSON_jobject_get(result, J_CSTR_TO_BUF("returnValue")), &returnValue);
        if (!returnValue) {
            PBNJSON_jdomparser_release(&new_parser);
            return NULL;
        }
    }
    *parser = new_parser;
    return result;
}

extern const char* SDL_webOSJsonStringify(jvalue_ref value)
{
    if (PBNJSON_jvalue_stringify != NULL) {
        return PBNJSON_jvalue_stringify(value);
    }
    return PBNJSON_jvalue_tostring_simple(value);
}

jvalue_ref PBNJSON_jobject_get_nested(jvalue_ref obj, ...)
{
    va_list iter;
    const char *key;
    va_start(iter, obj);
    while ((key = va_arg(iter, const char *)) != NULL) {
        if (!PBNJSON_jobject_get_exists(obj, PBNJSON_j_cstr_to_buffer(key), &obj)) {
            obj = PBNJSON_jinvalid();
            break;
        }
    }
    va_end(iter);
    return obj;
}

#endif // __WEBOS__
