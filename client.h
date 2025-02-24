#ifndef __IOT_CLIENT_H__
#define __IOT_CLIENT_H__

#include <iot/mongoose.h>

struct client_option {

    const char *mqtt_serve_address;      //mqtt 服务端口
    int mqtt_keepalive;                  //mqtt 保活间隔

    const char *cloud_mqtt_client_id;    //cloud mqtt client id
    const char *cloud_mqtt_serve_address;
    const char *cloud_mqtt_username;
    const char *cloud_mqtt_password;
    const char *topic_sub;
    const char *topic_pub;
    int cloud_mqtt_keepalive;
    int cloud_mqtt_qos;

    const char *cloud_mqtts_ca;
    const char *cloud_mqtts_cert;
    const char *cloud_mqtts_certkey;

    const char *dns4_url;
    int dns4_timeout;

    int debug_level;
    const char *callback_lua;

};

struct client_config {
    struct client_option *opts;
    void *cloud_mqtt_cfg;
};

struct client_private {

    struct client_config cfg;

    struct mg_mgr mgr;

    struct mg_connection *mqtt_conn;
    uint64_t ping_active;
    uint64_t pong_active;

    struct mg_connection *cloud_mqtt_conn;
    uint64_t cloud_ping_active;
    uint64_t cloud_pong_active;

    char client_id[21]; //id len 20 + 0

    int registered;
    uint64_t disconnected_check_times;

};

int client_main(void *user_options);

#endif //__IOT_CLIENT_H__