
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* ssid = "veer";
const char* password = "@12345678";

// Same broker IP as the ESP32-CAM sketch.
const char* mqtt_host = "172.81.0.12";
const uint16_t mqtt_port = 8883;

const char* tls_server_name = "emqx.emqx.svc.cluster.local";
const char* publish_topic = "device/test-device-2/scan";

const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n"
"MIIBjjCCATOgAwIBAgIJALCGN6avhackMAoGCCqGSM49BAMCMCoxGTAXBgNVBAMM\n"
"EG9jb2MtZGV2LXJvb3QtY2ExDTALBgNVBAoMBG9jb2MwHhcNMjYwNzMwMDcwMDIw\n"
"WhcNMzYwNzI3MDcwMDIwWjAqMRkwFwYDVQQDDBBvY29jLWRldi1yb290LWNhMQ0w\n"
"CwYDVQQKDARvY29jMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEPrwtAwoIno4K\n"
"nG21IklXZk6izmKiLwtw997v4FdaIONR0BztRCx6PN/ZDKA9wsAGkCHFpEhS9UTE\n"
"VyckakIeCqNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYD\n"
"VR0OBBYEFE47LWGtc54vt+9/cOF7gIrdTw3wMAoGCCqGSM49BAMCA0kAMEYCIQDE\n"
"2P9DN3JkTcB7iM7VaiEZz8rV+ktMRJ2QVelCByCd6AIhAK7HV40fTSuL2EJcFP+U\n"
"qmJYQTCVIPsAlDDBdDJbe11c\n"
"-----END CERTIFICATE-----\n";

const char* client_cert = \
"-----BEGIN CERTIFICATE-----\n"
"MIIBlTCCATugAwIBAgIJAIUImtGB4F1EMAoGCCqGSM49BAMCMCwxGzAZBgNVBAMM\n"
"Em9jb2MtZGV2LWRldmljZS1jYTENMAsGA1UECgwEb2NvYzAeFw0yNjA4MDUwNzEy\n"
"MDFaFw0yNzA4MDUwNzEyMDFaMCwxFjAUBgNVBAMMDXRlc3QtZGV2aWNlLTIxEjAQ\n"
"BgNVBAoMCW9jb2MtdGVzdDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABKOUp0S5\n"
"rWs011knXJ3B0BDh6nPE7uC6630ykNesiokSx+/BAqnfWZOD7A0Z4AcmiOzaeDm3\n"
"Yi33Viha2roRu2ujRjBEMA4GA1UdDwEB/wQEAwIHgDATBgNVHSUEDDAKBggrBgEF\n"
"BQcDAjAdBgNVHQ4EFgQU4ciDZR1Wwkr5YlfkNf/kxVi/zvcwCgYIKoZIzj0EAwID\n"
"SAAwRQIgGHLYUTdDAwyiZz763occJE7GzeHADIBVZTRx0XiXwM0CIQDjj/f2mv8F\n"
"NKD8niiIH/JCWdvoA3KwRC5jY7zetj3HaQ==\n"
"-----END CERTIFICATE-----\n";

const char* client_key = \
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIIEzZ99+VL6Fpbw3wVKGT/FqdrlhguXoX5lOH7Pxyl8ZoAoGCCqGSM49\n"
"AwEHoUQDQgAEo5SnRLmtazTXWSdcncHQEOHqc8Tu4LrrfTKQ16yKiRLH78ECqd9Z\n"
"k4PsDRngByaI7Np4ObdiLfdWKFrauhG7aw==\n"
"-----END EC PRIVATE KEY-----\n";

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
char mqttClientId[40];

bool connectWiFi() {
  Serial.print("Connecting to WiFi ");
  Serial.print(ssid);
  Serial.print("...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
    if (millis() - start > 20000) {
      Serial.println("\nWiFi connect timeout");
      return false;
    }
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void ensureTime() {
  Serial.println("Syncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long start = millis();
  time_t now;
  while ((now = time(nullptr)) < 24 * 3600) {
    delay(500);
    Serial.print('.');
    if (millis() - start > 15000) {
      Serial.println("\nNTP sync timeout");
      return;
    }
  }
  Serial.println("\nTime synced");
}

bool connectMqtt() {
  if (mqttClient.connected()) {
    return true;
  }

  Serial.println("Connecting to MQTT...");
  Serial.print("Broker IP: ");
  Serial.println(mqtt_host);
  Serial.print("TLS server name: ");
  Serial.println(tls_server_name);

  IPAddress brokerIp;
  if (!brokerIp.fromString(mqtt_host)) {
    Serial.println("Invalid broker IP");
    return false;
  }

  secureClient.stop();
  secureClient.setHandshakeTimeout(15);
  secureClient.setCACert(root_ca);
  secureClient.setCertificate(client_cert);
  secureClient.setPrivateKey(client_key);

  if (!secureClient.connect(brokerIp, mqtt_port, tls_server_name, root_ca, client_cert, client_key)) {
    char errbuf[128];
    secureClient.lastError(errbuf, sizeof(errbuf));
    Serial.print("TLS connect failed: ");
    Serial.println(errbuf);
    return false;
  }

  mqttClient.setServer(mqtt_host, mqtt_port);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(15);

  snprintf(mqttClientId, sizeof(mqttClientId), "esp32-hig-test-%06X", (unsigned)ESP.getEfuseMac());
  Serial.print("MQTT client id: ");
  Serial.println(mqttClientId);

  if (!mqttClient.connect(mqttClientId)) {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }

  Serial.println("MQTT connected");
  return true;
}

bool publishHig() {
  if (!mqttClient.connected() && !connectMqtt()) {
    return false;
  }

  Serial.print("Publishing to ");
  Serial.print(publish_topic);
  Serial.println(": hig");

  if (!mqttClient.publish(publish_topic, "hig", true)) {
    Serial.println("Publish failed");
    return false;
  }

  Serial.println("Publish success");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (!connectWiFi()) {
    return;
  }

  ensureTime();

  if (connectMqtt()) {
    publishHig();
  }
}

void loop() {
  mqttClient.loop();
  delay(1000);
}
