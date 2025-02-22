#include <iot/cJSON.h>
#include <iot/mongoose.h>
#include <iot/iot.h>
#include "mqtt.h"
#include "client.h"
#include "callback.h"

static void mqtt_ev_open_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MG_INFO(("mqtt client connection created"));
}

static void mqtt_ev_connect_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MG_INFO(("mqtt client connection connected"));
}

static void mqtt_ev_error_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MG_ERROR(("%p %s", c->fd, (char *) ev_data));
    c->is_closing = 1;
}

static void mqtt_ev_poll_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    if (!priv->cfg.opts->mqtt_keepalive) //no keepalive
        return;

    uint64_t now = mg_millis();

    if (priv->pong_active && now > priv->pong_active &&
        now - priv->pong_active > (priv->cfg.opts->mqtt_keepalive + 3)*1000) {
        MG_INFO(("mqtt client connction timeout"));
        c->is_draining = 1;
    }

}

static void mqtt_ev_close_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    MG_INFO(("mqtt client connection closed"));
    priv->mqtt_conn = NULL; // Mark that we're closed

}


static void mqtt_ev_mqtt_open_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;

    // MQTT connect is successful
    struct mg_str subt = mg_str(IOT_CLIENT_TOPIC); 

    MG_INFO(("connect to mqtt server: %s", priv->cfg.opts->mqtt_serve_address));
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    sub_opts.topic = subt;
    sub_opts.qos = MQTT_QOS;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("subscribed to %.*s", (int) subt.len, subt.ptr));

}

static void mqtt_ev_mqtt_cmd_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    struct client_private *priv = (struct client_private*)c->mgr->userdata;

    if (mm->cmd == MQTT_CMD_PINGRESP) {
        priv->pong_active = mg_millis();
    }
}


static void mqtt_ev_mqtt_msg_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    MG_DEBUG(("received %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,
        (int) mm->topic.len, mm->topic.ptr));

    // handle msg from iot-rpcd, send to cloud mqtt server
    local_mqtt_msg_callback(c, mm->topic, mm->data);

}

static void mqtt_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    switch (ev) {
        case MG_EV_OPEN:
            mqtt_ev_open_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_CONNECT:
            mqtt_ev_connect_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_ERROR:
            mqtt_ev_error_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_OPEN:
            mqtt_ev_mqtt_open_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_CMD:
            mqtt_ev_mqtt_cmd_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_MSG:
            mqtt_ev_mqtt_msg_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_POLL:
            mqtt_ev_poll_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_CLOSE:
            mqtt_ev_close_cb(c, ev, ev_data, fn_data);
            break;
    }
}

// Timer function - recreate client connection if it is closed
void timer_mqtt_fn(void *arg) {
    struct mg_mgr *mgr = (struct mg_mgr *)arg;
    struct client_private *priv = (struct client_private*)mgr->userdata;
    uint64_t now = mg_millis();

    if (priv->mqtt_conn == NULL) {
        struct mg_mqtt_opts opts = { 0 };

        opts.clean = true;
        opts.qos = MQTT_QOS;
        opts.message = mg_str("goodbye");
        opts.keepalive = priv->cfg.opts->mqtt_keepalive;

        priv->mqtt_conn = mg_mqtt_connect(mgr, priv->cfg.opts->mqtt_serve_address, &opts, mqtt_cb, NULL);
        priv->ping_active = now;
        priv->pong_active = now;

    } else if (priv->cfg.opts->mqtt_keepalive) { //need keep alive

        if (now < priv->ping_active) {
            MG_INFO(("system time loopback"));
            priv->ping_active = now;
            priv->pong_active = now;
        }
        if (now - priv->ping_active >= priv->cfg.opts->mqtt_keepalive * 1000) {
            mg_mqtt_ping(priv->mqtt_conn);
            priv->ping_active = now;
        }
    }
}

static void cloud_mqtt_ev_open_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MG_INFO(("cloud mqtt client connection created"));
}

static void cloud_mqtt_ev_connect_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;

    MG_INFO(("cloud mqtt client connection connected"));

    if (mg_url_is_ssl(priv->cfg.opts->cloud_mqtt_serve_address)) {
        struct mg_tls_opts opts = { 0 };
        opts.ca = priv->cfg.opts->cloud_mqtts_ca;
        opts.cert = priv->cfg.opts->cloud_mqtts_cert;
        opts.certkey = priv->cfg.opts->cloud_mqtts_certkey;

        mg_tls_init(c, &opts);

    }

}

static void cloud_mqtt_ev_error_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    MG_ERROR(("%lu %s", c->id, (char *) ev_data));
    c->is_closing = 1;
}

static void cloud_mqtt_ev_poll_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    if (!priv->cfg.opts->cloud_mqtt_keepalive) //no keepalive
        return;

    uint64_t now = mg_millis();

    if (priv->cloud_pong_active && now > priv->cloud_pong_active &&
        now - priv->cloud_pong_active > (priv->cfg.opts->cloud_mqtt_keepalive + 6)*1000) {
        MG_INFO(("cloud mqtt client connction timeout"));
        c->is_closing = 1;
    }

}

static void cloud_mqtt_ev_close_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;
    MG_INFO(("cloud mqtt client connection closed"));
    if ( priv->registered ) {
        priv->registered = 0;
        priv->disconnected_check_times = 0;
    }
    priv->cloud_mqtt_conn = NULL; // Mark that we're closed

}

static void cloud_mqtt_ev_mqtt_open_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct client_private *priv = (struct client_private*)c->mgr->userdata;

    // MQTT connect is successful
    MG_INFO(("connect to mqtt server: %s", priv->cfg.opts->cloud_mqtt_serve_address));
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    char *topic = "TODO";
    struct mg_str subt = mg_str(topic);
    sub_opts.topic = subt;
    sub_opts.qos = MQTT_QOS;
    mg_mqtt_sub(c, &sub_opts);
    MG_INFO(("subscribed to %.*s", (int) subt.len, subt.ptr));

    priv->registered = 1;
    cloud_mqtt_event_callback(c->mgr, "connected");

}

static void cloud_mqtt_ev_mqtt_cmd_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    struct client_private *priv = (struct client_private*)c->mgr->userdata;

    if (mm->cmd == MQTT_CMD_PINGRESP) {
        priv->cloud_pong_active = mg_millis();
    }

}

static void cloud_mqtt_ev_mqtt_msg_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    MG_DEBUG(("received %.*s <- %.*s", (int) mm->data.len, mm->data.ptr,
        (int) mm->topic.len, mm->topic.ptr));

    // handle msg from cloud mqtt server
    cloud_mqtt_msg_callback(c, mm->topic, mm->data);

}


static void cloud_mqtt_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {

    switch (ev) {
        case MG_EV_OPEN:
            cloud_mqtt_ev_open_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_CONNECT:
            cloud_mqtt_ev_connect_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_ERROR:
            cloud_mqtt_ev_error_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_OPEN:
            cloud_mqtt_ev_mqtt_open_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_CMD:
            cloud_mqtt_ev_mqtt_cmd_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_MQTT_MSG:
            cloud_mqtt_ev_mqtt_msg_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_POLL:
            cloud_mqtt_ev_poll_cb(c, ev, ev_data, fn_data);
            break;

        case MG_EV_CLOSE:
            cloud_mqtt_ev_close_cb(c, ev, ev_data, fn_data);
            break;
    }
}


// Timer function - recreate client connection if it is closed
void timer_cloud_mqtt_fn(void *arg) {
    struct mg_mgr *mgr = (struct mg_mgr *)arg;
    struct client_private *priv = (struct client_private*)mgr->userdata;
    uint64_t now = mg_millis();

    if (priv->cloud_mqtt_conn == NULL) {
        struct mg_mqtt_opts opts = { 0 };

        if (priv->cfg.opts->cloud_mqtt_client_id) {
            opts.client_id = mg_str(priv->cfg.opts->cloud_mqtt_client_id);
        }

        opts.clean = true;
        opts.qos = MQTT_QOS;
        opts.message = mg_str("goodbye");
        opts.keepalive = priv->cfg.opts->cloud_mqtt_keepalive;
        opts.version = 4;
        opts.user = mg_str(priv->cfg.opts->cloud_mqtt_username);
        opts.pass = mg_str(priv->cfg.opts->cloud_mqtt_password);

        priv->cloud_mqtt_conn = mg_mqtt_connect(mgr, priv->cfg.opts->cloud_mqtt_serve_address, &opts, cloud_mqtt_cb, NULL);
        priv->cloud_ping_active = now;
        priv->cloud_pong_active = now;

    } else if (priv->cfg.opts->cloud_mqtt_keepalive) { //need keep alive
        
        if (now < priv->cloud_ping_active) {
            MG_INFO(("system time loopback"));
            priv->cloud_ping_active = now;
            priv->cloud_pong_active = now;
        }
        if (now - priv->cloud_ping_active >= priv->cfg.opts->cloud_mqtt_keepalive * 1000) {
            mg_mqtt_ping(priv->cloud_mqtt_conn);
            priv->cloud_ping_active = now;
        }
    }
    if (priv->registered == 0 && ++priv->disconnected_check_times % 6 == 0) {
        cloud_mqtt_event_callback(mgr, "disconnected");
    }
}
