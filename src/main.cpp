#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "esp_camera.h"

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

AsyncWebServer server(80);

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_GRAYSCALE,
    .frame_size = FRAMESIZE_HVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_LATEST,
};

void setup() {
    // Setup serial
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // Connect to WiFi
    WiFi.begin("O2-Internet-223", "6JAK3HFN46");
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    if (ESP_OK != esp_camera_init(&camera_config)) {
        Serial.println("Camera Init Failed");
        return;
    }

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    server.serveStatic("/", LittleFS, "/");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // Send index.html.gz file with correct header
        AsyncWebServerResponse* response =
            request->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    server.on("/get", HTTP_GET, [](AsyncWebServerRequest* request) {
        camera_fb_t* pic = esp_camera_fb_get();

        if (!pic) {
            Serial.println("Camera Capture Failed");
            request->send(200, "text/plain", "Camera Capture Failed");
            return;
        }

        Serial.println("Camera Capture Succeeded");

        // Save picture to LittleFS
        File file = LittleFS.open("/picture.data", FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open file in writing mode");
            request->send(200, "text/plain", "Failed to open file in writing mode");
            return;
        }

        while (!file.write(pic->buf, pic->len)) {
            Serial.println("Failed to write to file");
        }
        file.flush();
        file.close();

        // Print pic params
        Serial.printf("Picture params: width: %d, height: %d, format: %d, len: %d\n", pic->width,
                      pic->height, pic->format, pic->len);
        Serial.printf("Saved file to path: %s\n", "/picture.data");

        esp_camera_fb_return(pic);

        request->send(200, "text/plain", "Camera Capture Succeeded");
    });

    server.on("/image", HTTP_GET, [](AsyncWebServerRequest* request) {
        camera_fb_t* pic = esp_camera_fb_get();
        esp_camera_fb_return(pic);
        pic = esp_camera_fb_get();
        request->send_P(200, "application/octet-stream", pic->buf, pic->len);
        esp_camera_fb_return(pic);
    });

    server.on("/format", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.format()) {
            request->send(200, "text/plain", "Format successful");
        } else {
            request->send(200, "text/plain", "Format failed");
        }
    });

    server.on("/fs-info", HTTP_GET, [](AsyncWebServerRequest* request) {
        String response = "Total bytes: " + String(LittleFS.totalBytes()) + "\n";
        response += "Used bytes: " + String(LittleFS.usedBytes()) + "\n";

        request->send(200, "text/plain", response);
    });

    server.begin();
}

void loop() {
    // put your main code here, to run repeatedly:
}
