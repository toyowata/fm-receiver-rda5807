/* mbed Microcontroller Library
 * Copyright (c) 2019-2024 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "USBSerial.h"
#include "SB1602E.h"

#define POLLING_INTERVAL        200ms
#define RDA5807_ADDR_SEQ        (0x10 << 1)
#define RDA5807_ADDR_IDX        (0x11 << 1)

USBSerial serial(false);
I2C fm(p24, p25);
SB1602E lcd(p24, p25);
InterruptIn btn(p5, PullUp);
InterruptIn v_up(p26, PullUp);
InterruptIn v_down(p29, PullUp);

uint8_t volume = 0;
int band_idx = 0;
int update_band = 0;
int update_vol = 0;

const uint16_t band[] = {
    800, // FM東京
    813, // J-WAVE
    825, // NHK FM
    897, // InternFM
    905, // TBSラジオ
    916, // 文化放送
    924, // ラジオ日本
    930  // ニッポン放送
};

void band_handler(void)
{
    update_band = 1;
}

void v_up_handler(void)
{
    if (volume < 15) {
        volume++;
    }
    update_vol = 1;
}

void v_down_handler(void)
{
    if (volume > 0) {
        volume--;
    }
    update_vol = 1;
}

void change_vol()
{
    char buf[3];
    char info[10];
    buf[0] = 0x05;
    buf[1] = 0x88;  // 0x05 H
    buf[2] = (0x80 | volume);  // 0x05 L LNAP, Volume
    fm.write(RDA5807_ADDR_IDX, buf , 3);
    sprintf(info, ":%02d", volume);
    lcd.printf(5, 1, info);
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

    sprintf(info, "%2d.%1dMHz", band[idx]/10, band[idx]%10);
    serial.printf("%s\n", info);
    lcd.printf(0, 0, info);
}

int main()
{
    char buf[10];

    lcd.setCharsInLine(8);
    lcd.clear();
    lcd.contrast(0x35);
    
    serial.printf("\n*** FMラジオ受信プログラム ***\n\n");
    btn.fall(band_handler);
    v_up.fall(v_up_handler);
    v_down.fall(v_down_handler);
        
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

    uint16_t chan =  band[0] - 760;
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
    volume = 10;
    change_vol();

    while (1) {
        if (update_band) {
            band_idx++;
            if (band_idx >= (int)(sizeof(band)/sizeof(uint16_t)) ) {
                band_idx = 0;
            }
            change_band(band_idx);
            update_band = 0;
        }
        if (update_vol) {
            change_vol();
            update_vol = 0;
        }

        char info[20];
        buf[0] = 0x0B;
        fm.write(RDA5807_ADDR_IDX, buf , 1);
        fm.read(RDA5807_ADDR_IDX, buf, 2);
        serial.printf("RSSI = 0x%02d", buf[0] >> 1);
        sprintf(info, "%02ddB :%02d", buf[0] >> 1, volume);
        lcd.printf(0, 1, info);

        ThisThread::sleep_for(POLLING_INTERVAL);
    }
}
