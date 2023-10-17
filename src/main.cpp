#include <Arduino.h>
#include <EEPROM.h>

// a single latching button is used to power on/off and select the effect.
// when the uc boots the last effect used is read from the EEPROM and the next effect is determined from that.
// max EEPROM size for attiny is 512.
// setting EFFECT_ADDR_UB to 256 leaves room for other data.
// by cycling through addresses we can wear out a cell more slowly than writing to the same address every time.
// this should change number of possible boots from around 100,000 to (EFFECT_ADDR_UB/2)*100,000.
#define EFFECT_ADDR_UB (EEPROM.length()/2)

#define PERIOD 500
#define TICS_PER_SEC (1000 / PERIOD)

#define BREATH 1

#define LIB8STATIC __attribute__ ((unused)) static inline
#define FASTLED_RAND16_2053  ((uint16_t)(2053))
#define FASTLED_RAND16_13849 ((uint16_t)(13849))
#define RAND16_SEED  1337
uint16_t rand16seed = RAND16_SEED;

#if defined(__AVR_ATmega328P__)
int pwmPin = 3;
#elif defined(__AVR_ATtiny85__)
int pwmPin = 0;
#endif


uint8_t addr = 0;
uint8_t max_brightness = 255;
const uint8_t min_brightness = 2;
uint16_t tic = 0;

#define NUM_EFFECTS 3
enum EFFECT {NONE = 0, BREATHING = 1, FLICKER = 2};
EFFECT effect = BREATHING;
//EFFECT effect = NONE;

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


// read a comment that micros() malfunctions at 1 MHz, but it works fine for me.
uint8_t finished_waiting(uint32_t interval) {
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


void get_effect() {
    uint8_t e = 0;
    uint16_t addr = 0;
    for (; addr < EFFECT_ADDR_UB; addr++) {
        uint8_t d = EEPROM.read(addr);
        // the default value of a byte in an erased EEPROM is 255
        // anything less than the 255 indicates datum was written to this address
        // the data are stored in an inverted form so they are closer to 255
        // which means fewer bits need to be change when setting an address back to the default (255)
        if (d < 255) {
           e = (~d) - 1; // e.g. (~254) - 1 = 0
           break; 
        }
    }

    if (addr == EFFECT_ADDR_UB) {
        // no data was found so use default values
        e = 0;
        addr = 0;
    }

    e = (e+1) % NUM_EFFECTS;
    effect = (EFFECT)e;

    EEPROM.update(addr, 255); // update only writes the value if it differs from the stored value
    EEPROM.write((addr+1) % EFFECT_ADDR_UB, (~e) - 1);
}

// sanity check after change clock frequency
//void blink_timing_debug01() {
//    digitalWrite(pwmPin, HIGH);
//    delay(1000);
//    digitalWrite(pwmPin, LOW);
//    delay(1000);
//}

// sanity check after change clock frequency
// make sure our time elapsed function still works
//void blink_timing_debug02() {
//    // 1000000 microseconds is 1 second
//    if (finished_waiting(1000000)) {
//        digitalWrite(pwmPin, !digitalRead(pwmPin));
//    }
//}


void setup() {
    //Serial.begin(4800); // cannot have a faster baud rate at 1 MHz
    //Serial.println(EFFECT_ADDR_UB, DEC);

    get_effect();
    pinMode(pwmPin, OUTPUT);
    max_brightness = 255;

    //https://playground.arduino.cc/Main/TimerPWMCheatsheet/
    //Pins 11 and 3: controlled by timer 2 in phase-correct PWM mode (cycle length = 510)
    TCCR2B = (TCCR2B & 0b11111000) | 0x01; // divisor of 1 at 1 MHz = PWM frequency of 1960.784375 Hz

    //TODO: can we make this code work for attiny85 too? different timer.

    //uint8_t run_debug_num = 2;
    //while(run_debug_num > 0) {
    //    if (run_debug_num == 1) {
    //        blink_timing_debug01();
    //    }
    //    else {
    //        blink_timing_debug02();
    //    }
    //}
}

void loop() {
    static uint8_t b_prev = 0;
    uint8_t b = max_brightness;

    switch(effect) {
        default:
        // fall through to next case
        case NONE:
            // running at 1MHz results in a lower PWM frequency. slow enough that the duty cycle causes visible flicker
            // a value of 32 will cause flicker if the timer's divisor is left at 64
            //b = 32; // for PWM flicker debugging
            b = max_brightness;
            break;
        case BREATHING:
            b = breathing(4000);
            break;
        case FLICKER:
            b = flicker(48000);
            break;
    }

    if (b != b_prev) {
        // only write if the value is new
        b_prev = b;
        analogWrite(pwmPin, b);
    }
}
