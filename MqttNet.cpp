#include "MqttNet.hpp"

MqttNet::MqttNet() {
  using namespace std::placeholders;
  wifiConnectHandler = WiFi.onStationModeGotIP(std::bind(&MqttNet::onWifiConnect, this));
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(std::bind(&MqttNet::onWifiDisconnect, this));
  mqttClient = new AsyncMqttClient;
  mqttClient->onConnect(std::bind(&MqttNet::onMqttConnect, this, _1));
  mqttClient->onDisconnect(std::bind(&MqttNet::onMqttDisconnect, this, _1));
  mqttClient->onMessage(std::bind(&MqttNet::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
}

void MqttNet::begin() {
  watchdogTicker.attach_ms(1000, std::bind(&MqttNet::watchdogHandler, this));
  dequeueTicker.attach_ms(125, std::bind(&MqttNet::dequeueHandler, this));
  statsTicker.attach_ms(_statsInterval, std::bind(&MqttNet::publishStats, this));
  if (WiFi.isConnected()) {
    connectToMqtt(true);
  } else {
    mqttReconnectTimer.once_scheduled(1, std::bind(&MqttNet::connectToMqtt, this, true));
  }
}

void MqttNet::connectToMqtt(bool cleanSession) {
  String prefix = String(mqtt_prefix);
  (prefix + "/" + String("net/connected")).toCharArray(will_topic, sizeof(will_topic));
  mqttClient->setWill(will_topic, 0, 1, "0");
  mqttClient->setServer(mqtt_host, mqtt_port);
  mqttClient->setSecure(mqtt_tls);
  if (strlen(mqtt_username) > 0 && strlen(mqtt_password) > 0) {
    mqttClient->setCredentials(mqtt_username, mqtt_password);
  }
  mqttClient->setCleanSession(cleanSession);
  mqttClient->connect();
}

void MqttNet::dequeueHandler() {
  if (!mqttClient->connected()) {
    while (!subqueue.empty()) {
      subqueue.pop();
    }
    while (!pubqueue.empty()) {
      pubqueue.pop();
    }
    return;
  }
  while (!subqueue.empty()) {
    MqttNetSubscribe sub;
    sub = subqueue.front();
    if (mqttClient->subscribe(sub.topic.c_str(), sub.qos)) {
      //delete sub;
      subqueue.pop();
    } else {
      return;
    }
  }
  while (!pubqueue.empty()) {
    MqttNetMessage message;
    message = pubqueue.front();
    Serial.print("dequeuing message topic=");
    Serial.print(message.topic);
    Serial.print(" payload=");
    Serial.println(message.payload.c_str());
    if (mqttClient->publish(message.topic.c_str(), 0, message.retain, message.payload.c_str())) {
      //delete message;
      pubqueue.pop();
    } else {
      return;
    }
  }
}

bool MqttNet::isConnected() {
  return mqttClient->connected();
}

void MqttNet::onMqttConnect(bool sessionPresent) {
  Serial.println("MqttNet: mqtt connected");
  publish("net/connected", 0, 1, "1");
  subscribe("net/ping", 0);
  subscribe("net/sync/reset", 0);
  subscribe("net/sync/name", 0);
  subscribe("net/sync/md5", 0);
  subscribe("net/sync/size", 0);
  subscribe("net/sync/data", 0);
  subscribe("net/restart", 0);
  publishMetadata();
  publishStats();
  if (connect_callback) {
    connect_callback(sessionPresent);
  }
}

void MqttNet::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.print("MqttNet: mqtt disconnected, reason=");
  Serial.println((int)reason, DEC);
  if (disconnect_callback) {
    disconnect_callback(reason);
  }
  mqttReconnectTimer.once_scheduled(1, std::bind(&MqttNet::connectToMqtt, this, false));
}

void MqttNet::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String sub_topic = String(topic).substring(strlen(mqtt_prefix) + 1);

  if (sub_topic.equals("net/junk")) {
    Serial.print(".");
    return;
  }

  Serial.print(millis(), DEC);
  Serial.print(" message: topic=");
  Serial.print(sub_topic);
  Serial.print(" index=");
  Serial.print(index, DEC);
  Serial.print(" len=");
  Serial.print(len, DEC);
  Serial.print(" total=");
  Serial.println(total, DEC);

  if (sub_topic.startsWith("net/sync/")) {
    if (allowRemoteSync) {
      String action = sub_topic.substring(9);
      onMqttFileMessage(action, payload, properties, len, index, total);
      return;
    } else {
      publish("net/sync/state", 0, 0, "disabled");
      return;
    }
  }

  if (message_callback) {
    message_callback(sub_topic, payload, properties, len, index, total);
  }
  
  if (index == 0 && len == total && len < 256 && !properties.dup) {
    String dataString;
    if (len > 0) {
      char data[len+1];
      strncpy(data, payload, sizeof(data));
      data[len] = 0;
      onMqttString(sub_topic, String(data), properties.retain);
    } else {
      onMqttString(sub_topic, String(""), properties.retain);
    }
  }
}

void MqttNet::onMqttFileMessage(String action, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (properties.retain || properties.dup) {
    return;
  }

  if (action.equals("reset")) {
    newFileName = "";
    newFileMD5 = "";
    newFileSize = -1;
    firmwareWriter.Abort();
    fileWriter.Abort();
    publish("net/sync/state", 0, 0, "ready");
    return;
  }

  if (action.equals("data")) {
    if (newFileName.length() > 0 && newFileMD5.length() > 0 && newFileSize >= 0) {
      if (newFileName.equals("*firmware*")) {
        if (firmwareWriter.Add((uint8_t*)payload, len)) {
          publish("net/sync/state", 0, 0, String(firmwareWriter.GetPosition()));
          if (firmwareWriter.GetPosition() >= newFileSize) {
            if (firmwareWriter.Commit()) {
              publish("net/sync/state", 0, 0, "ok");
              _restartRequiredForFirmware = true;
            } else {
              publish("net/sync/state", 0, 0, String("error: commit - ") + firmwareWriter.GetUpdaterError());
            }
            newFileName = "";
            newFileMD5 = "";
            newFileSize = -1;
          }
        } else {
          publish("net/sync/state", 0, 0, String("error: add - ") + firmwareWriter.GetUpdaterError());
        }
      } else {
        if (fileWriter.Add((uint8_t*)payload, len)) {
          publish("net/sync/state", 0, 0, String(fileWriter.GetPosition()));
          if (fileWriter.GetPosition() >= newFileSize) {
            if (fileWriter.Commit()) {
              publish("net/sync/state", 0, 0, "ok");
              if (file_callback) {
                file_callback(newFileName);
              }
            } else {
              publish("net/sync/state", 0, 0, "error: commit failed");
            }
            newFileName = "";
            newFileMD5 = "";
            newFileSize = -1;
          }
        } else {
          publish("net/sync/state", 0, 0, "error: add failed");
        }
      }
    } else {
      publish("net/sync/state", 0, 0, "error: not ready for data");
    }
    return;
  }

  String payloadString;
  if (index == 0 && len == total && len < 256) {
    if (len > 0) {
      char data[len+1];
      strncpy(data, payload, sizeof(data));
      data[len] = 0;
      payloadString = String(data);
    } else {
      payloadString = "";
    }
  }

  if (action.equals("name")) {
    newFileName = payloadString;
  } else if (action.equals("md5")) {
    newFileMD5 = payloadString;
  } else if (action.equals("size")) {
    newFileSize = payloadString.toInt();
  }

  if (newFileName.length() > 0 && newFileMD5.length() > 0 && newFileSize >= 0) {
    firmwareWriter.Abort();
    fileWriter.Abort();
    if (newFileName.equals("*firmware*")) {
      if (firmwareWriter.Begin(newFileMD5.c_str(), newFileSize)) {
        if (firmwareWriter.UpToDate()) {
          newFileName = "";
          newFileMD5 = "";
          newFileSize = -1;
          publish("net/sync/state", 0, 0, "ok");
          return;
        } else {
          publish("net/sync/state", 0, 0, String(firmwareWriter.GetPosition()));
          return;
        }
      } else {
        publish("net/sync/state", 0, 0, String("error: begin - " + firmwareWriter.GetUpdaterError()));
        return;
      }
    } else {
      if (fileWriter.Begin(newFileName.c_str(), newFileMD5.c_str(), newFileSize)) {
        if (fileWriter.UpToDate()) {
          newFileName = "";
          newFileMD5 = "";
          newFileSize = -1;
          publish("net/sync/state", 0, 0, "ok");
          return;
        } else {
          if (fileWriter.Open()) {
            publish("net/sync/state", 0, 0, String(fileWriter.GetPosition()));
            return;
          } else {
            publish("net/sync/state", 0, 0, "error: open failed");
            return;
          }
        }
      } else {
        publish("net/sync/state", 0, 0, "error: begin failed");
        return;
      }
    }
  } else {
    publish("net/sync/state", 0, 0, "waiting");
  }

}

void MqttNet::onMqttString(String topic, String payload, bool retain) {
  if (topic.equals("net/restart")) {
    _restartRequiredForNetwork = true;
    return;
  }
  if (topic.equals("net/ping")) {
    publish("net/pong", 0, 0, payload);
  }
  if (string_callback) {
    string_callback(topic, payload, retain);
  }
}

void MqttNet::onWifiConnect() {
  Serial.println("MqttNet: wifi connected");
}

void MqttNet::onWifiDisconnect() {
  Serial.println("MqttNet: wifi disconnected");
}

uint16_t MqttNet::publish(String topic, uint8_t qos, bool retain, String payload) {
  if (!mqttClient->connected()) {
    Serial.println("not connected, discarding message");
    return 0;
  }
  
  if (pubqueue.size() > _maxPublishQueue) {
    Serial.println("publish queue full, discarding message");
    return 0;
  }

  String full_topic = String(mqtt_prefix) + "/" + topic;
  MqttNetMessage message;
  message.qos = qos;
  message.retain = retain;
  message.topic = full_topic;
  message.payload = payload;
  pubqueue.push(message);
  return 1;
}

void MqttNet::publishMetadata() {
  if (mqttClient->connected()) {
    publish("net/address", 0, 1, WiFi.localIP().toString());
    publish("net/esp/boot_mode", 0, 1, String(ESP.getBootMode()));
    publish("net/esp/boot_version", 0, 1, String(ESP.getBootVersion()));
    publish("net/esp/chip_id", 0, 1, String(ESP.getChipId()));
    publish("net/esp/core_version", 0, 1, String(ESP.getCoreVersion()));
    publish("net/esp/cpu_freq_mhz", 0, 1, String(ESP.getCpuFreqMHz()));
    publish("net/esp/reset_info", 0, 1, String(ESP.getResetInfo()));
    publish("net/esp/reset_reason", 0, 1, String(ESP.getResetReason()));
    publish("net/esp/sdk_version", 0, 1, String(ESP.getSdkVersion()));
    publish("net/esp/sketch_md5", 0, 1, String(ESP.getSketchMD5()));
    publish("net/esp/sketch_size", 0, 1, String(ESP.getSketchSize()));
  }
}

void MqttNet::publishStats() {
  if (mqttClient->connected()) {
    publish("net/millis", 0, 1, String(millis()));
    publish("net/esp/free_heap", 0, 1, String(ESP.getFreeHeap()));
    publish("net/esp/free_cont_stack", 0, 1, String(ESP.getFreeContStack()));
  }
}

bool MqttNet::restartRequired() {
  return _restartRequiredForNetwork || _restartRequiredForFirmware;
}

bool MqttNet::restartRequiredForFirmware() {
  return _restartRequiredForFirmware;
}

void MqttNet::setConfig(const char *host, uint16_t port, bool tls, const char *username, const char *password, const char *prefix) {
  mqtt_host = host;
  mqtt_port = port;
  mqtt_tls = tls;
  mqtt_username = username;
  mqtt_password = password;
  mqtt_prefix = prefix;
}

void MqttNet::setWatchdog(long timeout) {
  _watchdogRestartTimeout = timeout;
  if (timeout <= 0) {
    _restartRequiredForWatchdog = false;
  }
}

uint16_t MqttNet::subscribe(String topic, uint8_t qos) {
  if (subqueue.size() > _maxSubscribeQueue) {
    return 0;
  }

  String full_topic = String(mqtt_prefix) + "/" + topic;
  MqttNetSubscribe sub;
  sub.qos = qos;
  sub.topic = full_topic;
  subqueue.push(sub);
  return 1;
}

void MqttNet::watchdogHandler() {
  if (WiFi.isConnected() && mqttClient->connected()) {
    _watchdogLastOk = millis();
  }
  if (_watchdogRestartTimeout > 0) {
    if (millis() - _watchdogLastOk > _watchdogRestartTimeout) {
      if (!_restartRequiredForWatchdog) {
        Serial.println("MqttNet: network watchdog requesting restart");
        _restartRequiredForWatchdog = true;
      }
    } else {
      _restartRequiredForWatchdog = false;
    }
  }
}
