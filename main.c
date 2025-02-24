#include <iot/mongoose.h>
#include <iot/iot.h>
#include "client.h"

#define LUA_CALLBACK_SCRIPT "/www/iot/handler/iot-client.lua"

static void usage(const char *prog, struct client_option *default_opts) {
    struct client_option *opts = default_opts;
    fprintf(stderr,
        "IoT-SDK v.%s\n"
        "Usage: %s OPTIONS\n"
        "  -s ADDR  - mqtt server address, default: '%s'\n"
        "  -i ID    - cloud mqtt client id, default: NULL\n"
        "  -S ADDR  - cloud mqtt server address, default: '%s'\n"
        "  -a n     - local mqtt keepalive, default: %d\n"
        "  -A n     - cloud mqtt keepalive, default: %d\n"
        "  -C CA    - ca content or file path for cloud mqtts communication, default: NULL\n"
        "  -c CERT  - cert content or file path for cloud mqtts communication, default: NULL\n"
        "  -k KEY   - key content or file path for cloud mqtts communication, default: NULL\n"
        "  -u USER  - client mqtt server username, default: '%s'\n"
        "  -p PASS  - client mqtt server password, default: '%s'\n"
        "  -d ADDR  - dns server address, default: '%s'\n"
        "  -t n     - dns server timeout, default: %d\n"
        "  -x PATH  - client connected/disconnected callback script, default: '%s'\n"
        "  -v LEVEL - debug level, from 0 to 4, default: %d\n",
        MG_VERSION, prog, opts->mqtt_serve_address, opts->cloud_mqtt_serve_address, opts->mqtt_keepalive, opts->cloud_mqtt_keepalive, \
        opts->cloud_mqtt_username, opts->cloud_mqtt_password, opts->dns4_url, opts->dns4_timeout, opts->callback_lua, opts->debug_level);

    exit(EXIT_FAILURE);
}

static void parse_args(int argc, char *argv[], struct client_option *opts) {
    // Parse command-line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            opts->mqtt_serve_address = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0) {
            opts->cloud_mqtt_client_id = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0) {
            opts->cloud_mqtt_serve_address = argv[++i];
        } else if (strcmp(argv[i], "-a") == 0) {
            opts->mqtt_keepalive = atoi(argv[++i]);
            if (opts->mqtt_keepalive < 6)
                opts->mqtt_keepalive = 6;
        } else if (strcmp(argv[i], "-A") == 0) {
            opts->cloud_mqtt_keepalive = atoi(argv[++i]);
            if (opts->cloud_mqtt_keepalive < 6)
                opts->cloud_mqtt_keepalive = 6;
        } else if (strcmp(argv[i], "-C") == 0) {
            opts->cloud_mqtts_ca = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0) {
            opts->cloud_mqtts_cert = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0) {
            opts->cloud_mqtts_certkey = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0) {
            opts->cloud_mqtt_username = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            opts->cloud_mqtt_password = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0) {
            opts->dns4_url = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            opts->dns4_timeout = atoi(argv[++i]);
            if (opts->dns4_timeout < 3)
                opts->dns4_timeout = 3;
        } else if (strcmp(argv[i], "-v") == 0) {
            opts->debug_level = atoi(argv[++i]);
        } else if( strcmp(argv[i], "-x") == 0) {
            opts->callback_lua = argv[++i];
        } else {
            usage(argv[0], opts);
        }
    }
}

int main(int argc, char *argv[]) {

    struct client_option opts = {
        .mqtt_serve_address = MQTT_LISTEN_ADDR,
        .mqtt_keepalive = 6,

        .cloud_mqtt_client_id = NULL,
        .cloud_mqtt_serve_address = "mqtts://mqtt.iot.hotray.cn:8883",
        .cloud_mqtt_keepalive = 6,

        .cloud_mqtts_ca = NULL,
        .cloud_mqtts_cert = NULL,
        .cloud_mqtts_certkey = NULL,

        .dns4_url = "udp://119.29.29.29:53", //if you want to use your own dns server, please change this, in a router, perfer to use udp://127.0.0.1:53
        .dns4_timeout = 6,

        .callback_lua = LUA_CALLBACK_SCRIPT,
        .debug_level = MG_LL_INFO,
    };

    parse_args(argc, argv, &opts);
    if (opts.cloud_mqtt_username == NULL) {
        usage(argv[0], &opts);
    }

    MG_INFO(("IoT-SDK version  : v%s", MG_VERSION));
    MG_INFO(("Username         : %s", opts.cloud_mqtt_username));
    MG_INFO(("DNSv4 Server     : %s", opts.dns4_url));
    MG_INFO(("Lua handler path : %s", opts.callback_lua));

    client_main(&opts);

    return 0;
}
