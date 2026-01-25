/*
 * TANITA WiFi Retrofit - ESP32 Firmware
 * Version: 1.1
 *
 * Replaces the SD Card PCB in TANITA BC-601/603/613 body scales
 * to send weight/body composition data via WiFi instead of SD card.
 *
 * This ESP32 acts as an SPI slave, responding to commands from the
 * handset using the proprietary TANITA protocol.
 *
 * Hardware connections:
 *   ESP32 GPIO23 (MOSI) <- Handset MOSI (data from handset)
 *   ESP32 GPIO19 (MISO) -> Handset MISO (data to handset)
 *   ESP32 GPIO18 (SCLK) <- Handset SCLK (clock from handset)
 *   ESP32 GPIO5  (CS)   <- Handset CS   (active low)
 *   ESP32 3.3V          <- Handset VCC
 *   ESP32 GND           <- Handset GND
 *
 * Protocol Analysis:
 *   Frame format: [length] [command] [payload...] [checksum]
 *   Checksum: ~(sum of all bytes except checksum) & 0xFF
 *   Response commands: request_cmd | 0x80
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "driver/spi_slave.h"
#include "config.h"

// ============================================================================
// Protocol Constants
// ============================================================================

// Command bytes (second byte of frame from handset)
#define CMD_STATUS_1        0x12
#define CMD_STATUS_2        0x13
#define CMD_STATUS_3        0x11
#define CMD_FILE_INIT       0x14
#define CMD_FILE_PATH       0x21
#define CMD_END_TRANSFER    0x22
#define CMD_DATA_CHUNK      0x23
#define CMD_WRITE_CHUNK     0x24
#define CMD_ALLOCATE        0x27
#define CMD_WRITE_PATH      0x41

// Response modifier
#define RESP_FLAG           0x80

// Virtual file paths
#define PATH_SYSTEM         "TANITA/GRAPHV1/SYSTEM/SYSTEM.TXT"
#define PATH_PROFILE_1      "TANITA/GRAPHV1/SYSTEM/PROF1.CSV"
#define PATH_DATA_1         "TANITA/GRAPHV1/DATA/DATA1.CSV"

// ============================================================================
// State Machine
// ============================================================================

enum ProtocolState {
    STATE_IDLE,
    STATE_HANDSHAKE,
    STATE_FILE_INIT,
    STATE_FILE_OPEN,
    STATE_READING,
    STATE_WRITING,
    STATE_WRITE_DATA,
    STATE_END
};

// ============================================================================
// Global Variables
// ============================================================================

// SPI buffers (must be DMA-capable, word-aligned)
WORD_ALIGNED_ATTR uint8_t spiRxBuffer[SPI_BUFFER_SIZE];
WORD_ALIGNED_ATTR uint8_t spiTxBuffer[SPI_BUFFER_SIZE];

// Protocol state
volatile ProtocolState currentState = STATE_IDLE;
volatile bool transactionComplete = false;
volatile size_t lastTransactionLen = 0;

// File simulation
String currentFilePath = "";
String currentFileContent = "";
uint32_t currentFileSize = 0;
uint32_t currentFileOffset = 0;
uint32_t writeAddress = 0;
uint32_t writeBaseAddress = 0;
uint8_t writeMode = 0;  // 0=none, 1=begin, 3=finalize

// Write chunk tracking
#define CHUNK_SIZE 32
uint32_t bytesWritten = 0;

// Measurement data accumulator
String measurementData = "";
bool measurementReceived = false;
bool writeInProgress = false;

// WiFi status
bool wifiConnected = false;

// ============================================================================
// Checksum Calculation
// ============================================================================

uint8_t calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;
}

bool validateChecksum(const uint8_t* data, size_t len) {
    if (len < 2) return false;
    uint8_t expected = calculateChecksum(data, len - 1);
    return (data[len - 1] == expected);
}

// ============================================================================
// SPI Slave Callbacks
// ============================================================================

// Called after a transaction is complete
void IRAM_ATTR spiSlavePostTransCb(spi_slave_transaction_t *trans) {
    lastTransactionLen = trans->trans_len / 8;  // Convert bits to bytes
    transactionComplete = true;
}

// ============================================================================
// Response Builders
// ============================================================================

size_t buildStatusResponse(uint8_t cmd, uint8_t* buffer) {
    buffer[0] = 0x04;
    buffer[1] = cmd | RESP_FLAG;
    buffer[2] = 0x00;
    if (cmd == CMD_STATUS_3) {
        buffer[2] = 0x01;  // Ready flag
    }
    buffer[3] = calculateChecksum(buffer, 3);
    return 4;
}

size_t buildFileInitResponse(uint8_t* buffer) {
    buffer[0] = 0x04;
    buffer[1] = CMD_FILE_INIT | RESP_FLAG;  // 0x94
    buffer[2] = 0x00;
    buffer[3] = calculateChecksum(buffer, 3);
    return 4;
}

size_t buildFileInfoResponse(uint32_t fileSize, uint8_t* buffer) {
    buffer[0] = 0x0D;
    buffer[1] = 0xA1;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = 0x00;
    buffer[5] = 0x00;
    buffer[6] = 0x00;
    buffer[7] = 0x00;
    // File size (little-endian)
    buffer[8] = fileSize & 0xFF;
    buffer[9] = (fileSize >> 8) & 0xFF;
    buffer[10] = (fileSize >> 16) & 0xFF;
    buffer[11] = (fileSize >> 24) & 0xFF;
    buffer[12] = calculateChecksum(buffer, 12);
    return 13;
}

size_t buildDataChunkResponse(const String& content, uint32_t offset, uint32_t chunkSize, uint8_t* buffer) {
    // Calculate actual bytes to send
    uint32_t remaining = content.length() - offset;
    uint32_t actualSize = min(chunkSize, remaining);

    // Frame length = 8 header bytes + data bytes
    uint8_t frameLen = 8 + actualSize;

    buffer[0] = frameLen;
    buffer[1] = 0xA3;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    // Offset (little-endian)
    uint32_t newOffset = offset + actualSize;
    buffer[4] = newOffset & 0xFF;
    buffer[5] = (newOffset >> 8) & 0xFF;
    buffer[6] = (newOffset >> 16) & 0xFF;
    buffer[7] = (newOffset >> 24) & 0xFF;

    // Copy data
    for (uint32_t i = 0; i < actualSize; i++) {
        buffer[8 + i] = content[offset + i];
    }

    buffer[frameLen] = calculateChecksum(buffer, frameLen);
    return frameLen + 1;
}

size_t buildEndReadResponse(uint32_t finalOffset, uint8_t* buffer) {
    buffer[0] = 0x09;
    buffer[1] = 0xA2;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    // Final offset (little-endian)
    buffer[4] = finalOffset & 0xFF;
    buffer[5] = (finalOffset >> 8) & 0xFF;
    buffer[6] = (finalOffset >> 16) & 0xFF;
    buffer[7] = (finalOffset >> 24) & 0xFF;
    buffer[8] = calculateChecksum(buffer, 8);
    return 9;
}

size_t buildAllocateResponse(uint32_t address, uint8_t* buffer) {
    buffer[0] = 0x09;
    buffer[1] = 0xA7;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    // Address (little-endian)
    buffer[4] = address & 0xFF;
    buffer[5] = (address >> 8) & 0xFF;
    buffer[6] = (address >> 16) & 0xFF;
    buffer[7] = (address >> 24) & 0xFF;
    buffer[8] = calculateChecksum(buffer, 8);
    return 9;
}

size_t buildWriteAckResponse(uint8_t* buffer) {
    buffer[0] = 0x04;
    buffer[1] = 0xC1;
    buffer[2] = 0x00;
    buffer[3] = 0x3A;  // Fixed checksum
    return 4;
}

size_t buildWriteChunkAckResponse(uint32_t nextAddress, uint8_t* buffer) {
    buffer[0] = 0x09;
    buffer[1] = 0xA4;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    // Next address (little-endian)
    buffer[4] = nextAddress & 0xFF;
    buffer[5] = (nextAddress >> 8) & 0xFF;
    buffer[6] = (nextAddress >> 16) & 0xFF;
    buffer[7] = (nextAddress >> 24) & 0xFF;
    buffer[8] = calculateChecksum(buffer, 8);
    return 9;
}

// ============================================================================
// File Content Provider
// ============================================================================

String getFileContent(const String& path) {
    if (path.indexOf("SYSTEM.TXT") >= 0) {
        return String(SYSTEM_FILE_CONTENT);
    }
    if (path.indexOf("PROF1.CSV") >= 0 || path.indexOf("PROF2.CSV") >= 0 ||
        path.indexOf("PROF3.CSV") >= 0 || path.indexOf("PROF4.CSV") >= 0) {
        return String(DEFAULT_PROFILE);
    }
    if (path.indexOf("DATA") >= 0 && path.indexOf(".CSV") >= 0) {
        // Return empty for data files (we don't need to provide historical data)
        return "";
    }
    return "";
}

// ============================================================================
// Extract File Path from Frame
// ============================================================================

String extractFilePath(const uint8_t* data, size_t len) {
    // File path starts at byte 2 and goes until checksum
    String path = "";
    for (size_t i = 2; i < len - 1; i++) {
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            path += (char)data[i];
        } else {
            break;
        }
    }
    return path;
}

// ============================================================================
// Extract Data from Write Chunk
// ============================================================================

void extractWriteData(const uint8_t* data, size_t len) {
    // Data starts at byte 2 and goes until checksum
    for (size_t i = 2; i < len - 1; i++) {
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            measurementData += (char)data[i];
        } else if (data[i] == 0x0D || data[i] == 0x0A) {
            measurementData += (char)data[i];
        }
    }
}

// ============================================================================
// Protocol Handler
// ============================================================================

size_t handleProtocol(const uint8_t* rxData, size_t rxLen, uint8_t* txData) {
    if (rxLen < 2) {
        return 0;
    }

    uint8_t frameLen = rxData[0];
    uint8_t cmd = rxData[1];

#if DEBUG_SERIAL
    Serial.printf("RX [%d bytes]: ", rxLen);
    for (size_t i = 0; i < min(rxLen, (size_t)16); i++) {
        Serial.printf("%02X ", rxData[i]);
    }
    if (rxLen > 16) Serial.print("...");
    Serial.println();
#endif

    size_t respLen = 0;

    // Handle based on command byte
    switch (cmd) {
        case CMD_STATUS_1:  // 0x12
        case CMD_STATUS_2:  // 0x13
        case CMD_STATUS_3:  // 0x11
            respLen = buildStatusResponse(cmd, txData);
            currentState = STATE_HANDSHAKE;
            break;

        case CMD_FILE_INIT:  // 0x14
            respLen = buildFileInitResponse(txData);
            currentState = STATE_FILE_INIT;
            break;

        case CMD_FILE_PATH:  // 0x21 - File path for read
            currentFilePath = extractFilePath(rxData, rxLen);
            currentFileContent = getFileContent(currentFilePath);
            currentFileSize = currentFileContent.length();
            currentFileOffset = 0;
#if DEBUG_SERIAL
            Serial.printf("File read request: %s (size: %d)\n",
                         currentFilePath.c_str(), currentFileSize);
#endif
            respLen = buildFileInfoResponse(currentFileSize, txData);
            currentState = STATE_FILE_OPEN;
            break;

        case CMD_END_TRANSFER:  // 0x22
            respLen = buildEndReadResponse(currentFileOffset, txData);
            currentState = STATE_IDLE;
            writeInProgress = false;
            break;

        case CMD_DATA_CHUNK:  // 0x23 - Request data chunk
            if (currentState == STATE_FILE_OPEN || currentState == STATE_READING) {
                uint8_t chunkSize = rxData[2];
                respLen = buildDataChunkResponse(currentFileContent,
                                                  currentFileOffset,
                                                  chunkSize,
                                                  txData);
                currentFileOffset += min((uint32_t)chunkSize,
                                        (uint32_t)(currentFileSize - currentFileOffset));
                currentState = STATE_READING;
            }
            break;

        case CMD_WRITE_CHUNK:  // 0x24 - Write data chunk
            {
                // Extract data bytes (after length and command, before checksum)
                uint8_t dataLen = frameLen - 2;  // Total minus header (2) gives payload + checksum
                if (dataLen > 1) dataLen--;  // Remove checksum byte

                extractWriteData(rxData, rxLen);
                bytesWritten += dataLen;

                // Calculate next address (base + 0x0100 offset + bytes written)
                // Based on observed pattern: addresses increment by chunk size
                writeAddress = writeBaseAddress + 0x0100 + bytesWritten;

                respLen = buildWriteChunkAckResponse(writeAddress, txData);
                currentState = STATE_WRITE_DATA;
            }
            break;

        case CMD_ALLOCATE:  // 0x27 - Allocate space
            // Store base address from allocation request
            writeBaseAddress = rxData[2] | (rxData[3] << 8);
            writeAddress = writeBaseAddress;
            bytesWritten = 0;
            respLen = buildAllocateResponse(writeAddress, txData);
            currentState = STATE_WRITING;
            break;

        case CMD_WRITE_PATH:  // 0x41 - Write file path
            {
                currentFilePath = extractFilePath(rxData, rxLen);
                // Extract write mode byte (byte before checksum)
                // 0x01 = begin write, 0x03 = finalize write
                writeMode = rxData[rxLen - 2];

                if (writeMode == 0x01) {
                    // Beginning new write
                    measurementData = "";
                    bytesWritten = 0;
                    writeInProgress = true;
#if DEBUG_SERIAL
                    Serial.printf("File write BEGIN: %s\n", currentFilePath.c_str());
#endif
                } else if (writeMode == 0x03) {
                    // Finalizing write
                    writeInProgress = false;
                    measurementReceived = true;
#if DEBUG_SERIAL
                    Serial.printf("File write FINALIZE: %s\n", currentFilePath.c_str());
                    Serial.println("=== MEASUREMENT DATA COMPLETE ===");
                    Serial.println(measurementData);
                    Serial.println("=================================");
#endif
                }

                respLen = buildWriteAckResponse(txData);
                currentState = STATE_WRITING;
            }
            break;

        default:
#if DEBUG_SERIAL
            Serial.printf("Unknown command: 0x%02X\n", cmd);
#endif
            break;
    }

#if DEBUG_SERIAL
    if (respLen > 0) {
        Serial.printf("TX [%d bytes]: ", respLen);
        for (size_t i = 0; i < min(respLen, (size_t)16); i++) {
            Serial.printf("%02X ", txData[i]);
        }
        if (respLen > 16) Serial.print("...");
        Serial.println();
    }
#endif

    return respLen;
}

// ============================================================================
// Measurement Data Parser
// ============================================================================

struct MeasurementData {
    String model;
    String date;
    String time;
    int bodyType;
    int gender;
    int age;
    float height;
    int activityLevel;
    float weight;
    float fatWhole;
    float fatRightArm;
    float fatLeftArm;
    float fatRightLeg;
    float fatLeftLeg;
    float fatTrunk;
    float muscleWhole;
    float muscleRightArm;
    float muscleLeftArm;
    float muscleRightLeg;
    float muscleLeftLeg;
    float muscleTrunk;
    float boneMass;
    int physiqueRating;
    int bmrDaily;
    int metabolicAge;
    float bodyWater;
    bool valid;
};

MeasurementData parseMeasurement(const String& data) {
    MeasurementData m;
    m.valid = false;

    // Parse key-value pairs from the CSV format
    // Format: key,value,key,value,...

    int pos = 0;
    while (pos < data.length()) {
        // Find key
        int commaPos = data.indexOf(',', pos);
        if (commaPos < 0) break;

        String key = data.substring(pos, commaPos);
        pos = commaPos + 1;

        // Find value
        String value;
        if (data[pos] == '"') {
            // Quoted string
            int endQuote = data.indexOf('"', pos + 1);
            if (endQuote < 0) break;
            value = data.substring(pos + 1, endQuote);
            pos = endQuote + 2;  // Skip closing quote and comma
        } else {
            commaPos = data.indexOf(',', pos);
            if (commaPos < 0) {
                // Last value
                int crPos = data.indexOf('\r', pos);
                if (crPos > 0) {
                    value = data.substring(pos, crPos);
                } else {
                    value = data.substring(pos);
                }
                pos = data.length();
            } else {
                value = data.substring(pos, commaPos);
                pos = commaPos + 1;
            }
        }

        // Map to struct fields
        if (key == "MO") m.model = value;
        else if (key == "DT") m.date = value;
        else if (key == "Ti") m.time = value;
        else if (key == "Bt") m.bodyType = value.toInt();
        else if (key == "GE") m.gender = value.toInt();
        else if (key == "AG") m.age = value.toInt();
        else if (key == "Hm") m.height = value.toFloat();
        else if (key == "AL") m.activityLevel = value.toInt();
        else if (key == "Wk") { m.weight = value.toFloat(); m.valid = true; }
        else if (key == "FW") m.fatWhole = value.toFloat();
        else if (key == "Fr") m.fatRightArm = value.toFloat();
        else if (key == "Fl") m.fatLeftArm = value.toFloat();
        else if (key == "FR") m.fatRightLeg = value.toFloat();
        else if (key == "FL") m.fatLeftLeg = value.toFloat();
        else if (key == "FT") m.fatTrunk = value.toFloat();
        else if (key == "mW") m.muscleWhole = value.toFloat();
        else if (key == "mr") m.muscleRightArm = value.toFloat();
        else if (key == "ml") m.muscleLeftArm = value.toFloat();
        else if (key == "mR") m.muscleRightLeg = value.toFloat();
        else if (key == "mL") m.muscleLeftLeg = value.toFloat();
        else if (key == "mT") m.muscleTrunk = value.toFloat();
        else if (key == "bW") m.boneMass = value.toFloat();
        else if (key == "IF") m.physiqueRating = value.toInt();
        else if (key == "rD") m.bmrDaily = value.toInt();
        else if (key == "rA") m.metabolicAge = value.toInt();
        else if (key == "ww") m.bodyWater = value.toFloat();
    }

    return m;
}

// ============================================================================
// WiFi Functions
// ============================================================================

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - startTime) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println(" Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        wifiConnected = false;
        Serial.println(" Failed!");
    }
}

bool sendMeasurementData(const MeasurementData& data) {
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot send data");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();  // For testing - use proper certificates in production

    HTTPClient http;
    http.begin(client, API_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + API_KEY);
    http.setTimeout(HTTP_TIMEOUT_MS);

    // Build JSON payload
    String json = "{";
    json += "\"model\":\"" + data.model + "\",";
    json += "\"date\":\"" + data.date + "\",";
    json += "\"time\":\"" + data.time + "\",";
    json += "\"weight\":" + String(data.weight, 1) + ",";
    json += "\"bodyFat\":" + String(data.fatWhole, 1) + ",";
    json += "\"muscleMass\":" + String(data.muscleWhole, 1) + ",";
    json += "\"boneMass\":" + String(data.boneMass, 1) + ",";
    json += "\"bodyWater\":" + String(data.bodyWater, 1) + ",";
    json += "\"bmr\":" + String(data.bmrDaily) + ",";
    json += "\"metabolicAge\":" + String(data.metabolicAge) + ",";
    json += "\"physiqueRating\":" + String(data.physiqueRating) + ",";
    json += "\"fatSegments\":{";
    json += "\"rightArm\":" + String(data.fatRightArm, 1) + ",";
    json += "\"leftArm\":" + String(data.fatLeftArm, 1) + ",";
    json += "\"rightLeg\":" + String(data.fatRightLeg, 1) + ",";
    json += "\"leftLeg\":" + String(data.fatLeftLeg, 1) + ",";
    json += "\"trunk\":" + String(data.fatTrunk, 1);
    json += "},";
    json += "\"muscleSegments\":{";
    json += "\"rightArm\":" + String(data.muscleRightArm, 1) + ",";
    json += "\"leftArm\":" + String(data.muscleLeftArm, 1) + ",";
    json += "\"rightLeg\":" + String(data.muscleRightLeg, 1) + ",";
    json += "\"leftLeg\":" + String(data.muscleLeftLeg, 1) + ",";
    json += "\"trunk\":" + String(data.muscleTrunk, 1);
    json += "}";
    json += "}";

    Serial.println("Sending measurement data...");
    Serial.println(json);

    int httpCode = http.POST(json);

    if (httpCode > 0) {
        Serial.printf("HTTP Response: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            Serial.println("Data sent successfully!");
            http.end();
            return true;
        }
    } else {
        Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

// ============================================================================
// SPI Slave Initialization
// ============================================================================

void initSPISlave() {
    // SPI slave configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SPI_BUFFER_SIZE,
    };

    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = PIN_SPI_CS,
        .flags = 0,
        .queue_size = 1,
        .mode = 0,  // SPI Mode 0
        .post_setup_cb = nullptr,
        .post_trans_cb = spiSlavePostTransCb,
    };

    // Initialize SPI slave
    esp_err_t ret = spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("SPI slave init failed: %d\n", ret);
        return;
    }

    Serial.println("SPI slave initialized");

    // Clear buffers
    memset(spiRxBuffer, 0, SPI_BUFFER_SIZE);
    memset(spiTxBuffer, 0, SPI_BUFFER_SIZE);
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
#if DEBUG_SERIAL
    Serial.begin(DEBUG_BAUD_RATE);
    delay(1000);
    Serial.println();
    Serial.println("================================");
    Serial.println("TANITA WiFi Retrofit v1.0");
    Serial.println("================================");
#endif

    // Status LED
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Connect to WiFi
    connectWiFi();

    // Initialize SPI slave
    initSPISlave();

    Serial.println("Ready! Waiting for scale data...");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Prepare transaction
    spi_slave_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length = SPI_BUFFER_SIZE * 8;  // Length in bits
    trans.rx_buffer = spiRxBuffer;
    trans.tx_buffer = spiTxBuffer;

    // Queue transaction and wait
    transactionComplete = false;
    esp_err_t ret = spi_slave_queue_trans(HSPI_HOST, &trans, portMAX_DELAY);
    if (ret != ESP_OK) {
        delay(1);
        return;
    }

    // Wait for transaction to complete
    spi_slave_transaction_t* rtrans;
    ret = spi_slave_get_trans_result(HSPI_HOST, &rtrans, portMAX_DELAY);
    if (ret != ESP_OK) {
        return;
    }

    // Process received data
    if (lastTransactionLen > 0) {
        // Handle protocol and prepare response for next transaction
        size_t respLen = handleProtocol(spiRxBuffer, lastTransactionLen, spiTxBuffer);

        // Pad response buffer
        for (size_t i = respLen; i < SPI_BUFFER_SIZE; i++) {
            spiTxBuffer[i] = 0x00;
        }
    }

    // Check if we received measurement data
    if (measurementReceived) {
        measurementReceived = false;

        // Blink LED to indicate data received
        digitalWrite(PIN_LED_STATUS, HIGH);

        // Parse and send the data
        MeasurementData data = parseMeasurement(measurementData);
        if (data.valid) {
            Serial.println("=== PARSED MEASUREMENT ===");
            Serial.printf("Weight: %.1f kg\n", data.weight);
            Serial.printf("Body Fat: %.1f%%\n", data.fatWhole);
            Serial.printf("Muscle: %.1f kg\n", data.muscleWhole);
            Serial.printf("BMR: %d kcal\n", data.bmrDaily);
            Serial.println("==========================");

            // Send to server
            if (!sendMeasurementData(data)) {
                Serial.println("Failed to send data - will retry on next measurement");
            }
        }

        // Clear for next measurement
        measurementData = "";
        digitalWrite(PIN_LED_STATUS, LOW);
    }

    // Check WiFi connection periodically
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, reconnecting...");
            connectWiFi();
        }
    }
}
