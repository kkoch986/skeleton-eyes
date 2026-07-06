// TFT_eSPI User Setup for GC9A01 240x240 displays
// ESP32-S3 SuperMini with YOUR exact pin configuration

#define USER_SETUP_LOADED

// Driver selection
#define GC9A01_DRIVER

// Display size
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

#define TFT_MOSI 11 // SDA -> IO11
#define TFT_SCLK 10 // SCL -> IO10
#define TFT_RST 12  // RST -> IO12 (shared)
#define TFT_DC 8    // DC -> IO8 (shared)

#define SPI_FREQUENCY 60000000 // 60MHz - Maximum supported by GC9A01
#define SPI_READ_FREQUENCY 20000000

// Color depth
#define TFT_RGB_ORDER TFT_BGR

// Specify SPI port explicitly
#define TFT_SPI_PORT 1
