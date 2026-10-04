#pragma once
// Waveshare ESP32-C5-Touch-LCD-1.69, ST7789V2 240x280.
// Board LCD, I2C, codec and power pins are owned by the OFFICIAL Waveshare BSP.
// External pads marked TX / RX: U0TXD=GPIO11, U0RXD=GPIO12.
// We route UART1 to those pads; Serial must remain USB CDC (not UART0).
constexpr int LINK_TX=11, LINK_RX=12;
constexpr unsigned long LINK_BAUD=115200;
constexpr uint32_t LINK_TIMEOUT_MS=5000;
// Diagnostic only, disabled by default. Use headphones / low volume to avoid feedback.
#define ENABLE_AUDIO_LOOPBACK 0
