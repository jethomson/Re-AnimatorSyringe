#include <Arduino.h>
#include <EEPROM.h>

#define PERIOD 500
#define TICS_PER_SEC (1000 / PERIOD)

#define BREATH 1

#define LIB8STATIC __attribute__ ((unused)) static inline
#define FASTLED_RAND16_2053  ((uint16_t)(2053))
#define FASTLED_RAND16_13849 ((uint16_t)(13849))
#define RAND16_SEED  1337
uint16_t rand16seed = RAND16_SEED;

int pwmPin = 0;
uint8_t addr = 0;
uint8_t max_brightness = 255;
const uint8_t min_brightness = 2;
uint16_t tic = 0;

#define NUM_EFFECTS 3
enum EFFECT {NONE = 0, BREATHING = 1, FLICKER = 2};
EFFECT effect = BREATHING;

LIB8STATIC uint8_t random8() {
    rand16seed = (rand16seed * FASTLED_RAND16_2053) + FASTLED_RAND16_13849;
    // return the sum of the high and low bytes, for better
    //  mixing and non-sequential correlation
    return (uint8_t)(((uint8_t)(rand16seed & 0xFF)) +
           ((uint8_t)(rand16seed >> 8)));
}


LIB8STATIC uint8_t random8(uint8_t lim) {
    uint8_t r = random8();
    r = (r*lim) >> 8;
    return r;
}

LIB8STATIC uint8_t random8(uint8_t min, uint8_t lim) {
    uint8_t delta = lim - min;
    uint8_t r = random8(delta) + min;
    return r;
}


uint8_t finished_waiting(uint16_t interval) {
    uint8_t finished = 0;
    uint32_t dt = 0;
    static uint32_t pm = micros(); // previous micros
    dt = micros() - pm; // even when micros() overflows this still gives the correct time elapsed
    if (dt >= interval) {
        pm = micros();
        finished = 1;
    }
    return finished;
}

uint8_t breathing(uint16_t interval) {
    static uint8_t b = 0;
    //static uint8_t t = 0;
    static uint8_t dir = 1;
    if (finished_waiting(interval)) {
        //b = ((max_brightness - min_brightness) * abs((int8_t)(tic % TICS_PER_SEC) - (TICS_PER_SEC / 2))) / (TICS_PER_SEC / 2) + min_brightness;
        if (dir) {
            b++;
        }
        else {
            b--;
        }
        if (b == max_brightness) {
            dir = 0;
        }
        if (b == min_brightness) {
            dir = 1;
        }
    }
    return b;
}

uint8_t flicker(uint16_t interval) {
  static uint8_t b = max_brightness;
    if (finished_waiting(interval)) {
        b = ((random8(1,11) > 4)*max_brightness);
    }
    return b;
}


void setup() {
    uint8_t e = EEPROM.read(addr);
    e = (e+1) % NUM_EFFECTS;
    EEPROM.write(addr, e);
    effect = (EFFECT)e;

    pinMode(pwmPin, OUTPUT);
    max_brightness = 255;
}

void loop() {
    uint8_t b = 255;

    switch(effect) {
        default:
        // fall through to next case
        case NONE:
            b = max_brightness;
            break;
        case BREATHING:
            b = breathing(500);
            break;
        case FLICKER:
            b = flicker(6000);
            break;
    }
    analogWrite(pwmPin, b);
}
