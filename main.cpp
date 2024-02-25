/* mbed Microcontroller Library
 * Copyright (c) 2019-2024 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include <time.h>

#include "mbed.h"
#include "USBSerial.h"
#include "SB1602E.h"

#define POLLING_INTERVAL        500ms
#define RDA5807_ADDR_SEQ        (0x10 << 1)
#define RDA5807_ADDR_IDX        (0x11 << 1)

USBSerial serial(false);
I2C fm(p24, p25);
InterruptIn btn(p5);
SB1602E lcd(p24, p25);

uint8_t volume = 0;
int band_idx = 0;
int update = 0;
const uint16_t band[] = {
    795,
    800,
    813,
    825,
    905,
    916,
    924,
    930
};

void toggle(void)
{
    update = 1;
}

void change_band(int idx)
{
    char buf[3];
    char info[10];
    uint16_t chan = (band[idx] - 760);
    char h3 = chan >> 2;   // 0x03 H
    char l3 = chan & 0x03; // 0x03L
    l3 = l3 << 6;
    l3 = l3 | 0x18; // 0x03 L TUNE=1, BAND=10 (Japan), SPACE=00
    buf[0] = 0x03;  // index
    buf[1] = h3;    // 0x03 H freq
    buf[2] = l3;    // 0x03 L freq
    fm.write(RDA5807_ADDR_IDX, buf , 3);

    sprintf(info, "%2d.%d MHz", band[idx]/10, band[idx]%10);
    serial.printf("%s\n", info);
    lcd.printf(0, 0, info);
    sprintf(info, "vol: %02d", volume);
    lcd.printf(0, 1, info);
}

int main()
{
    char buf[10];

    lcd.setCharsInLine(8);
    lcd.clear();
    lcd.contrast(0x35);
    

    serial.printf("\n*** FMラジオ受信プログラム ***\n\n");
    btn.mode(PullUp);
    btn.fall(toggle);
    
    // read CHIPID
    buf[0] = 0;
    fm.write(RDA5807_ADDR_IDX, buf , 1);
    fm.read(RDA5807_ADDR_IDX, buf, 2);
    serial.printf("Chip ID = 0x%02x%02x\n\n", buf[0], buf[1]);

    buf[0] = 0; // 0x02 H DMUTE=0
    buf[1] = 0; // 0x02 L
    buf[2] = 0; // 0x03 H
    buf[3] = 0; // 0x03 L
    fm.write(RDA5807_ADDR_SEQ, buf , 4);

    uint16_t chan = (800) - 760;
    char h3 = chan >> 2;   // 0x03 H
    char l3 = chan & 0x03; // 0x03 L
    l3 = l3 << 6;
    l3 = l3 | 0x18; //0x03 L TUNE=1, BAND=10 (Japan), SPACE=00

    buf[0] = 0xc2; // 0x02 H DMUTE=1, seek-up
    buf[1] = 0x85; // 0x02 L CLK=32.768kHz, new methos, power up
    buf[2] = h3;   // 0x03 H freq
    buf[3] = l3;   // 0x03 L freq
    buf[4] = 0x0A; // 0x04 H DE=50us, softmute enable
    buf[5] = 0x00; // 0x04 L
    buf[6] = 0x88; // 0x05 H
    buf[7] = 0x80; // 0x05 L Volume 0
    fm.write(RDA5807_ADDR_SEQ, buf , 8);

    change_band(0);

    volume = 12;
    buf[0] = 0x05;
    buf[1] = 0x88;  // 0x05 H
    buf[2] = (0x80 | volume);  // 0x05 L LNAP, Volume
    fm.write(RDA5807_ADDR_IDX, buf , 3);

    update = 0;

    while (1) {
        if (update != 0) {
            band_idx++;
            if (band_idx >= (int)(sizeof(band)/sizeof(uint16_t)) ) {
                band_idx = 0;
            }
            change_band(band_idx);
            update = 0;
        }
        ThisThread::sleep_for(POLLING_INTERVAL);
    }
}
