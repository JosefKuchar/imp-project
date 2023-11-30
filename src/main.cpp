#include <Arduino.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "esp_camera.h"
#include "image_manipulation.h"
#include "model_data.h"
#include "soc/rtc_wdt.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

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

std::unique_ptr<tflite::MicroInterpreter> interpreter;
#define ARENA_SIZE 1024 * 32
uint8_t tensor_arena[ARENA_SIZE];

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

config_t parseConfig() {
    // Load config from LittleFS
    File file = LittleFS.open("/config.json", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file in reading mode");
        return {};
    }

    StaticJsonDocument<768> doc;

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

    // // Obtain a pointer to the model's input tensor
    // TfLiteTensor* input = interpreter->input(0);

    // // Print out the input tensor's details to verify
    // // the model is working as expected
    // Serial.print("Input size: ");
    // Serial.println(input->dims->size);
    // Serial.print("Input bytes: ");
    // Serial.println(input->bytes);

    // for (int i = 0; i < input->dims->size; i++) {
    //     Serial.print("Input dim ");
    //     Serial.print(i);
    //     Serial.print(": ");
    //     Serial.println(input->dims->data[i]);
    // }

    // // Set input data
    // for (int i = 0; i < 784; i++) {
    //     input->data.f[i] = input_data[i];
    // }

    // // Run inference
    // if (interpreter->Invoke() != kTfLiteOk) {
    //     Serial.println("Failed to invoke tflite");
    //     return;
    // }

    // // Obtain a pointer to the output tensor
    // TfLiteTensor* output = interpreter->output(0);

    // // Print out the output tensor's details to verify
    // // the model is working as expected
    // Serial.print("Output size: ");
    // Serial.println(output->dims->size);
    // Serial.print("Output bytes: ");
    // Serial.println(output->bytes);

    // for (int i = 0; i < output->dims->size; i++) {
    //     Serial.print("Output dim ");
    //     Serial.print(i);
    //     Serial.print(": ");
    //     Serial.println(output->dims->data[i]);
    // }

    // for (int i = 0; i < 10; i++) {
    //     Serial.print("Output data ");
    //     Serial.print(i);
    //     Serial.print(": ");
    //     Serial.println(output->data.f[i]);
    // }

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

    server.on("/image", HTTP_GET, [](AsyncWebServerRequest* request) {
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
        }

        File file = LittleFS.open("/log.txt", FILE_APPEND);
        if (!file) {
            Serial.println("Failed to open log");
            return;
        }
        file.println(value);
        file.close();

        request->send_P(200, "application/octet-stream", pic->buf, pic->len);
        esp_camera_fb_return(pic);
    });

    server.on("/image2", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncResponseStream* response = request->beginResponseStream("application/octet-stream");

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
            for (int i = 0; i < 10; i++) {
                Serial.print("Output data ");
                Serial.print(i);
                Serial.print(": ");
                Serial.println(output->data.f[i]);
            }

            response->write(out_image_data, 28 * 28);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        request->send(response);
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

    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        }
    });

    server.begin();
}

void loop() {
    vTaskDelete(NULL);
}
