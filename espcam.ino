#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    5
#define SIOD_GPIO_NUM    8
#define SIOC_GPIO_NUM    9

#define Y9_GPIO_NUM      4
#define Y8_GPIO_NUM      6
#define Y7_GPIO_NUM      7
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      17
#define Y4_GPIO_NUM      21
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM      16

#define VSYNC_GPIO_NUM   1
#define HREF_GPIO_NUM    2
#define PCLK_GPIO_NUM    15


const char* WIFI_SSID     = "TP-LINK_96B298";
const char* WIFI_PASSWORD = "9258369415";
const char* MQTT_HOST = "172.81.0.12";
const uint16_t MQTT_PORT = 8883;
const char* publish_topic = "device/test-device-2/access/scan";
const char* TLS_SERVER_NAME = "emqx.emqx.svc.cluster.local";
const char* DUMMY_RFID_UID = "044F8A2C5E7180";


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
char mqttClientId[64];
httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;


#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";

static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "X-Timestamp: %d.%06d\r\n"
    "\r\n";



static const char INDEX_HTML[] PROGMEM = R"HTML(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>OCOC ESP32-S3 Camera</title>

<style>

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    padding: 20px;

    background: #111;

    color: white;

    font-family:
        Arial,
        Helvetica,
        sans-serif;

    text-align: center;
}

.container {
    width: 100%;
    max-width: 900px;

    margin: auto;
}

h1 {
    margin-bottom: 20px;
}

.video-container {
    width: 100%;

    background: #000;

    border-radius: 12px;

    overflow: hidden;

    box-shadow:
        0 0 20px rgba(0,0,0,0.5);
}

#stream {
    display: block;

    width: 100%;

    height: auto;
}

.buttons {
    display: flex;

    flex-wrap: wrap;

    justify-content: center;

    gap: 12px;

    margin-top: 20px;
}

button {

    padding: 14px 24px;

    border: none;

    border-radius: 8px;

    font-size: 16px;

    font-weight: bold;

    cursor: pointer;
}

.capture {
    background: #2196F3;

    color: white;
}

.mqtt {
    background: #00a86b;

    color: white;
}

button:active {
    transform: scale(0.97);
}

#captureImage {

    width: 100%;

    margin-top: 20px;

    border-radius: 12px;

    display: none;

    background: #000;
}

.status {

    margin-top: 15px;

    padding: 10px;

    border-radius: 8px;

    background: #222;

    color: #ccc;
}

</style>

</head>


<body>

<div class="container">

<h1>
    OCOC ESP32-S3 Camera
</h1>


<!-- ======================================================
     LIVE VIDEO
     ====================================================== -->

<div class="video-container">

    <img
        id="stream"
        src="/stream"
        alt="Live Camera">

</div>


<!-- ======================================================
     BUTTONS
     ====================================================== -->

<div class="buttons">

    <button
        class="capture"
        onclick="captureImage()">

        Capture Image

    </button>


    <button
        class="mqtt"
        onclick="captureAndSendMQTT()">

        Capture + Send MQTT

    </button>

</div>


<!-- ======================================================
     CAPTURED IMAGE
     ====================================================== -->

<img
    id="captureImage"
    alt="Captured Image">


<!-- ======================================================
     STATUS
     ====================================================== -->

<div
    class="status"
    id="status">

    Camera Ready

</div>


</div>


<script>


// ========================================================
// CAPTURE IMAGE
// ========================================================

function captureImage()
{

    const status =
        document.getElementById("status");

    const image =
        document.getElementById("captureImage");


    status.innerHTML =
        "Capturing image...";


    image.style.display =
        "block";


    image.src =
        "/capture?t=" + Date.now();


    image.onload =
        function()
        {

            status.innerHTML =
                "Image captured successfully.";

        };


    image.onerror =
        function()
        {

            status.innerHTML =
                "Camera capture failed.";

        };

}


// ========================================================
// CAPTURE + MQTT
// ========================================================

function captureAndSendMQTT()
{

    const status =
        document.getElementById("status");

    const image =
        document.getElementById("captureImage");


    status.innerHTML =
        "Capturing image and sending to MQTT...";


    image.style.display =
        "block";


    image.src =
        "/capture?mqtt=1&t=" + Date.now();


    image.onload =
        function()
        {

            status.innerHTML =
                "Image captured and sent to MQTT.";

        };


    image.onerror =
        function()
        {

            status.innerHTML =
                "MQTT capture failed.";

        };

}
</script>
</body>
</html>
)HTML";

void mqttCallback(
    char* topic,
    byte* payload,
    unsigned int length)
{

    Serial.println();
    Serial.println("========== MQTT MESSAGE ==========");

    Serial.print("Topic: ");
    Serial.println(topic);

    Serial.print("Payload: ");

    for (
        unsigned int i = 0;
        i < length;
        i++
    )
    {
        Serial.print(
            (char)payload[i]
        );
    }

    Serial.println();

    Serial.println(
        "=================================="
    );
}

bool connectWiFi()
{

    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        return true;
    }


    Serial.println();
    Serial.println(
        "Connecting to WiFi..."
    );


    WiFi.mode(WIFI_STA);

    WiFi.setSleep(false);

    WiFi.setAutoReconnect(true);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );


    unsigned long start =
        millis();


    while (
        WiFi.status() !=
        WL_CONNECTED
    )
    {

        delay(500);

        Serial.print(".");


        if (
            millis() - start >
            20000
        )
        {

            Serial.println();

            Serial.println(
                "WiFi connection timeout"
            );

            return false;
        }

    }


    Serial.println();

    Serial.println(
        "WiFi connected"
    );


    Serial.print(
        "IP address: "
    );

    Serial.println(
        WiFi.localIP()
    );


    Serial.print(
        "RSSI: "
    );

    Serial.println(
        WiFi.RSSI()
    );


    return true;
}
bool syncTime()
{

    Serial.println();

    Serial.println(
        "Synchronizing time..."
    );


    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov"
    );


    unsigned long start =
        millis();


    time_t now;


    while (
        (now = time(nullptr)) <
        24 * 3600
    )
    {

        delay(500);

        Serial.print(".");


        if (
            millis() - start >
            15000
        )
        {

            Serial.println();

            Serial.println(
                "NTP synchronization timeout"
            );

            return false;
        }

    }


    Serial.println();

    Serial.println(
        "Time synchronized"
    );


    return true;
}
bool connectMQTT()
{

    if (
        mqttClient.connected()
    )
    {
        return true;
    }


    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        return false;
    }


    Serial.println();

    Serial.println(
        "Connecting to MQTT..."
    );

    Serial.print(
        "Broker host: "
    );

    Serial.println(
        MQTT_HOST
    );

    Serial.print(
        "TLS server name: "
    );

    Serial.println(
        TLS_SERVER_NAME
    );

    IPAddress brokerIp;

    if (
        !WiFi.hostByName(
            MQTT_HOST,
            brokerIp
        )
    )
    {

        Serial.println(
            "Failed to resolve broker host"
        );

        return false;
    }

    Serial.print(
        "Resolved broker IP: "
    );

    Serial.println(
        brokerIp
    );

    secureClient.setCACert(
        root_ca
    );

    secureClient.setCertificate(
       client_cert
    );

    secureClient.setPrivateKey(
        client_key
    );


    secureClient.setHandshakeTimeout(
        15
    );

    secureClient.stop();

    if (
        !secureClient.connect(
            brokerIp,
            MQTT_PORT,
            TLS_SERVER_NAME,
            root_ca,
            client_cert,
            client_key
        )
    )
    {

        char errbuf[128];

        secureClient.lastError(
            errbuf,
            sizeof(errbuf)
        );

        Serial.print(
            "TLS connect failed: "
        );

        Serial.println(
            errbuf
        );

        return false;
    }

    mqttClient.setServer(
        MQTT_HOST,
        MQTT_PORT
    );

    mqttClient.setCallback(
        mqttCallback
    );

    mqttClient.setKeepAlive(
        30
    );


    mqttClient.setSocketTimeout(
        15
    );

    snprintf(
        mqttClientId,
        sizeof(mqttClientId),

        "ococ-camera-%06X",

        (unsigned int)
        ESP.getEfuseMac()
    );


    Serial.print(
        "MQTT Client ID: "
    );

    Serial.println(
        mqttClientId
    );

    bool connected =
        mqttClient.connect(
            mqttClientId
        );


    if (!connected)
    {

        Serial.print(
            "MQTT connection failed. State = "
        );

        Serial.println(
            mqttClient.state()
        );

        return false;
    }


    Serial.println(
        "MQTT connected!"
    );


    return true;
}

bool publishHealthCheck()
{

    if (
        !mqttClient.connected()
    )
    {
        if (!connectMQTT())
        {
            return false;
        }
    }


    Serial.println();
    Serial.println(
        "Publishing MQTT health check..."
    );


    bool sent =
        mqttClient.publish(
            publish_topic,
            "hii",
            true
        );


    if (
        sent
    )
    {

        Serial.print(
            "Health check published to "
        );

        Serial.println(
            publish_topic
        );

        return true;
    }


    Serial.println(
        "Health check publish failed"
    );

    return false;
}

bool publishImage(
    camera_fb_t* fb)
{

    if (fb == nullptr)
    {
        return false;
    }


    if (
        !mqttClient.connected()
    )
    {

        if (!connectMQTT())
        {
            return false;
        }

    }


    Serial.println();

    Serial.println(
        "========== MQTT IMAGE =========="
    );


    Serial.print(
        "Image size: "
    );

    Serial.print(
        fb->len
    );

    Serial.println(
        " bytes"
    );


    Serial.print(
        "Topic: "
    );

    Serial.println(
        publish_topic
    );

    const uint16_t uidLen = (uint16_t)strlen(DUMMY_RFID_UID);
    const size_t totalLen = 2 + (size_t)uidLen + fb->len;

    if (
        !mqttClient.beginPublish(

            publish_topic,

            totalLen,

            false

        )
    )
    {

        Serial.println(
            "beginPublish() failed"
        );

        return false;
    }

    uint8_t lenBuf[2];
    lenBuf[0] = (uint8_t)((uidLen >> 8) & 0xFF);
    lenBuf[1] = (uint8_t)(uidLen & 0xFF);

    size_t written = mqttClient.write(lenBuf, 2);
    if (written != 2)
    {
        Serial.print("MQTT write failed for UID length: ");
        Serial.println(written);
        mqttClient.endPublish();
        return false;
    }

    written = mqttClient.write((const uint8_t*)DUMMY_RFID_UID, uidLen);
    if (written != uidLen)
    {
        Serial.print("MQTT write failed for UID bytes: ");
        Serial.println(written);
        mqttClient.endPublish();
        return false;
    }

    const size_t CHUNK = 1024;
    size_t offset = 0;
    size_t remaining = fb->len;

    while (remaining > 0)
    {
        const size_t toWrite = remaining > CHUNK ? CHUNK : remaining;
        size_t chunkWritten = mqttClient.write(fb->buf + offset, toWrite);

        if (chunkWritten == 0)
        {
            Serial.print("MQTT image write failed at byte ");
            Serial.println(offset);
            mqttClient.endPublish();
            return false;
        }

        offset += chunkWritten;
        remaining -= chunkWritten;
    }

    if (
        !mqttClient.endPublish()
    )
    {

        Serial.println(
            "endPublish() failed"
        );

        return false;
    }


    Serial.println(
        "JPEG + RFID UID published successfully"
    );


    Serial.println(
        "================================"
    );


    return true;
}



camera_fb_t* captureFrame()
{

    camera_fb_t* fb =
        esp_camera_fb_get();


    if (
        fb == nullptr
    )
    {

        Serial.println(
            "Camera capture failed"
        );

        return nullptr;
    }


    Serial.print(
        "Captured image: "
    );

    Serial.print(
        fb->len
    );

    Serial.println(
        " bytes"
    );


    return fb;
}

static esp_err_t captureHandler(
    httpd_req_t* req)
{

    camera_fb_t* fb =
        captureFrame();


    if (
        fb == nullptr
    )
    {

        httpd_resp_send_500(
            req
        );

        return ESP_FAIL;
    }

    char query[64];

    bool sendMQTT =
        false;


    size_t queryLen =
        httpd_req_get_url_query_len(
            req
        );


    if (
        queryLen > 0 &&
        queryLen < sizeof(query)
    )
    {

        if (
            httpd_req_get_url_query_str(
                req,
                query,
                sizeof(query)
            ) == ESP_OK
        )
        {

            if (
                strstr(
                    query,
                    "mqtt=1"
                ) != nullptr
            )
            {
                sendMQTT = true;
            }

        }

    }


    if (sendMQTT)
    {

        if (
            WiFi.status() !=
            WL_CONNECTED
        )
        {
            connectWiFi();
        }


        if (
            !mqttClient.connected()
        )
        {
            connectMQTT();
        }


        if (
            mqttClient.connected()
        )
        {

            publishImage(
                fb
            );

        }
        else
        {

            Serial.println(
                "MQTT unavailable"
            );

        }

    }

    httpd_resp_set_type(
        req,
        "image/jpeg"
    );


    httpd_resp_set_hdr(
        req,
        "Content-Disposition",
        "inline; filename=capture.jpg"
    );


    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );


    esp_err_t res =
        httpd_resp_send(

            req,

            (const char*)fb->buf,

            fb->len
        );

    esp_camera_fb_return(
        fb
    );


    return res;
}

static esp_err_t streamHandler(
    httpd_req_t* req)
{

    camera_fb_t* fb = NULL;

    esp_err_t res =
        ESP_OK;


    size_t jpgLen = 0;

    uint8_t* jpgBuf = NULL;


    char partBuf[128];


    struct timeval timestamp;

    res =
        httpd_resp_set_type(
            req,
            STREAM_CONTENT_TYPE
        );


    if (
        res != ESP_OK
    )
    {
        return res;
    }


    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );

    while (true)
    {

        fb =
            esp_camera_fb_get();


        if (
            fb == nullptr
        )
        {

            Serial.println(
                "Camera capture failed"
            );

            res =
                ESP_FAIL;

            break;
        }


        timestamp =
            fb->timestamp;

        if (
            fb->format ==
            PIXFORMAT_JPEG
        )
        {

            jpgLen =
                fb->len;

            jpgBuf =
                fb->buf;

        }
        res =
            httpd_resp_send_chunk(

                req,

                STREAM_BOUNDARY,

                strlen(
                    STREAM_BOUNDARY
                )
            );


        if (
            res == ESP_OK
        )
        {

            int headerLen =
                snprintf(

                    partBuf,

                    sizeof(partBuf),

                    STREAM_PART,

                    jpgLen,

                    (int)
                    timestamp.tv_sec,

                    (int)
                    timestamp.tv_usec
                );


            res =
                httpd_resp_send_chunk(

                    req,

                    partBuf,

                    headerLen
                );

        }

        if (
            res == ESP_OK
        )
        {

            res =
                httpd_resp_send_chunk(

                    req,

                    (const char*)
                    jpgBuf,

                    jpgLen
                );

        }

        esp_camera_fb_return(
            fb
        );


        fb = NULL;

        jpgBuf = NULL;


        if (
            res != ESP_OK
        )
        {

            Serial.println(
                "Stream client disconnected"
            );

            break;
        }


        // Small yield
        delay(1);

    }


    return res;
}

static esp_err_t indexHandler(
    httpd_req_t* req)
{

    httpd_resp_set_type(
        req,
        "text/html"
    );


    return httpd_resp_send(

        req,

        INDEX_HTML,

        strlen(
            INDEX_HTML
        )
    );
}

void startCameraServer()
{

    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();


    config.max_uri_handlers =
        8;


    config.server_port =
        80;


    httpd_uri_t indexUri = {

        .uri = "/",

        .method = HTTP_GET,

        .handler = indexHandler,

        .user_ctx = NULL
    };


    httpd_uri_t captureUri = {

        .uri = "/capture",

        .method = HTTP_GET,

        .handler = captureHandler,

        .user_ctx = NULL
    };


    if (
        httpd_start(
            &camera_httpd,
            &config
        ) == ESP_OK
    )
    {

        httpd_register_uri_handler(
            camera_httpd,
            &indexUri
        );


        httpd_register_uri_handler(
            camera_httpd,
            &captureUri
        );

    }

    config.server_port =
        81;


    httpd_uri_t streamUri = {

        .uri = "/stream",

        .method = HTTP_GET,

        .handler = streamHandler,

        .user_ctx = NULL
    };


    if (
        httpd_start(
            &stream_httpd,
            &config
        ) == ESP_OK
    )
    {

        httpd_register_uri_handler(
            stream_httpd,
            &streamUri
        );

    }


    Serial.println();
    Serial.println(
        "Camera HTTP server started"
    );

}

bool initCamera()
{

    camera_config_t config;


    config.ledc_channel =
        LEDC_CHANNEL_0;

    config.ledc_timer =
        LEDC_TIMER_0;


    config.pin_d0 =
        Y2_GPIO_NUM;

    config.pin_d1 =
        Y3_GPIO_NUM;

    config.pin_d2 =
        Y4_GPIO_NUM;

    config.pin_d3 =
        Y5_GPIO_NUM;

    config.pin_d4 =
        Y6_GPIO_NUM;

    config.pin_d5 =
        Y7_GPIO_NUM;

    config.pin_d6 =
        Y8_GPIO_NUM;

    config.pin_d7 =
        Y9_GPIO_NUM;


    config.pin_xclk =
        XCLK_GPIO_NUM;

    config.pin_pclk =
        PCLK_GPIO_NUM;

    config.pin_vsync =
        VSYNC_GPIO_NUM;

    config.pin_href =
        HREF_GPIO_NUM;


    config.pin_sccb_sda =
        SIOD_GPIO_NUM;

    config.pin_sccb_scl =
        SIOC_GPIO_NUM;


    config.pin_pwdn =
        PWDN_GPIO_NUM;

    config.pin_reset =
        RESET_GPIO_NUM;


    config.xclk_freq_hz =
        20000000;

    config.pixel_format =
        PIXFORMAT_JPEG;


    // VGA = 640x480
    config.frame_size =
        FRAMESIZE_QVGA;


    // Lower number = higher quality
    config.jpeg_quality =
        12;

    if (
        psramFound()
    )
    {

        Serial.println(
            "PSRAM detected"
        );


        config.fb_location =
            CAMERA_FB_IN_PSRAM;


        config.fb_count =
            2;


        config.grab_mode =
            CAMERA_GRAB_LATEST;

    }
    else
    {

        Serial.println(
            "WARNING: PSRAM not found"
        );


        config.fb_location =
            CAMERA_FB_IN_DRAM;


        config.fb_count =
            1;


        config.grab_mode =
            CAMERA_GRAB_WHEN_EMPTY;

    }

    esp_err_t err =
        esp_camera_init(
            &config
        );


    if (
        err != ESP_OK
    )
    {

        Serial.print(
            "Camera init failed: 0x"
        );

        Serial.println(
            err,
            HEX
        );

        return false;
    }

    sensor_t* sensor =
        esp_camera_sensor_get();


    if (
        sensor != nullptr
    )
    {

        Serial.print(
            "Camera PID: 0x"
        );

        Serial.println(
            sensor->id.PID,
            HEX
        );


        sensor->set_framesize(
            sensor,
            FRAMESIZE_QVGA
        );

    }
    Serial.println(
        "Camera initialized"
    );
    return true;
}
void setup()
{

    Serial.begin(
        115200
    );


    delay(1000);


    Serial.println();
    Serial.println(
        "=========================================="
    );

    Serial.println(
        " OCOC ESP32-S3 CAMERA"
    );

    Serial.println(
        " Live Video + Capture + MQTT"
    );

    Serial.println(
        "=========================================="
    );

    if (
        !initCamera()
    )
    {

        Serial.println(
            "Camera initialization failed."
        );


        while (true)
        {
            delay(1000);
        }

    }

    if (
        !connectWiFi()
    )
    {

        Serial.println(
            "WiFi connection failed."
        );

    }



    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {

        syncTime();

    }
    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {

        connectMQTT();

        publishHealthCheck();

    }

    startCameraServer();
    Serial.println();

    Serial.println(
        "=========================================="
    );

    Serial.println(
        " CAMERA READY"
    );


    Serial.print(
        "Web server: http://"
    );

    Serial.println(
        WiFi.localIP()
    );


    Serial.print(
        "Live stream: http://"
    );

    Serial.print(
        WiFi.localIP()
    );

    Serial.println(
        ":81/stream"
    );


    Serial.print(
        "MQTT topic: "
    );

    Serial.println(
        publish_topic
    );


    Serial.println(
        "=========================================="
    );

}

void loop()
{

    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {

        connectWiFi();

    }

    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {

        if (
            !mqttClient.connected()
        )
        {

            connectMQTT();

        }


        mqttClient.loop();

    }

    delay(2);

}