#include <iot/mongoose.h>
#include <iot/cJSON.h>
#include <iot/iot.h>
#include "mqtt.h"
#include "client.h"
#include "callback.h"

static int s_signo;
static void signal_handler(int signo) {
    s_signo = signo;
}

/*
{
    code = 0, -- if code !=0, don't send request
    data = {
        method = "call",
        param = {"ubus", "call", {
            object = "system",
            method = "board"
        }}
    }
}
*/
void timer_report_fn(void *arg) {

    struct client_private *priv = (struct client_private*)((struct mg_mgr*)arg)->userdata;
    cJSON *root = NULL;
    char *printed = NULL;
    if (!priv->mqtt_conn) {
        MG_ERROR(("mqtt client not connected"));
        return;
    }

    struct mg_str ret = lua_callback(arg, "gen_request", "");

    if (!ret.ptr) {
        MG_DEBUG(("no report data"));
        return;
    }

    root = cJSON_ParseWithLength(ret.ptr, ret.len);
    cJSON *code = cJSON_GetObjectItem(root, FIELD_CODE);
    cJSON *data = cJSON_GetObjectItem(root, FIELD_DATA);

    if (!cJSON_IsNumber(code) || cJSON_GetNumberValue(code) != 0 || !cJSON_IsObject(data)) {
        MG_DEBUG(("no report data"));
        goto end;
    }

    printed = cJSON_Print(data);

    //simulate from cloud, send report request to iot-rpcd
    report_mqtt_msg_callback(priv->mqtt_conn, mg_str("report_timer"), mg_str(printed));

end:

    if (printed)
        cJSON_free(printed);

    if (root)
        cJSON_Delete(root);

    if (ret.ptr) {
        MG_DEBUG(("ret: %.*s", (int) ret.len, ret.ptr));
        free((void*)ret.ptr);
    }

}

int client_init(void **priv, void *opts) {

    struct client_private *p;
    int timer_opts = MG_TIMER_REPEAT | MG_TIMER_RUN_NOW;

    signal(SIGINT, signal_handler);   // Setup signal handlers - exist event
    signal(SIGTERM, signal_handler);  // manager loop on SIGINT and SIGTERM

    *priv = NULL;
    p = calloc(1, sizeof(struct client_private));
    if (!p)
        return -1;
    
    //生成client id
    char rnd[10];
    mg_random(rnd, sizeof(rnd));
    mg_hex(rnd, sizeof(rnd), p->client_id);
    
    p->cfg.opts = opts;
    mg_log_set(p->cfg.opts->debug_level);

    mg_mgr_init(&p->mgr);
    p->mgr.dnstimeout = p->cfg.opts->dns4_timeout*1000;
    p->mgr.dns4.url = p->cfg.opts->dns4_url;

    p->mgr.userdata = p;

    mg_timer_add(&p->mgr, 1000, timer_opts, timer_mqtt_fn, &p->mgr);
    mg_timer_add(&p->mgr, 1000, timer_opts, timer_cloud_mqtt_fn, &p->mgr);
    mg_timer_add(&p->mgr, 1000, timer_opts, timer_report_fn, &p->mgr);


    *priv = p;

    return 0;

}


void client_run(void *handle) {
    struct client_private *priv = (struct client_private *)handle;
    while (s_signo == 0) mg_mgr_poll(&priv->mgr, 1000);  // Event loop, 1000ms timeout
}

void client_exit(void *handle) {
    struct client_private *priv = (struct client_private *)handle;
    mg_mgr_free(&priv->mgr);
    free(handle);
}

int client_main(void *user_options) {

    struct client_option *opts = (struct client_option *)user_options;
    void *client_handle;
    int ret;

    ret = client_init(&client_handle, opts);
    if (ret)
        exit(EXIT_FAILURE);

    client_run(client_handle);

    client_exit(client_handle);

    return 0;

}
