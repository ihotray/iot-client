#ifndef __IOT_CALLBACK_H__
#define __IOT_CALLBACK_H__

#include <iot/mongoose.h>

#define report_mqtt_msg_callback(c, t, d) cloud_mqtt_msg_callback(c, t, d)

void local_mqtt_msg_callback(struct mg_connection *c, struct mg_str topic, struct mg_str data);
void cloud_mqtt_msg_callback(struct mg_connection *c, struct mg_str topic, struct mg_str data);
void cloud_mqtt_event_callback(struct mg_mgr *mgr, const char* event);
struct mg_str lua_callback(void *arg, const char *method, const char *data);

#endif