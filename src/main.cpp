#include <Arduino.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "camera_config.h"
#include "esp_camera.h"
#include "image_manipulation.h"
#include "model_data.h"
#include "soc/rtc_wdt.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

struct rectangle_t {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
};
struct config_t {
    std::vector<rectangle_t> rectangles;
};

config_t config;
QueueHandle_t queue;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
std::unique_ptr<tflite::MicroInterpreter> interpreter;
#define ARENA_SIZE 1024 * 32
uint8_t tensor_arena[ARENA_SIZE];

config_t parseConfig() {
    // Load config from LittleFS
    File file = LittleFS.open("/config.json", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file in reading mode");
        return {};
    }

    StaticJsonDocument<2048> doc;

    // Parse config
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Failed to parse config");
        return {};
    }

    config_t config;
    // Parse rectangles
    for (JsonObject rectangle : doc["rectangles"].as<JsonArray>()) {
        unsigned rectangle_x = rectangle["x"];
        unsigned rectangle_y = rectangle["y"];
        unsigned rectangle_width = rectangle["width"];
        unsigned rectangle_height = rectangle["height"];
        config.rectangles.push_back({.x = rectangle_x,
                                     .y = rectangle_y,
                                     .width = rectangle_width,
                                     .height = rectangle_height});
    }
    file.close();
    return config;
}

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

    queue = xQueueCreate(1, sizeof(AsyncWebServerRequest*));

    if (ESP_OK != esp_camera_init(&camera_config)) {
        Serial.println("Camera Init Failed");
        return;
    }

    // Setup model
    const tflite::Model* model = tflite::GetModel(tmnist_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.print("Model provided is schema version ");
        Serial.print(model->version());
        Serial.print(" not equal to supported version ");
        Serial.println(TFLITE_SCHEMA_VERSION);
    }

    // Add resolver
    static tflite::MicroMutableOpResolver<8> resolver;
    resolver.AddReadVariable();
    resolver.AddQuantize();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddDequantize();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();

    // Build interpreter
    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, ARENA_SIZE);
    interpreter = std::move(std::unique_ptr<tflite::MicroInterpreter>(&static_interpreter));
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("Failed to allocate tensors");
        return;
    } else {
        Serial.println("Successfully allocated tensors");
        Serial.print("Used bytes: ");
        Serial.println(interpreter->arena_used_bytes());
    }

    config = parseConfig();

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // Send index.html.gz file with correct header
        AsyncWebServerResponse* response =
            request->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    server.on(
        "/api/upload-config", HTTP_POST,
        [](AsyncWebServerRequest* request) { Serial.println("Received request to upload config"); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            Serial.printf("Received %d bytes of data\n", len);
            // Save request body json to LittleFS
            File file = LittleFS.open("/config.json", FILE_WRITE);
            if (!file) {
                Serial.println("Failed to open file in writing mode");
                request->send(200, "text/plain", "Failed to open file in writing mode");
                return;
            }
            // Write to file
            while (!file.write(data, len)) {
                Serial.println("Failed to write to file");
            }
            file.flush();
            file.close();
            Serial.println("Written to config");

            // Parse config
            config = parseConfig();
            request->send(200, "text/plain", "Config saved");
        });

    server.on("/api/image", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->client()->setRxTimeout(60000);
        AsyncResponseStream* response = request->beginResponseStream("application/octet-stream");
        camera_fb_t* pic = esp_camera_fb_get();
        esp_camera_fb_return(pic);
        pic = esp_camera_fb_get();
        for (auto rectangle : config.rectangles) {
            uint8_t image_data[28 * 28] = {0};
            response->write(image_data, 28 * 28);
        }
        response->write(pic->buf, pic->len);
        request->send(response);
        esp_camera_fb_return(pic);
    });

    server.on("/api/inference", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->client()->setRxTimeout(60000);
        if (xQueueSend(queue, &request, 10) != pdPASS) {
            Serial.println("Failed to send request to queue");
        }
    });

    server.on("/api/log", HTTP_DELETE, [](AsyncWebServerRequest* request) {
        File file = LittleFS.open("/log.txt", FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open log");
            return;
        }
        file.close();
        request->send(200, "text/plain", "Log reset");
    });

    server.serveStatic("/", LittleFS, "/");

    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        }
    });

    server.begin();
    timeClient.begin();
}

void processImage() {
    AsyncWebServerRequest* request;
    if (xQueueReceive(queue, &request, 10)) {
        AsyncResponseStream* response = request->beginResponseStream("application/octet-stream");
        String value = "";

        // Begin response
        uint8_t out_image_data[28 * 28] = {0};
        out_image_t out_image = {
            .pixels = out_image_data,
            .w = 28,
            .h = 28,
        };
        camera_fb_t* pic = esp_camera_fb_get();
        esp_camera_fb_return(pic);
        pic = esp_camera_fb_get();

        // Loop through all rectangles
        for (auto rectangle : config.rectangles) {
            in_image_t in_image = {
                .pixels = pic->buf,
                .w = pic->width,
                .h = pic->height,
                .offsetX = rectangle.x,
                .offsetY = rectangle.y,
                .sectionWidth = rectangle.width,
                .sectionHeight = rectangle.height,
            };
            scale(&in_image, &out_image, 28, 28);

            // Obtain a pointer to the model's input tensor
            TfLiteTensor* input = interpreter->input(0);

            // Set input data
            for (int i = 0; i < 784; i++) {
                input->data.f[i] = out_image_data[i] / 255.0;
            }

            // Run inference
            if (interpreter->Invoke() != kTfLiteOk) {
                Serial.println("Failed to invoke tflite");
                return;
            }

            // Obtain a pointer to the output tensor
            TfLiteTensor* output = interpreter->output(0);

            // Print out the output tensor's details to verify
            int max_index = 0;
            float max_value = 0;
            for (int i = 0; i < 10; i++) {
                if (output->data.f[i] > max_value) {
                    max_value = output->data.f[i];
                    max_index = i;
                }
            }
            value += String(max_index);
            response->write(out_image_data, 28 * 28);
        }

        File file = LittleFS.open("/log.txt", FILE_APPEND);
        if (!file) {
            Serial.println("Failed to open log");
            return;
        }
        file.print("[");
        file.print(timeClient.getFormattedDate());
        file.print("] ");
        file.println(value);
        file.close();

        response->write(pic->buf, pic->len);
        request->send(response);
        esp_camera_fb_return(pic);
    }
}

void loop() {
    timeClient.update();
    processImage();
}
