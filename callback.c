
#include <lualib.h>
#include <lauxlib.h>
#include <iot/cJSON.h>
#include <iot/mongoose.h>
#include <iot/iot.h>
#include "mqtt.h"
#include "client.h"

void lua_callback(void *arg, const char *method, const char *data, struct mg_str *out) {
    struct client_private *priv = (struct client_private*)((struct mg_mgr*)arg)->userdata;
    const char *ret = NULL;
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    if ( luaL_dofile(L, priv->cfg.opts->callback_lua) ) {
        MG_ERROR(("lua dofile %s failed", priv->cfg.opts->callback_lua));
        goto done;
    }

    lua_getfield(L, -1, "call");
    if (!lua_isfunction(L, -1)) {
        MG_ERROR(("method gen_rpc_request is not a function"));
        goto done;
    }

    lua_pushstring(L, method);
    lua_pushstring(L, data);

    if (lua_pcall(L, 2, 1, 0)) {//two param, one return values, zero error func
        MG_ERROR(("callback failed"));
        goto done;
    }

    ret = lua_tostring(L, -1);
    if (!ret) {
        MG_ERROR(("lua call no ret"));
        goto done;
    }

    //MG_INFO(("ret: %s", ret));

    //must free by caller
    if (out)
        *out = mg_strdup(mg_str(ret));

done:
    if (L)
        lua_close(L);

}


// cloud mqtt connect/disconnect callback
void cloud_mqtt_event_callback(struct mg_mgr *mgr, const char* event) {
    struct client_private *priv = (struct client_private *)mgr->userdata;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event", event);
    cJSON_AddStringToObject(root, "address", priv->cfg.opts->cloud_mqtt_serve_address);

    char *params = cJSON_Print(root);

    MG_INFO(("callback on_event: %s", params));

    //don't care the return value
    lua_callback(mgr, "on_event", params, NULL);

    free(params);
    cJSON_Delete(root);
}


void local_mqtt_msg_callback(struct mg_connection *c, struct mg_str topic, struct mg_str data) {
    // receive from rpcd
    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    if ( !priv->cloud_mqtt_conn ) {
        MG_DEBUG(("cloud mqtt client not connected"));
        return;
    }

    cJSON *root = cJSON_ParseWithLength(data.ptr, data.len);
    cJSON *code = cJSON_GetObjectItem(root, FIELD_CODE);
    if ( cJSON_IsNumber(code) && cJSON_GetNumberValue(code) == -10405 ) { // no data from lua callback, ignore it, the error code is from iot-rpcd
        cJSON_Delete(root);
        return;
    }
    cJSON_Delete(root);

    struct mg_str pubt = mg_str(priv->cfg.opts->topic_pub);
    struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
    pub_opts.topic = pubt;
    pub_opts.message = data;
    pub_opts.qos = priv->cfg.opts->cloud_mqtt_qos, pub_opts.retain = false;
    mg_mqtt_pub(priv->cloud_mqtt_conn, &pub_opts);
    MG_DEBUG(("pub %.*s -> %.*s", (int) data.len, data.ptr,
        (int) pubt.len, pubt.ptr));

}

/*
{
        "method":       "call",
        "param":        ["plugin/unicom/callback", "handler", {
                        "topic":        "report_timer",   //if topic is report_timer, it's a timer report
                        "to":   "mg/iot-client/channel",  //response reply to. async response
                        "data": {
                                "method":       "call",
                                "param":        ["ubus", "call", {
                                                "object":       "system",
                                                "method":       "board"
                                        }]
                        }
                }]
}
*/

void cloud_mqtt_msg_callback(struct mg_connection *c, struct mg_str topic, struct mg_str data) {
    // receive from cloud mqtt
    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    if ( !priv->mqtt_conn && data.len > 0 ) {
        MG_ERROR(("mqtt client not connected"));
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, FIELD_METHOD, "call");

    cJSON *param = cJSON_CreateArray();
    cJSON_AddItemToArray(param, cJSON_CreateString(priv->cfg.opts->module));
    cJSON_AddItemToArray(param, cJSON_CreateString(priv->cfg.opts->func));

    cJSON *args = cJSON_CreateObject();
    char *s_topic = mg_mprintf("%.*s", (int) topic.len, topic.ptr);
    cJSON_AddItemToObject(args, FIELD_TOPIC, cJSON_CreateString(s_topic));
    free(s_topic);
    cJSON_AddItemToObject(args, FIELD_TO, cJSON_CreateString(IOT_CLIENT_RPCD_TOPIC_PREFIX));
    cJSON *data_obj = cJSON_ParseWithLength(data.ptr, data.len);
    if (data_obj) {
        cJSON_AddItemToObject(args, FIELD_DATA, data_obj);
    } else {
        char *s_data = mg_mprintf("%.*s", (int) data.len, data.ptr);
        cJSON_AddItemToObject(args, FIELD_DATA, cJSON_CreateString(s_data));
        free(s_data);
    }
    cJSON_AddItemToArray(param, args);

    cJSON_AddItemToObject(root, FIELD_PARAM, param);

    char *printed = cJSON_Print(root);

    // send data to iot-rpcd
    struct mg_str pubt = mg_str(IOT_CLIENT_RPCD_TOPIC);
    struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
    pub_opts.topic = pubt;
    pub_opts.message = mg_str(printed);
    pub_opts.qos = MQTT_QOS, pub_opts.retain = false;
    mg_mqtt_pub(priv->mqtt_conn, &pub_opts);
    MG_DEBUG(("pub %s -> %.*s", printed, (int) pubt.len, pubt.ptr));

    cJSON_free(printed);
    cJSON_Delete(root);
}
