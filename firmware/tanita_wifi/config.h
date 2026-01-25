#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WiFi Configuration
// ============================================================================
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

// ============================================================================
// HTTPS Endpoint Configuration
// ============================================================================
#define API_ENDPOINT      "https://your-server.com/api/weight"
#define API_KEY           "your-api-key-here"

// ============================================================================
// ESP32 SPI Slave Pin Configuration
// ============================================================================
// These pins connect to the handset connector (originally going to SD PCB)
// Adjust based on your wiring

#define PIN_SPI_MOSI      23    // Master Out Slave In (data from handset)
#define PIN_SPI_MISO      19    // Master In Slave Out (data to handset)
#define PIN_SPI_SCLK      18    // SPI Clock (from handset)
#define PIN_SPI_CS        5     // Chip Select (directly from handset, directly active low)

// ============================================================================
// LED Status Indicator (optional)
// ============================================================================
#define PIN_LED_STATUS    2     // Built-in LED on most ESP32 dev boards

// ============================================================================
// Debug Configuration
// ============================================================================
#define DEBUG_SERIAL      1     // Enable serial debug output
#define DEBUG_BAUD_RATE   115200

// ============================================================================
// Protocol Configuration
// ============================================================================
#define SPI_BUFFER_SIZE   256   // Max SPI transaction buffer size
#define DATA_BUFFER_SIZE  1024  // Buffer for measurement data accumulation

// ============================================================================
// Profile Configuration (simulated data for handset reads)
// ============================================================================
// This is the system file content the handset expects
#define SYSTEM_FILE_CONTENT "SD,TANITA,GRAPHV1\r\nDo not delete, modify, nor remove this system file by yourself!\r\n"

// Default profile (will be returned when handset reads PROF1.CSV)
// You can customize this based on your setup
#define DEFAULT_PROFILE "{0,16,~1,2,~3,4,MO,\"BC-601\",DB,\"01/01/1990\",Bt,0,GE,1,Hm,175.0,AL,2,CS,00\r\n"

// ============================================================================
// Timing Configuration
// ============================================================================
#define WIFI_CONNECT_TIMEOUT_MS   10000
#define HTTP_TIMEOUT_MS           5000

#endif // CONFIG_H
