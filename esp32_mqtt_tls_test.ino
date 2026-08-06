/*
  esp32_mqtt_tls_test.ino

  ESP32 (WROOM-32) one-shot MQTT connection test.

  Behavior:
  - Connects to WiFi
  - Optionally syncs NTP when strict TLS is enabled
  - Connects once to the MQTT broker
  - Publishes a single `hii` message

  Quick usage:
  - Paste WiFi and PEM contents (root CA, client cert, client key) below.
  - Leave `TEST_INSECURE_TLS = true` for a quick connectivity test.
  - Set `TEST_INSECURE_TLS = false` to enforce broker certificate validation.
  - Upload and open Serial Monitor at 115200.

  Security: Embedding private keys in source is for testing only. Use
  SPIFFS/LittleFS or secure elements for production.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// --------- User configuration (replace placeholders) ---------
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Broker connect host (can be IP or DNS)
const char* mqtt_host = "ec2-13-207-195-130.ap-south-1.compute.amazonaws.com";
const uint16_t mqtt_port = 8883;

// TLS server name (SNI / hostname verification).
// Set this to the broker certificate's SAN/CN if it differs from mqtt_host.
const char* tls_server_name = "emqx.emqx.svc.cluster.local";

const char* publish_topic = "device/test-device-2/scan";

// Optional MQTT auth (set to nullptr to skip)
const char* mqtt_user = nullptr; // e.g. "username"
const char* mqtt_pass = nullptr; // e.g. "password"

// Last Will and Testament (LWT)
const char* lwt_topic = "test/esp32/lwt";
const char* lwt_payload = "offline";
const bool lwt_retain = false;
const int lwt_qos = 1;

// Publish heartbeat interval (ms)
const unsigned long PUBLISH_INTERVAL_MS = 15000UL;

// Paste your PEM contents between the quotes below, including BEGIN/END lines.
// Keep the formatting exactly as in the PEM file.
// ===== Real Root CA from root-ca.crt =====
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

// ===== Real Client Certificate from test-device-2.crt =====
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

// ===== Real Client Private Key from test-device-2.key =====
const char* client_key = \
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIIEzZ99+VL6Fpbw3wVKGT/FqdrlhguXoX5lOH7Pxyl8ZoAoGCCqGSM49\n"
"AwEHoUQDQgAEo5SnRLmtazTXWSdcncHQEOHqc8Tu4LrrfTKQ16yKiRLH78ECqd9Z\n"
"k4PsDRngByaI7Np4ObdiLfdWKFrauhG7aw==\n"
"-----END EC PRIVATE KEY-----\n";
// ---------------------------------------------------------------

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);
char mqttClientId[40];

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

bool connectWiFi() {
  Serial.print("Connecting to WiFi "); Serial.print(ssid); Serial.print("...");
  WiFi.disconnect(true);
  delay(100);
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
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());
  return true;
}

// Ensure system time is set via NTP before attempting strict TLS verification.
void ensureTime() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping NTP sync because WiFi is not connected");
    return;
  }
  Serial.println("Syncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long start = millis();
  time_t now;
  while ((now = time(nullptr)) < 24 * 3600) {
    delay(500);
    Serial.print('.');
    if (millis() - start > 15000) {
      Serial.println("\nWarning: NTP sync timeout — TLS validation may fail if clock is wrong");
      return;
    }
  }
  Serial.println("\nTime synced: ");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.println(asctime(&timeinfo));
}

bool mqttConnect() {
  if (mqttClient.connected()) return true;

  Serial.print("Connecting to MQTT broker "); Serial.print(mqtt_host);
  Serial.print(":"); Serial.println(mqtt_port);
  Serial.print("Using TLS server name: "); Serial.println(tls_server_name);

  IPAddress brokerIp;
  if (!WiFi.hostByName(mqtt_host, brokerIp)) {
    Serial.println("Failed to resolve broker host to an IP address");
    return false;
  }
  Serial.print("Resolved broker IP: ");
  Serial.println(brokerIp);

  secureClient.stop();
  secureClient.setHandshakeTimeout(15);
  secureClient.setCACert(root_ca);
  secureClient.setCertificate(client_cert);
  secureClient.setPrivateKey(client_key);

  // Connect to the broker IP, but send the certificate name as the TLS host.
  if (!secureClient.connect(brokerIp, mqtt_port, tls_server_name, root_ca, client_cert, client_key)) {
    char errbuf[128];
    secureClient.lastError(errbuf, sizeof(errbuf));
    Serial.print("TLS connect failed: ");
    Serial.println(errbuf);
    return false;
  }

  mqttClient.setServer(mqtt_host, mqtt_port);
  mqttClient.setCallback(callback);

  // PubSubClient will use the already-open TLS socket.
  bool ok = mqttClient.connect(mqttClientId);

  if (ok) {
    Serial.println("MQTT connected");
    if (mqttClient.publish(publish_topic, "hii")) { 
      Serial.print("Published to ");
      Serial.print(publish_topic);
      Serial.println(": hii");
    } else {
      Serial.println("Publish failed");
    }
    mqttClient.loop();
    return true;
  }

  Serial.print("MQTT connect failed, rc=");
  Serial.println(mqttClient.state());
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  snprintf(mqttClientId, sizeof(mqttClientId), "esp32-tls-test-%06X", (unsigned)ESP.getEfuseMac());
  Serial.print("MQTT client id: ");
  Serial.println(mqttClientId);

  if (!connectWiFi()) {
    Serial.println("WiFi failed. Check SSID/password, band (2.4 GHz), signal strength, and router access.");
    while (true) { delay(1000); }
  }

  ensureTime();

  Serial.println("Starting MQTT connection test...");

  // One-shot connection test: connect once, publish once, then stop.
  if (!mqttConnect()) {
    Serial.println("MQTT connection failed. Check broker reachability, credentials, and TLS settings.");
  } else {
    Serial.println("Connection test complete.");
  }
}

void loop() {
  mqttClient.loop();
  delay(1000);
}
