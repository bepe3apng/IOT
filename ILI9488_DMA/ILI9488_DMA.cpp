#include "ILI9488_DMA.h"
#include <driver/gpio.h>
#include <driver/spi_master.h>

ILI9488_DMA::ILI9488_DMA(int8_t cs, int8_t dc, int8_t rst, spi_host_device_t spiHost)
: _cs(cs), _dc(dc), _rst(rst)
{
}

bool ILI9488_DMA::begin(uint32_t spiFreq)
{
    // Configure DC pin
    gpio_set_direction((gpio_num_t)_dc, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_dc, 1);

    // SPI bus config
    spi_bus_config_t buscfg = {
        .mosi_io_num = 23,
        .miso_io_num = -1,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 480 * 320 * 3 + 32
    };

    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = spiFreq;
    devcfg.mode           = 0;
    devcfg.spics_io_num   = _cs;
    devcfg.queue_size     = 4;
    devcfg.flags          = SPI_DEVICE_NO_DUMMY;


    spi_bus_add_device(SPI2_HOST, &devcfg, &spi);

    // Reset
    if (_rst >= 0) {
        gpio_set_direction((gpio_num_t)_rst, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)_rst, 1);
        delay(10);
        gpio_set_level((gpio_num_t)_rst, 0);
        delay(20);
        gpio_set_level((gpio_num_t)_rst, 1);
        delay(120);
    }

    initILI9488();
    return true;
}

void ILI9488_DMA::writeCmd(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)_dc, 0);

    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(spi, &t);
}

void ILI9488_DMA::writeData(uint8_t data)
{
    gpio_set_level((gpio_num_t)_dc, 1);

    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    spi_device_transmit(spi, &t);
}

void ILI9488_DMA::writeDataDMA(const uint8_t* data, int len)
{
    gpio_set_level((gpio_num_t)_dc, 1);

    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_transmit(spi, &t); // DMA handles transfer
}

void ILI9488_DMA::initILI9488()
{
    writeCmd(0x11);   // Sleep OUT
    delay(120);

    writeCmd(0x3A);   // Interface Pixel Format
    writeData(0x66);  // 18-bit RGB888

    writeCmd(0x36);   // Memory Access
    writeData(0x48);

    writeCmd(0x29);   // Display ON
    delay(20);
}

void ILI9488_DMA::setAddrWindow(int x0, int y0, int x1, int y1)
{
    writeCmd(0x2A);
    writeData(x0 >> 8); writeData(x0);
    writeData(x1 >> 8); writeData(x1);

    writeCmd(0x2B);
    writeData(y0 >> 8); writeData(y0);
    writeData(y1 >> 8); writeData(y1);

    writeCmd(0x2C);
}

void ILI9488_DMA::setRotation(uint8_t r)
{
    rotation = r & 3;

    writeCmd(0x36);

    switch (rotation) {
        case 0: writeData(0x48); _width=320; _height=480; break;
        case 1: writeData(0x28); _width=480; _height=320; break;
        case 2: writeData(0x88); _width=320; _height=480; break;
        case 3: writeData(0xE8); _width=480; _height=320; break;
    }
}

void ILI9488_DMA::fillScreen(uint16_t color565)
{
    uint8_t r = (color565 & 0xF800) >> 8;
    uint8_t g = (color565 & 0x07E0) >> 3;
    uint8_t b = (color565 & 0x001F) << 3;

    setAddrWindow(0, 0, _width - 1, _height - 1);

    static uint8_t buf[1024*3];
    for (int i = 0; i < 1024; i++) {
        buf[i*3+0] = r;
        buf[i*3+1] = g;
        buf[i*3+2] = b;
    }

    int pixels = _width * _height;
    while (pixels > 0) {
        int chunk = min(1024, pixels);
        writeDataDMA(buf, chunk * 3);
        pixels -= chunk;
    }
}

void ILI9488_DMA::dmaDrawRGB888(int x, int y, int w, int h, const uint8_t* rgb)
{
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    writeDataDMA(rgb, w * h * 3);
}
void ILI9488_DMA::drawPixel888(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;

    setAddrWindow(x, y, x, y);

    uint8_t pixel[3] = { r, g, b };
    writeDataDMA(pixel, 3);
}
size_t ILI9488_DMA::write(uint8_t c) {
    // пока просто выводим текст в Serial,
    // чтобы проект компилировался и хоть где-то было видно сообщения
    Serial.write(c);
    return 1;
}
void ILI9488_DMA::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
    drawRGBBitmap(x, y, (const uint16_t*)bitmap, w, h);
}

void ILI9488_DMA::drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
{
    if (!bitmap || w <= 0 || h <= 0) return;

    int16_t x2 = x + w - 1;
    int16_t y2 = y + h - 1;

    // Полностью вне экрана
    if (x >= _width || y >= _height || x2 < 0 || y2 < 0) return;

    // Клиппинг
    int16_t xstart = (x < 0) ? 0 : x;
    int16_t ystart = (y < 0) ? 0 : y;
    int16_t xend   = (x2 >= _width)  ? (_width  - 1) : x2;
    int16_t yend   = (y2 >= _height) ? (_height - 1) : y2;

    int16_t cw = xend - xstart + 1;
    int16_t ch = yend - ystart + 1;

    // Окно + команда RAMWR (0x2C) внутри setAddrWindow()
    setAddrWindow(xstart, ystart, xend, yend);

    // Стартовый указатель в исходном (неклипнутом) bitmap
    const uint16_t *srcBase = bitmap + (ystart - y) * w + (xstart - x);

    // Максимальная ширина у тебя 480 (см. setRotation), значит 480*3 = 1440 байт
    uint8_t lineBuf[480 * 3];

for (int16_t row = 0; row < ch; row++) {
    const uint16_t *src = srcBase + row * w;
    int idx = 0;

    for (int16_t col = 0; col < cw; col++) {
        uint16_t c = src[col];

        // Если в RGB565 использовался byte swap — раскомментировать
         c = (c << 8) | (c >> 8);

        uint8_t r5 = (c >> 11) & 0x1F;
        uint8_t g6 = (c >> 5)  & 0x3F;
        uint8_t b5 =  c        & 0x1F;

        // Масштабирование с репликацией бит (правильная обратная операция)
        lineBuf[idx++] = (r5 << 3) | (r5 >> 2);  // R
        lineBuf[idx++] = (g6 << 2) | (g6 >> 4);  // G
        lineBuf[idx++] = (b5 << 3) | (b5 >> 2);  // B
    }

    writeDataDMA(lineBuf, cw * 3);
}

}
