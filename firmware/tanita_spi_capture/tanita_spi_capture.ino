/*
 * TANITA SPI Protocol Capture/Debug Tool
 *
 * Simple sketch for capturing and analyzing the SPI protocol
 * without WiFi functionality. Use this first to verify your
 * wiring and understand the communication.
 *
 * Hardware connections:
 *   ESP32 GPIO23 (MOSI) <- Handset MOSI
 *   ESP32 GPIO19 (MISO) -> Handset MISO
 *   ESP32 GPIO18 (SCLK) <- Handset SCLK
 *   ESP32 GPIO5  (CS)   <- Handset CS
 *   ESP32 3.3V          <- Handset VCC
 *   ESP32 GND           <- Handset GND
 */

#include "driver/spi_slave.h"

// Pin definitions
#define PIN_SPI_MOSI      23
#define PIN_SPI_MISO      19
#define PIN_SPI_SCLK      18
#define PIN_SPI_CS        5
#define PIN_LED           2

#define BUFFER_SIZE       128

// DMA-capable aligned buffers
WORD_ALIGNED_ATTR uint8_t rxBuffer[BUFFER_SIZE];
WORD_ALIGNED_ATTR uint8_t txBuffer[BUFFER_SIZE];

// Transaction tracking
volatile bool transactionDone = false;
volatile size_t rxLen = 0;

// Protocol state
enum State { IDLE, HANDSHAKE, FILE_OP };
State currentState = IDLE;

// Known file contents
const char* SYSTEM_FILE = "SD,TANITA,GRAPHV1\r\nDo not delete, modify, nor remove this system file by yourself!\r\n";
const char* PROFILE_FILE = "{0,16,~1,2,~3,4,MO,\"BC-601\",DB,\"01/01/1990\",Bt,0,GE,1,Hm,175.0,AL,2,CS,00\r\n";

String currentFile = "";
uint32_t fileOffset = 0;
uint32_t fileSize = 0;

// Data capture
String capturedData = "";

// ============================================================================
// Checksum
// ============================================================================

uint8_t calcChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;
}

// ============================================================================
// Response Builders
// ============================================================================

size_t buildResponse(uint8_t* tx, const uint8_t* pattern, size_t len) {
    memcpy(tx, pattern, len);
    return len;
}

size_t statusResponse(uint8_t cmd, uint8_t* tx) {
    tx[0] = 0x04;
    tx[1] = cmd | 0x80;
    tx[2] = (cmd == 0x11) ? 0x01 : 0x00;
    tx[3] = calcChecksum(tx, 3);
    return 4;
}

size_t fileInitResponse(uint8_t* tx) {
    tx[0] = 0x04;
    tx[1] = 0x94;
    tx[2] = 0x00;
    tx[3] = calcChecksum(tx, 3);
    return 4;
}

size_t fileInfoResponse(uint32_t size, uint8_t* tx) {
    tx[0] = 0x0D;
    tx[1] = 0xA1;
    tx[2] = 0x00; tx[3] = 0x00;
    tx[4] = 0x00; tx[5] = 0x00; tx[6] = 0x00; tx[7] = 0x00;
    tx[8] = size & 0xFF;
    tx[9] = (size >> 8) & 0xFF;
    tx[10] = (size >> 16) & 0xFF;
    tx[11] = (size >> 24) & 0xFF;
    tx[12] = calcChecksum(tx, 12);
    return 13;
}

size_t dataChunkResponse(const char* content, uint32_t contentLen, uint32_t offset, uint32_t reqSize, uint8_t* tx) {
    uint32_t remaining = contentLen - offset;
    uint32_t chunkSize = (remaining < reqSize) ? remaining : reqSize;
    uint8_t frameLen = 8 + chunkSize;

    uint32_t newOffset = offset + chunkSize;

    tx[0] = frameLen;
    tx[1] = 0xA3;
    tx[2] = 0x00; tx[3] = 0x00;
    tx[4] = newOffset & 0xFF;
    tx[5] = (newOffset >> 8) & 0xFF;
    tx[6] = (newOffset >> 16) & 0xFF;
    tx[7] = (newOffset >> 24) & 0xFF;

    for (uint32_t i = 0; i < chunkSize; i++) {
        tx[8 + i] = content[offset + i];
    }

    tx[frameLen] = calcChecksum(tx, frameLen);
    return frameLen + 1;
}

size_t endReadResponse(uint32_t offset, uint8_t* tx) {
    tx[0] = 0x09;
    tx[1] = 0xA2;
    tx[2] = 0x00; tx[3] = 0x00;
    tx[4] = offset & 0xFF;
    tx[5] = (offset >> 8) & 0xFF;
    tx[6] = (offset >> 16) & 0xFF;
    tx[7] = (offset >> 24) & 0xFF;
    tx[8] = calcChecksum(tx, 8);
    return 9;
}

size_t allocateResponse(uint32_t addr, uint8_t* tx) {
    tx[0] = 0x09;
    tx[1] = 0xA7;
    tx[2] = 0x00; tx[3] = 0x00;
    tx[4] = addr & 0xFF;
    tx[5] = (addr >> 8) & 0xFF;
    tx[6] = (addr >> 16) & 0xFF;
    tx[7] = (addr >> 24) & 0xFF;
    tx[8] = calcChecksum(tx, 8);
    return 9;
}

size_t writeAckResponse(uint8_t* tx) {
    tx[0] = 0x04;
    tx[1] = 0xC1;
    tx[2] = 0x00;
    tx[3] = 0x3A;
    return 4;
}

size_t writeChunkAckResponse(uint32_t nextAddr, uint8_t* tx) {
    tx[0] = 0x09;
    tx[1] = 0xA4;
    tx[2] = 0x00; tx[3] = 0x00;
    tx[4] = nextAddr & 0xFF;
    tx[5] = (nextAddr >> 8) & 0xFF;
    tx[6] = (nextAddr >> 16) & 0xFF;
    tx[7] = (nextAddr >> 24) & 0xFF;
    tx[8] = calcChecksum(tx, 8);
    return 9;
}

// ============================================================================
// Helper Functions
// ============================================================================

void printHex(const uint8_t* data, size_t len, const char* prefix) {
    Serial.print(prefix);
    for (size_t i = 0; i < len && i < 64; i++) {
        Serial.printf("%02X ", data[i]);
    }
    if (len > 64) Serial.print("...");
    Serial.println();
}

String extractPath(const uint8_t* data, size_t len) {
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

void extractWriteData(const uint8_t* data, size_t len) {
    for (size_t i = 2; i < len - 1; i++) {
        char c = data[i];
        if (c >= 0x20 && c <= 0x7E) {
            capturedData += c;
        } else if (c == 0x0D || c == 0x0A) {
            capturedData += c;
        }
    }
}

const char* getFileContent(const String& path) {
    if (path.indexOf("SYSTEM.TXT") >= 0) return SYSTEM_FILE;
    if (path.indexOf("PROF") >= 0) return PROFILE_FILE;
    return "";
}

// ============================================================================
// Protocol Handler
// ============================================================================

uint32_t writeAddr = 0;
uint32_t bytesWritten = 0;

size_t handleCommand(const uint8_t* rx, size_t len, uint8_t* tx) {
    if (len < 2) return 0;

    uint8_t frameLen = rx[0];
    uint8_t cmd = rx[1];

    printHex(rx, len, ">> RX: ");

    size_t respLen = 0;

    switch (cmd) {
        case 0x11:
        case 0x12:
        case 0x13:
            respLen = statusResponse(cmd, tx);
            Serial.println("   [STATUS]");
            break;

        case 0x14:
            respLen = fileInitResponse(tx);
            Serial.println("   [FILE_INIT]");
            break;

        case 0x21: // Read file path
            currentFile = extractPath(rx, len);
            fileOffset = 0;
            {
                const char* content = getFileContent(currentFile);
                fileSize = strlen(content);
            }
            Serial.printf("   [READ_FILE] %s (size=%d)\n", currentFile.c_str(), fileSize);
            respLen = fileInfoResponse(fileSize, tx);
            break;

        case 0x22: // End transfer
            respLen = endReadResponse(fileOffset, tx);
            Serial.println("   [END_TRANSFER]");
            if (capturedData.length() > 0) {
                Serial.println("\n========== CAPTURED DATA ==========");
                Serial.println(capturedData);
                Serial.println("====================================\n");
                capturedData = "";
            }
            break;

        case 0x23: // Request chunk
            {
                uint8_t chunkSize = rx[2];
                const char* content = getFileContent(currentFile);
                respLen = dataChunkResponse(content, fileSize, fileOffset, chunkSize, tx);
                fileOffset += chunkSize;
                if (fileOffset > fileSize) fileOffset = fileSize;
                Serial.printf("   [READ_CHUNK] size=%d, offset now=%d\n", chunkSize, fileOffset);
            }
            break;

        case 0x24: // Write chunk
            extractWriteData(rx, len);
            bytesWritten += (len - 3); // minus header and checksum
            writeAddr += 0x20;
            respLen = writeChunkAckResponse(writeAddr, tx);
            Serial.printf("   [WRITE_CHUNK] %d bytes, addr=0x%04X\n", len - 3, writeAddr);
            break;

        case 0x27: // Allocate
            writeAddr = rx[2] | (rx[3] << 8);
            bytesWritten = 0;
            respLen = allocateResponse(writeAddr, tx);
            Serial.printf("   [ALLOCATE] addr=0x%04X\n", writeAddr);
            break;

        case 0x41: // Write file path
            currentFile = extractPath(rx, len);
            {
                uint8_t mode = rx[len - 2];
                if (mode == 0x01) {
                    capturedData = "";
                    Serial.printf("   [WRITE_BEGIN] %s\n", currentFile.c_str());
                } else if (mode == 0x03) {
                    Serial.printf("   [WRITE_FINALIZE] %s\n", currentFile.c_str());
                }
            }
            respLen = writeAckResponse(tx);
            break;

        default:
            Serial.printf("   [UNKNOWN CMD 0x%02X]\n", cmd);
            break;
    }

    if (respLen > 0) {
        printHex(tx, respLen, "<< TX: ");
    }

    return respLen;
}

// ============================================================================
// SPI Callback
// ============================================================================

void IRAM_ATTR spiPostTrans(spi_slave_transaction_t* trans) {
    rxLen = trans->trans_len / 8;
    transactionDone = true;
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n================================");
    Serial.println("TANITA SPI Protocol Capture v1.0");
    Serial.println("================================\n");

    pinMode(PIN_LED, OUTPUT);

    // SPI slave config
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUFFER_SIZE,
    };

    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = PIN_SPI_CS,
        .flags = 0,
        .queue_size = 1,
        .mode = 0,
        .post_setup_cb = nullptr,
        .post_trans_cb = spiPostTrans,
    };

    esp_err_t ret = spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("SPI init failed: %d\n", ret);
        while (1) delay(1000);
    }

    Serial.println("SPI slave initialized. Waiting for data...\n");
    memset(txBuffer, 0, BUFFER_SIZE);
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    spi_slave_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length = BUFFER_SIZE * 8;
    trans.rx_buffer = rxBuffer;
    trans.tx_buffer = txBuffer;

    transactionDone = false;
    esp_err_t ret = spi_slave_queue_trans(HSPI_HOST, &trans, portMAX_DELAY);
    if (ret != ESP_OK) {
        delay(1);
        return;
    }

    spi_slave_transaction_t* rtrans;
    ret = spi_slave_get_trans_result(HSPI_HOST, &rtrans, portMAX_DELAY);
    if (ret != ESP_OK) {
        return;
    }

    if (rxLen > 0) {
        digitalWrite(PIN_LED, HIGH);

        // Process and prepare response for next transaction
        size_t respLen = handleCommand(rxBuffer, rxLen, txBuffer);

        // Pad remaining buffer with zeros
        for (size_t i = respLen; i < BUFFER_SIZE; i++) {
            txBuffer[i] = 0x00;
        }

        digitalWrite(PIN_LED, LOW);
    }
}
