#include "eye_control.h"

#include <Wire.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Default I2C transport (Arduino Wire)                              */
/* ------------------------------------------------------------------ */

#ifndef EYE_WIRE
#define EYE_WIRE Wire
#endif

static bool _transmit(uint8_t addr, const uint8_t *data, uint8_t len) {
    EYE_WIRE.beginTransmission(addr);
    for (uint8_t i = 0; i < len; i++)
        EYE_WIRE.write(data[i]);
    return EYE_WIRE.endTransmission() == 0;
}

static bool _request(uint8_t addr, uint8_t *buf, uint8_t len) {
    uint8_t got = EYE_WIRE.requestFrom((int)addr, (int)len);
    if (got < len) return false;
    for (uint8_t i = 0; i < len; i++)
        buf[i] = EYE_WIRE.read();
    return true;
}

/* ------------------------------------------------------------------ */
/*  Low-level helpers                                                 */
/* ------------------------------------------------------------------ */

static void _send1(uint8_t addr, uint8_t cmd) {
    uint8_t buf[1] = {cmd};
    _transmit(addr, buf, 1);
}

static void _send2(uint8_t addr, uint8_t cmd, uint8_t a) {
    uint8_t buf[2] = {cmd, a};
    _transmit(addr, buf, 2);
}

static void _send3(uint8_t addr, uint8_t cmd, uint8_t a, uint8_t b) {
    uint8_t buf[3] = {cmd, a, b};
    _transmit(addr, buf, 3);
}

static void _send4(uint8_t addr, uint8_t cmd, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t buf[4] = {cmd, a, b, c};
    _transmit(addr, buf, 4);
}

static void _send5(uint8_t addr, uint8_t cmd, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    uint8_t buf[5] = {cmd, a, b, c, d};
    _transmit(addr, buf, 5);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void eye_init(uint8_t addr) {
    (void)addr;
    EYE_WIRE.begin();
}

bool eye_probe(uint8_t addr) {
    EYE_WIRE.beginTransmission(addr);
    return EYE_WIRE.endTransmission() == 0;
}

void eye_look(int16_t x, int16_t y) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send5(addr, EYE_CMD_LOOK,
           (uint8_t)( x       & 0xFF),
           (uint8_t)((x >> 8) & 0xFF),
           (uint8_t)( y       & 0xFF),
           (uint8_t)((y >> 8) & 0xFF));
}

void eye_jump(int16_t x, int16_t y) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send5(addr, EYE_CMD_JUMP,
           (uint8_t)( x       & 0xFF),
           (uint8_t)((x >> 8) & 0xFF),
           (uint8_t)( y       & 0xFF),
           (uint8_t)((y >> 8) & 0xFF));
}

void eye_blink(uint16_t duration_ms) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send3(addr, EYE_CMD_BLINK,
           (uint8_t)( duration_ms        & 0xFF),
           (uint8_t)((duration_ms >> 8)  & 0xFF));
}

void eye_squint(uint8_t level) {
    _send2(EYE_DEFAULT_ADDR, EYE_CMD_SQUINT, level);
}

void eye_sclera_rgb(uint16_t rgb565) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send3(addr, EYE_CMD_SCLERA_RGB,
           (uint8_t)( rgb565       & 0xFF),
           (uint8_t)((rgb565 >> 8) & 0xFF));
}

void eye_iris_rgb(uint16_t rgb565) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send3(addr, EYE_CMD_IRIS_RGB,
           (uint8_t)( rgb565       & 0xFF),
           (uint8_t)((rgb565 >> 8) & 0xFF));
}

void eye_curve_params(uint8_t falloff, uint8_t minimum, uint8_t strength) {
    _send4(EYE_DEFAULT_ADDR, EYE_CMD_CURVE_PARAMS, falloff, minimum, strength);
}

void eye_auto_blink(bool on) {
    _send2(EYE_DEFAULT_ADDR, EYE_CMD_AUTO_BLINK, on ? 1 : 0);
}

void eye_auto_blink_speed(uint16_t interval_ms) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    _send3(addr, EYE_CMD_AUTO_BLINK_SPEED,
           (uint8_t)( interval_ms        & 0xFF),
           (uint8_t)((interval_ms >> 8)  & 0xFF));
}

void eye_idle(void) {
    _send1(EYE_DEFAULT_ADDR, EYE_CMD_IDLE);
}

void eye_smoothing(uint8_t level) {
    _send2(EYE_DEFAULT_ADDR, EYE_CMD_SMOOTHING, level);
}

void eye_sprite_mode(bool on) {
    _send2(EYE_DEFAULT_ADDR, EYE_CMD_SPRITE_MODE, on ? 1 : 0);
}

void eye_sprite_index(uint8_t index) {
    _send2(EYE_DEFAULT_ADDR, EYE_CMD_SET_SPRITE, index);
}

void eye_reset(void) {
    _send1(EYE_DEFAULT_ADDR, EYE_CMD_RESET);
}

void eye_reset_device(void) {
    _send1(EYE_DEFAULT_ADDR, EYE_CMD_RESET_DEVICE);
}

void eye_wifi_ssid(const char *ssid) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    size_t len = strlen(ssid);
    if (len > 32) len = 32;
    uint8_t buf[34] = {EYE_CMD_WIFI_SSID};
    memcpy(buf + 1, ssid, len);
    buf[1 + len] = 0;
    _transmit(addr, buf, 2 + len);
}

void eye_wifi_pass(const char *pass) {
    uint8_t addr = EYE_DEFAULT_ADDR;
    size_t len = strlen(pass);
    if (len > 64) len = 64;
    uint8_t buf[66] = {EYE_CMD_WIFI_PASS};
    memcpy(buf + 1, pass, len);
    buf[1 + len] = 0;
    _transmit(addr, buf, 2 + len);
}

void eye_wifi_connect(void) {
    _send1(EYE_DEFAULT_ADDR, EYE_CMD_WIFI_CONNECT);
}

void eye_wifi_forget(void) {
    _send1(EYE_DEFAULT_ADDR, EYE_CMD_WIFI_FORGET);
}

/* ------------------------------------------------------------------ */
/*  Read functions                                                     */
/* ------------------------------------------------------------------ */

bool eye_read_status(eye_status_t *status, uint8_t addr) {
    _send1(addr, EYE_CMD_STATUS);
    uint8_t buf[9];
    if (!_request(addr, buf, 9))
        return false;
    status->x                 = (int16_t)(buf[0] | (buf[1] << 8));
    status->y                 = (int16_t)(buf[2] | (buf[3] << 8));
    status->squint            = buf[4];
    status->external_control  = buf[5];
    status->sprite_mode      = buf[6];
    status->autonomous        = buf[7];
    return true;
}

uint8_t eye_get_mode(uint8_t addr) {
    _send1(addr, EYE_CMD_GET_MODE);
    uint8_t buf[1];
    if (!_request(addr, buf, 1))
        return 0xFF;
    return buf[0];
}
