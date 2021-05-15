#include <Arduino.h>
#include <I2C_eeprom.h>


// select tacho version

#define TACHO_VERSION           1
//#define TACHO_VERSION           2


#if TACHO_VERSION == 1

static const int mem_size = 256;
static I2C_eeprom ee(0x50, I2C_DEVICESIZE_24LC02);

#elif TACHO_VERSION == 2

static const int mem_size = 512;
static I2C_eeprom ee(0x50, I2C_DEVICESIZE_24LC04);

#endif


char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};



#if TACHO_VERSION == 1

// set odometer value in buffer
static void set_odometer (uint32_t x, uint8_t *p) {
    x = (x-14) / 2;

    for (unsigned int i=0; i<8; i++) {
        p[i*2] = ((x >> 3) & 0xFF) + (i < (x & 0x07) ? 1 : 0);
        p[i*2+1] = ((x >> 11) & 0xFF) + (i < (x & 0x07) && ((x & 0x7F8) == 0x7F8) ? 1 : 0);
    }
}

// get odometer value from buffer
static int get_odometer (uint32_t *x, uint8_t *p) {
    uint32_t tmp = 14;

    for (int i=0; i<8; i++) {
        tmp += ((uint32_t) p[i*2+0]) << 1;
        tmp += ((uint32_t) p[i*2+1]) << 9;
    }

    *x = tmp;
    return 0;
}

// print raw odometer data
static void print_odometer_data (void) {
    uint8_t data[16];
    ee.readBlock(16, data, 16);

    for (int i=0; i<16; i++) {
        Serial.print(hex_chars[(data[i]>>4) & 0x0F]);
        Serial.print(hex_chars[(data[i]>>0) & 0x0F]);

        if (i%4 == 3) {
            Serial.println();
        } else {
            Serial.print(' ');
        }
    }
}

// write new value to EEPROM
static void update_odometer (uint32_t x) {
    uint8_t data[16];
    set_odometer(x, data);
    ee.writeBlock(16, data, 16);
}

#elif TACHO_VERSION == 2

// calculate checksum for odometer value from buffer
static uint8_t chksum (uint8_t *p) {
    uint8_t sum = 0x5a;

    for (int i=0; i<3; i++) {
        sum -= p[i] & 0x0F;
        sum += ((p[i] >> 4) & 0x0F) * 0x0F;
    }

    return sum;
}

// set odometer value in buffer
static void set_odometer (uint32_t x, uint8_t *p) {
    for (int i=0; i<3; i++) {
        p[i] = x & 0xFF;
        x >>= 8;
    }

    p[3] = chksum(p);
}

// get odometer value from buffer
static int get_odometer (uint32_t *x, uint8_t *p) {
    uint32_t val = 0;
    for (int i=0; i<3; i++) {
        val <<= 8;
        val |= p[2-i];
    }

    uint8_t sum = chksum(p);
    if (p[3] != sum) {
        return -1;
    }

    return 0;
}

// print raw odometer data (all valid values found)
static void print_odometer_data (void) {
    for (int i=0; i<mem_size; i+=4) {
        uint32_t odometer;
        ee.readBlock(i, (uint8_t*) &odometer, 4);

        if (get_odometer(&odometer, (uint8_t*) &odometer) < 0) {
            continue;
        }

        for (int j=0; j<3; j++) {
            Serial.print(hex_chars[(i>>(12-(j+1)*4)) & 0x0F]);
        }

        Serial.print(" odometer: ");
        for (int j=0; j<8; j++) {
            Serial.print(hex_chars[(odometer>>(32-(j+1)*4)) & 0x0F]);
            if (j%2 == 1) {
                Serial.print(' ');
            }
        }

        Serial.print("-> ");
        Serial.print(odometer & 0x00FFFFFF);
        Serial.println();
    }
}

// write new value to EEPROM
static void update_odometer (uint32_t x) {
    uint8_t data[16];

    memset(data, 0xFF, 16);
    set_odometer(x, data);
    ee.writeBlock(0*16, data, 16);

    memset(data, 0xFF, 16);
    ee.writeBlock(1*16, data, 16);
    ee.writeBlock(2*16, data, 16);
    ee.writeBlock(16*16, data, 16);
    ee.writeBlock(17*16, data, 16);
    ee.writeBlock(18*16, data, 16);
}

#endif


// dump memory
static void dump_memory (void) {
    char buf[8];
    uint8_t data[16];

    for (int i=0; i<mem_size; i+=16) {
        ee.readBlock(i, data, 16);
        for (int j=0; j<16; j++) {
            sprintf(buf, "%02x", data[j]);
            Serial.println(buf);
        }
    }
}


// generate tacho signal
static void generate_tacho_signal (unsigned int speed_kmh) {
    uint32_t speed_delay_us = 1000000UL*186/200 / speed_kmh;

    for (;;) {
        digitalWrite(13, HIGH);
        delay(speed_delay_us/2 / 1000);
        delayMicroseconds((speed_delay_us/2) % 1000);

        digitalWrite(13, LOW);
        delay(speed_delay_us/2 / 1000);
        delayMicroseconds((speed_delay_us/2) % 1000);
    }
}


void setup (void) {
    Serial.begin(115200);

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    ee.begin();

    //print_odometer_data();
    //update_odometer(123456);
    //dump_memory();
    //generate_tacho_signal(42);

    digitalWrite(13, HIGH);
}


void loop (void) {}

