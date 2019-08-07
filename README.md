# MqttNet

## MQTT Topics

| Topic                           | Published-By | Retain | Description                             |
|---------------------------------|--------------|--------|-----------------------------------------|
| $prefix/net/connected           | MqttNet      | yes    | 0=offline (by will), 1=online           |
| $prefix/net/restart             | remote       | no     | Restart the ESP                         |
| $prefix/net/sync/reset          | remote       | no     |                                         |
| $prefix/net/sync/name           | remote       | no     |                                         |
| $prefix/net/sync/md5            | remote       | no     |                                         |
| $prefix/net/sync/size           | remote       | no     |                                         |
| $prefix/net/sync/state          | MqttNet      | no     |                                         |
| $prefix/net/address             | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/boot_mode       | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/boot_version    | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/chip_id         | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/core_version    | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/cpu_freq_mhz    | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/reset_info      | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/reset_reason    | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/sdk_version     | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/sketch_md5      | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/esp/sketch_size     | MqttNet      | yes    | Metadata, published once per connection |
| $prefix/net/millis              | MqttNet      | yes    | Statistics, published once per minute   |
| $prefix/net/esp/free_heap       | MqttNet      | yes    | Statistics, published once per minute   |
| $prefix/net/esp/free_cont_stack | MqttNet      | yes    | Statistics, published once per minute   |
