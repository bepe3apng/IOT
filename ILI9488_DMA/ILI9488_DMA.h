#pragma once
#include <Arduino.h>
#include <driver/spi_master.h>

class ILI9488_DMA : public Print {
public:
    ILI9488_DMA(int8_t cs, int8_t dc, int8_t rst, spi_host_device_t spiHost = SPI2_HOST);

    bool begin(uint32_t spiFreq = 40000000);   // Инициализация дисплея + SPI DMA
    void setRotation(uint8_t r);               // Поворот (0..3)
    void fillScreen(uint16_t color565);        // Заливка экрана
    void dmaDrawRGB888(int x, int y, int w, int h, const uint8_t* rgb);
    void drawPixel888(int x, int y, uint8_t r, uint8_t g, uint8_t b);
// Adafruit_GFX-style: RGB565 bitmap
void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h);
void drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h);

    // Чтобы проект компилировался с tft.print()
    virtual size_t write(uint8_t c) override;

private:
    spi_device_handle_t spi = nullptr;
    int8_t _cs, _dc, _rst;
    int _width = 320;
    int _height = 480;
    uint8_t rotation = 0;

    void initILI9488();
    void setAddrWindow(int x0, int y0, int x1, int y1);
    void writeCmd(uint8_t cmd);
    void writeData(uint8_t data);
    void writeDataDMA(const uint8_t* data, int len);
};
