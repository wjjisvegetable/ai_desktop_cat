#pragma once
// Reconstructed assignment, NOT a record of the original wiring.
constexpr int SERVO_PINS[8] = {4,5,6,7,8,9,10,11};
constexpr const char *SERVO_NAMES[8] = {"head_yaw","head_pitch","ear_left","ear_right","paw_left","paw_right","spare_6","spare_7"};
constexpr int LIGHT_PIN=12, LIGHT_DARK_LEVEL=LOW;
constexpr int LINK_TX=17, LINK_RX=18;
constexpr unsigned long LINK_BAUD=115200;
constexpr const char *AP_SSID="DesktopCat";
constexpr const char *AP_PASSWORD="CatDraft2026"; // change before regular use
constexpr uint32_t PWM_HZ=50;
constexpr uint8_t PWM_BITS=14;
