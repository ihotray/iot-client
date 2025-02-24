# IOT-Client
A general-purpose mqtt client with lua callback。
IOT-Client是一个基于libiot开发的IoT设备客户端程序,实现了设备与本地服务和云平台的双向通信。

## 功能特性

- 双MQTT连接管理
  - 本地MQTT连接: 与本地iot-rpcd服务通信
  - 云端MQTT连接: 与云平台通信(支持TLS加密)
- 自动重连机制
- 心跳保活
- 支持Lua回调脚本处理事件
- 支持DNS解析
- 定时上报功能
- JSON格式数据交互

## 编译

```bash
make
```

## 运行

```bash
iot-client [OPTIONS]
```

### 命令行参数

```
Options:
  -s ADDR  - 本地MQTT服务器地址,默认:mqtt://localhost:1883
  -i ID    - 云端MQTT客户端ID,默认:NULL
  -S ADDR  - 云端MQTT服务器地址,默认:mqtts://mqtt.iot.hotray.cn:8883
  -a n     - 本地MQTT心跳间隔(秒),默认:6
  -A n     - 云端MQTT心跳间隔(秒),默认:6
  -C CA    - TLS CA证书路径
  -c CERT  - TLS 客户端证书路径
  -k KEY   - TLS 客户端私钥路径
  -u USER  - 云端MQTT用户名
  -p PASS  - 云端MQTT密码
  -d ADDR  - DNS服务器地址,默认:udp://119.29.29.29:53
  -t n     - DNS超时时间(秒),默认:6
  -x PATH  - Lua回调脚本路径,默认:/www/iot/handler/iot-client.lua
  -v LEVEL - 调试级别(0-4),默认:2
```

## 示例

连接本地MQTT服务器并使用TLS连接云平台:

```bash
iot-client -s mqtt://localhost:1883 \
           -S mqtts://mqtt.iot.hotray.cn:8883 \
           -u device1 -p secret \
           -C ca.crt -c client.crt -k client.key
```

## Lua回调脚本

iot-client支持使用Lua脚本处理设备事件和生成上报数据。回调脚本需实现以下接口:

```lua
-- 生成上报请求，由iot-rpcd生成回复后发到云端
function gen_request()
    return {
        code = 0, //非0，表示无需上报
        data = { //请求内容
            method = "call",
            param = {...}
        }
    }
end

-- 设备连接/断开事件处理
function on_event(event)
    if event == "connected" then
        -- 处理连接事件
    elseif event == "disconnected" then
        -- 处理断开事件
    end
end
```

## 许可证

GPLv3 License
