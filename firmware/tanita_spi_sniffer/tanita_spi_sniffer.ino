/*
 * TANITA SPI PROTOCOL GRABBER  (passive bus sniffer)
 * ===================================================
 *
 * Purpose
 * -------
 * Passively tap the SPI bus between the TANITA handset (SPI master) and the
 * original SD-Card PCB (SPI slave) and stream every CS-framed transaction
 * over USB serial. The original SD PCB STAYS CONNECTED and fully functional --
 * this firmware only LISTENS, it never drives the bus. Decode the captured
 * stream on a host with tools/decode_capture.py.
 *
 * Why two SPI peripherals
 * -----------------------
 * Each logical message in this protocol is its own CS pulse, and is meaningful
 * in only one direction (the other line is idle). To capture full-duplex we run
 * BOTH ESP32-S3 SPI peripherals as slaves on the SAME clock and chip-select:
 *
 *      SPI2 (slave A):  reads the SIMO line (handset -> SD, i.e. MOSI)
 *      SPI3 (slave B):  reads the SOMI line (SD -> handset, i.e. MISO)
 *
 * Both share SCK and CS (the ESP32 GPIO matrix fans one input pin out to
 * several peripheral inputs). Crucially BOTH set miso_io_num = -1 so neither
 * peripheral ever drives a pin -- the tap is high-impedance and invisible to
 * the real bus. A CS pulse ends both slave transactions simultaneously, giving
 * a byte-aligned MOSI/MISO pair per frame.
 *
 * Wiring (see hardware/ schematic). Tap these handset<->SD-PCB signals:
 *      TANITA SCK   -> GPIO12   (PIN_SCK,  shared input)
 *      TANITA CS    -> GPIO10   (PIN_CS,   shared input, active low)
 *      TANITA SIMO  -> GPIO11   (PIN_SIMO, MOSI, into SPI2)
 *      TANITA SOMI  -> GPIO13   (PIN_SOMI, MISO, into SPI3)
 *      TANITA GND   -> GND      (REQUIRED common ground)
 *      TANITA VCC   -> (do NOT connect; power the ESP32 from USB)
 *
 * Adjust the GPIOs below to match your board if needed. Any free GPIOs that
 * are valid SPI inputs will do; these are sane defaults for ESP32-S3 DevKitC.
 *
 * Output format (one line per CS transaction), consumed by decode_capture.py:
 *      <seq>\t<t_us>\t<mosi hex bytes>\t|\t<somi hex bytes>
 * Lines starting with '#' are comments/metadata.
 */

#include "driver/spi_slave.h"
#include "esp_timer.h"

// ---- Pin map (edit to match your wiring) --------------------------------
#define PIN_SCK   12   // shared clock  (handset SCK)
#define PIN_CS    10   // shared chip select (handset CS, active low)
#define PIN_SIMO  11   // MOSI: handset -> SD  (into SPI2)
#define PIN_SOMI  13   // MISO: SD -> handset  (into SPI3)
#define PIN_LED   2    // activity LED (optional)

// ESP32-S3 SPI host IDs
#define HOST_MOSI  SPI2_HOST
#define HOST_MISO  SPI3_HOST

#define BUF_SIZE   256                 // max bytes per transaction (mult. of 4)
#define QUEUE_DEPTH 4                   // queued transactions per host

// DMA-capable, word-aligned buffers. dummyTx is all-zero and never reaches a
// pin (miso disabled) but the driver still wants a tx pointer.
WORD_ALIGNED_ATTR static uint8_t rxMosi[QUEUE_DEPTH][BUF_SIZE];
WORD_ALIGNED_ATTR static uint8_t rxMiso[QUEUE_DEPTH][BUF_SIZE];
WORD_ALIGNED_ATTR static uint8_t dummyTx[BUF_SIZE];

static spi_slave_transaction_t transMosi[QUEUE_DEPTH];
static spi_slave_transaction_t transMiso[QUEUE_DEPTH];

static volatile uint32_t seq = 0;

// --------------------------------------------------------------------------

static void initSlave(spi_host_device_t host, int dataPin) {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = dataPin;   // the line we listen on
    bus.miso_io_num = -1;        // NEVER drive a pin -> passive / high-Z
    bus.sclk_io_num = PIN_SCK;   // shared clock input
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = BUF_SIZE;

    spi_slave_interface_config_t slv = {};
    slv.spics_io_num = PIN_CS;   // shared CS input
    slv.flags = 0;
    slv.queue_size = QUEUE_DEPTH;
    slv.mode = 0;                // CPOL=0, CPHA=0 (confirm with a scope)
    slv.post_setup_cb = nullptr;
    slv.post_trans_cb = nullptr;

    esp_err_t ret = spi_slave_initialize(host, &bus, &slv, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("# FATAL: spi_slave_initialize(host=%d) -> %d\n", host, ret);
        while (true) delay(1000);
    }
}

static void queuePair(int i) {
    memset(&transMosi[i], 0, sizeof(spi_slave_transaction_t));
    transMosi[i].length = BUF_SIZE * 8;
    transMosi[i].rx_buffer = rxMosi[i];
    transMosi[i].tx_buffer = dummyTx;

    memset(&transMiso[i], 0, sizeof(spi_slave_transaction_t));
    transMiso[i].length = BUF_SIZE * 8;
    transMiso[i].rx_buffer = rxMiso[i];
    transMiso[i].tx_buffer = dummyTx;

    // Both must be queued before the master clocks the next CS pulse.
    spi_slave_queue_trans(HOST_MISO, &transMiso[i], portMAX_DELAY);
    spi_slave_queue_trans(HOST_MOSI, &transMosi[i], portMAX_DELAY);
}

static void printLine(uint32_t s, int64_t t_us,
                      const uint8_t* mosi, const uint8_t* miso, size_t n) {
    // seq \t t_us \t mosi hex \t | \t miso hex
    Serial.printf("%lu\t%lld\t", (unsigned long)s, (long long)t_us);
    for (size_t i = 0; i < n; i++) Serial.printf("%02X ", mosi[i]);
    Serial.print("\t|\t");
    for (size_t i = 0; i < n; i++) Serial.printf("%02X ", miso[i]);
    Serial.println();
}

void setup() {
    Serial.begin(921600);
    delay(800);
    pinMode(PIN_LED, OUTPUT);
    memset(dummyTx, 0, sizeof(dummyTx));

    Serial.println("# tanita-spi-sniff v1");
    Serial.println("# fields: seq  t_us  MOSI(hex)  |  SOMI(hex)");
    Serial.printf("# pins: SCK=%d CS=%d SIMO=%d SOMI=%d (passive, miso disabled)\n",
                  PIN_SCK, PIN_CS, PIN_SIMO, PIN_SOMI);

    initSlave(HOST_MOSI, PIN_SIMO);
    initSlave(HOST_MISO, PIN_SOMI);

    for (int i = 0; i < QUEUE_DEPTH; i++) queuePair(i);
    Serial.println("# armed - waiting for bus traffic...");
}

void loop() {
    spi_slave_transaction_t* doneMiso = nullptr;
    spi_slave_transaction_t* doneMosi = nullptr;

    // A CS pulse completes both slaves. Collect MISO first, then its MOSI peer.
    if (spi_slave_get_trans_result(HOST_MISO, &doneMiso, portMAX_DELAY) != ESP_OK)
        return;
    if (spi_slave_get_trans_result(HOST_MOSI, &doneMosi, portMAX_DELAY) != ESP_OK)
        return;

    size_t nMiso = doneMiso->trans_len / 8;
    size_t nMosi = doneMosi->trans_len / 8;
    size_t n = (nMosi > nMiso) ? nMosi : nMiso;   // they should match

    if (n > 0) {
        digitalWrite(PIN_LED, HIGH);
        printLine(seq++, esp_timer_get_time(),
                  (const uint8_t*)doneMosi->rx_buffer,
                  (const uint8_t*)doneMiso->rx_buffer, n);
        digitalWrite(PIN_LED, LOW);
    }

    // Re-arm both buffers for the next CS pulse. Index by buffer pointer so we
    // reuse the same DMA buffer slot that just completed.
    int slot = ((uint8_t(*)[BUF_SIZE])doneMosi->rx_buffer) - rxMosi;
    if (slot < 0 || slot >= QUEUE_DEPTH) slot = 0;
    queuePair(slot);
}
