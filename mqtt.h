#ifndef __IOT_MQTT_H__
#define __IOT_MQTT_H__

#define IOT_CLIENT_TOPIC  "mg/iot-client/+"
#define IOT_CLIENT_RPCD_TOPIC "mg/iot-client/channel/iot-rpcd"
#define IOT_CLIENT_RPCD_TOPIC_PREFIX "mg/iot-client/channel"

void timer_mqtt_fn(void *arg);
void timer_cloud_mqtt_fn(void *arg);

#endif