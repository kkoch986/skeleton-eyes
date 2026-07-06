#ifndef EYE_CONTROL_H
#define EYE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* I2C commands */
#define EYE_CMD_LOOK            0x01
#define EYE_CMD_BLINK           0x02
#define EYE_CMD_SQUINT          0x03
#define EYE_CMD_CURVE_PARAMS    0x06
#define EYE_CMD_STATUS          0x07
#define EYE_CMD_RESET           0x08
#define EYE_CMD_SCLERA_RGB      0x09
#define EYE_CMD_IRIS_RGB        0x0A
#define EYE_CMD_AUTO_BLINK      0x0B
#define EYE_CMD_IDLE            0x0C
#define EYE_CMD_JUMP            0x0D
#define EYE_CMD_SMOOTHING       0x0E
#define EYE_CMD_WIFI_SSID       0x0F
#define EYE_CMD_WIFI_PASS       0x10
#define EYE_CMD_WIFI_CONNECT    0x11
#define EYE_CMD_WIFI_FORGET     0x12
#define EYE_CMD_WIFI_STATUS     0x13
#define EYE_CMD_RESET_DEVICE    0x14
#define EYE_CMD_AUTO_BLINK_SPEED 0x15
#define EYE_CMD_SPRITE_MODE     0x16
#define EYE_CMD_GET_MODE        0x17
#define EYE_CMD_SET_SPRITE      0x18

#define EYE_DEFAULT_ADDR        0x42
#define EYE_SPRITE_NONE         255

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t squint;
    bool    external_control;
    bool    sprite_mode;
    bool    autonomous;
} eye_status_t;

/* I2C transport — implement these for your platform */
bool eye_transmit(uint8_t addr, const uint8_t *data, uint8_t len);
bool eye_request(uint8_t addr, uint8_t *buf, uint8_t len);

/* High-level API */
void    eye_init(uint8_t addr);
bool    eye_probe(uint8_t addr);
void    eye_look(int16_t x, int16_t y);
void    eye_jump(int16_t x, int16_t y);
void    eye_blink(uint16_t duration_ms);
void    eye_squint(uint8_t level);
void    eye_sclera_rgb(uint16_t rgb565);
void    eye_iris_rgb(uint16_t rgb565);
void    eye_curve_params(uint8_t falloff, uint8_t minimum, uint8_t strength);
void    eye_auto_blink(bool on);
void    eye_auto_blink_speed(uint16_t interval_ms);
void    eye_idle(void);
void    eye_smoothing(uint8_t level);
void    eye_sprite_mode(bool on);
void    eye_sprite_index(uint8_t index);
void    eye_reset(void);
void    eye_reset_device(void);
void    eye_wifi_ssid(const char *ssid);
void    eye_wifi_pass(const char *pass);
void    eye_wifi_connect(void);
void    eye_wifi_forget(void);
bool    eye_read_status(eye_status_t *status, uint8_t addr);
uint8_t eye_get_mode(uint8_t addr);

#ifdef __cplusplus
}
#endif

#endif /* EYE_CONTROL_H */
