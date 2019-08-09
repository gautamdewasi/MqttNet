#ifndef MQTTNET_HPP
#define MQTTNET_HPP

#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <queue>

#include "FirmwareWriter.hpp"
#include "FileWriter.hpp"

typedef void (*mqttnet_connect_callback_t)(bool sessionPresent);
typedef void (*mqttnet_disconnect_callback_t)(AsyncMqttClientDisconnectReason reason);
typedef void (*mqttnet_message_callback_t)(String topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
typedef void (*mqttnet_string_callback_t)(String topic, String payload, bool retain);
typedef void (*mqttnet_file_callback_t)(String filename);

class MqttNetMessage {
 public:
  String topic;
  String payload;
  uint8_t qos = 0;
  bool retain = false;
};

class MqttNetSubscribe {
 public:
  String topic;
  uint8_t qos = 0;
};

class MqttNet {
 private:
  AsyncMqttClient *mqttClient;
  Ticker mqttReconnectTimer;
  Ticker dequeueTicker;
  Ticker statsTicker;
  WiFiEventHandler wifiConnectHandler;
  WiFiEventHandler wifiDisconnectHandler;
  FirmwareWriter firmwareWriter;
  FileWriter fileWriter;
  String newFileName;
  String newFileMD5;
  int newFileSize;
  const char *clientid;
  const char *mqtt_host;
  uint16_t mqtt_port;
  bool mqtt_tls;
  const char *mqtt_username;
  const char *mqtt_password;
  const char *mqtt_prefix = "test/123/";
  char will_topic[64];
  bool _restartRequiredForNetwork = false;
  bool _restartRequiredForFirmware = false;
  int _maxSubscribeQueue = 20;
  int _maxPublishQueue = 20;
  int _statsInterval = 60000;
  std::queue<MqttNetMessage> pubqueue;
  std::queue<MqttNetSubscribe> subqueue;
  void onWifiConnect();
  void onWifiDisconnect();
  void onMqttConnect(bool sessionPresent);
  void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
  void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
  void onMqttFileMessage(String action, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
  void onMqttString(String topic, String payload, bool retain);
  void connectToMqtt(bool cleanSession=true);
  void dequeueHandler();
  void publishMetadata();
  void publishStats();

 public:
  MqttNet();
  bool allowRemoteSync = false;
  mqttnet_connect_callback_t connect_callback;
  mqttnet_disconnect_callback_t disconnect_callback;
  mqttnet_file_callback_t file_callback;
  mqttnet_message_callback_t message_callback;
  mqttnet_string_callback_t string_callback;
  void begin();
  bool isConnected();
  uint16_t publish(String topic, uint8_t qos, bool retain, String payload);
  bool restartRequired();
  bool restartRequiredForFirmware();
  void setConfig(const char *host, uint16_t port, bool tls, const char *username, const char *password, const char *prefix);
  uint16_t subscribe(String topic, uint8_t qos);
};

#endif
