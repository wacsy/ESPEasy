#include <LOLIN_EPD.h>
#include <Adafruit_GFX.h>

const unsigned char gImage_num1[128] PROGMEM = {
    /* 0X02,0X01,0X20,0X00,0X20,0X00, */
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xDF,
    0xFF,
    0xF7,
    0xFF,
    0xDF,
    0xFF,
    0xF7,
    0xFF,
    0xDF,
    0xFF,
    0xF7,
    0xFF,
    0xDF,
    0xFF,
    0xE7,
    0xFF,
    0x80,
    0x00,
    0x07,
    0xFF,
    0x00,
    0x00,
    0x07,
    0xFF,
    0x00,
    0x00,
    0x07,
    0xFF,
    0xFF,
    0xFF,
    0xE7,
    0xFF,
    0xFF,
    0xFF,
    0xF7,
    0xFF,
    0xFF,
    0xFF,
    0xF7,
    0xFF,
    0xFF,
    0xFF,
    0xF7,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
};

const unsigned char gImage_num2[128] PROGMEM = {
    /* 0X02,0X01,0X20,0X00,0X20,0X00, */
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xE1,
    0xFF,
    0xC7,
    0xFF,
    0xC1,
    0xFF,
    0x87,
    0xFF,
    0x99,
    0xFF,
    0x27,
    0xFF,
    0x3F,
    0xFE,
    0x67,
    0xFF,
    0x7F,
    0xFC,
    0xE7,
    0xFF,
    0x7F,
    0xF9,
    0xE7,
    0xFF,
    0x7F,
    0xF3,
    0xE7,
    0xFF,
    0x7F,
    0xE7,
    0xE7,
    0xFF,
    0x3F,
    0xCF,
    0xE7,
    0xFF,
    0x1F,
    0x1F,
    0xE7,
    0xFF,
    0x80,
    0x3F,
    0xC7,
    0xFF,
    0x80,
    0x7E,
    0x07,
    0xFF,
    0xE0,
    0xFE,
    0x1F,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
};

/*D1 mini*/
#define EPD_CS D0
#define EPD_DC D8
#define EPD_RST -1  // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY -1 // can set to -1 to not use a pin (will wait a fixed delay)

/*D32 Pro*/
// #define EPD_CS 14
// #define EPD_DC 27
// #define EPD_RST 33  // can set to -1 and share with microcontroller Reset!
// #define EPD_BUSY -1 // can set to -1 to not use a pin (will wait a fixed delay)

LOLIN_IL3897 EPD(250, 122, EPD_DC, EPD_RST, EPD_CS, EPD_BUSY); //hardware SPI

// #define EPD_MOSI D7
// #define EPD_CLK D5
// LOLIN_IL3897 EPD(250,122, EPD_MOSI, EPD_CLK, EPD_DC, EPD_RST, EPD_CS, EPD_BUSY); //IO

void setup()
{
  EPD.begin();
  EPD.clearBuffer();
  EPD.setTextColor(EPD_BLACK);
  EPD.println("hello world!");
  // EPD.display();
  // delay(2000);
  EPD.partInit();
  delay(100);
}

void loop()
{
  // for (uint8_t i = 0; i < 4; i++)
  // {
  //   EPD.setRotation(i);
  //   EPD.clearBuffer();

  //   EPD.setCursor(0,0);
  //   EPD.println("hello world!");
  //   EPD.println(EPD.getRotation());
  //   EPD.display();
  //   delay(2000);
  // }

  EPD.partDisplay(0, 32, gImage_num1, 32, 32);
  EPD.partDisplay(0, 32, gImage_num2, 32, 32);
  delay(2000);
}
