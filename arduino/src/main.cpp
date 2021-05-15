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


static const char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};


static void print_hex4 (uint8_t x) {
    Serial.print(hex_chars[x & 0x0F]);
}

static void print_hex8 (uint8_t x) {
    print_hex4((x>>4) & 0x0F);
    print_hex4((x>>0) & 0x0F);
}


#if TACHO_VERSION == 1

// set odometer value in buffer
static void set_odometer (uint32_t x, uint8_t *p) {
    x = (x-14) / 2;

    // see README for description of the encoding
    for (unsigned int i=0; i<8; i++) {
        p[i*2] = ((x >> 3) & 0xFF) + (i < (x & 0x07) ? 1 : 0);
        p[i*2+1] = ((x >> 11) & 0xFF) + (i < (x & 0x07) && ((x & 0x7F8) == 0x7F8) ? 1 : 0);
    }
}

// get odometer value from buffer
static int get_odometer (uint32_t *x, uint8_t *p) {
    uint32_t tmp = 14;

    // see README for description of the encoding
    for (int i=0; i<8; i++) {
        tmp += ((uint32_t) p[i*2+0]) << 1;
        tmp += ((uint32_t) p[i*2+1]) << 9;
    }

    // TODO: verify value

    *x = tmp;
    return 0;
}

// print raw odometer data
static void print_odometer_data (void) {
    uint8_t data[16];
    ee.readBlock(16, data, 16);

    // print 4 bytes each line
    for (int i=0; i<16; i++) {
        print_hex8(data[i]);

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

    // calculate and set checksum
    p[3] = chksum(p);
}

// get odometer value from buffer
static int get_odometer (uint32_t *x, uint8_t *p) {
    uint32_t val = 0;
    for (int i=0; i<3; i++) {
        val <<= 8;
        val |= p[2-i];
    }

    // calculate and verify checksum
    uint8_t sum = chksum(p);
    if (p[3] != sum) {
        return -1;
    }

    return 0;
}

// print raw odometer data (all valid values found)
static void print_odometer_data (void) {
    uint8_t odometer_raw[4];
    uint32_t odometer;

    for (int i=0; i<mem_size; i+=4) {
        ee.readBlock(i, odometer_raw, 4);

        // checksum invalid?
        if (get_odometer(&odometer, odometer_raw) < 0) {
            continue;
        }

        // print address
        for (int j=0; j<3; j++) {
            print_hex4((i>>(12-(j+1)*4)) & 0x0F);
        }

        Serial.print(" odometer: ");

        // print raw data
        for (int j=0; j<4; j++) {
            print_hex8(odometer_raw[j]);
            Serial.print(' ');
        }

        Serial.print("-> ");

        // print odometer value
        Serial.print(odometer & 0x00FFFFFF);

        Serial.println();
    }
}

// write new value to EEPROM
static void update_odometer (uint32_t x) {
    uint8_t data[16];

    // write new value + clear remaining odometer values in this page
    memset(data, 0xFF, 16);
    set_odometer(x, data);
    ee.writeBlock(0*16, data, 16);

    // clear all other pages containing odometer values
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
    uint8_t data[16];

    for (int i=0; i<mem_size; i+=16) {
        ee.readBlock(i, data, 16);

        // print one byte each line
        for (int j=0; j<16; j++) {
            print_hex8(data[j]);
            Serial.println();
        }
    }
}


// generate tacho signal
static void generate_tacho_signal (unsigned int speed_kmh) {
    uint32_t speed_delay_us = 1000000UL*186/200 / speed_kmh;
    uint32_t speed_delay_us_half = speed_delay_us / 2;

    for (;;) {
        digitalWrite(13, HIGH);
        delay(speed_delay_us_half / 1000);
        delayMicroseconds(speed_delay_us_half % 1000);

        digitalWrite(13, LOW);
        delay(speed_delay_us_half / 1000);
        delayMicroseconds(speed_delay_us_half % 1000);
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

