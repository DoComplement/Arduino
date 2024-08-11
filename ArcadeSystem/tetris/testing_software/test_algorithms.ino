
#include <RGBmatrixPanel.h>
#include <AlmostRandom.h>

#define CLK  8 
#define OE   9
#define LAT 10
#define A   A0
#define B   A1
#define C   A2

AlmostRandom ar;
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, false);

const uint8_t btns[] = {
  0,      //  new game
  11,     //  move object right
  12,     //  move object left
  A3,     //  pause game
  A4,     //  volume toggle
  A5      //  rotate object
}, down = 13;

//                          red     orange  yellow  green   blue    cyan    purple  white   black
const uint16_t colors[9] = {0xf800, 0xfc80, 0xffe0, 0x07e0, 0x001f, 0x07ff, 0xf81f, 0xffff, 0x0000};

uint16_t board[32] = {
	0xffff,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0xffff
};

const uint32_t theme[16][3] = {
	{0xffffffff,0xffffffff,0xffffffff},{0x24924927,0x49249249,0xf2492492},{0x24924927,0x49249249,0xf2492492},{0x49800127,0x46dc4928,0xf26da5b2},{0x49800127,0x38dc4928,0xf26da5b2},{0x21900927,0x38dc8949,0xf249a5b2},{0x21900927,0x38dc8949,0xf249a5b2},{0x49900927,0x46dc8948,0xf24da5b2},{0x9900927,0x48dc8949,0xf26d25b2},{0x21900927,0x46dc8949,0xf26925b2},{0x21900927,0x38dc8949,0xf26925b2},{0x49900927,0x38dc8948,0xf26da5b2},{0x49900927,0xb8dc8942,0xf26da5b1},{0x24924927,0x49249249,0xf2492492},{0x24924927,0x49249249,0xf2492492},{0xffffffff,0xffffffff,0xffffffff}
};


void setup() {
  matrix.begin();
  
  #if defined(CONFIG_IDF_TARGET_ESP32S3)                                // enable the timers for ESP32-S3 rng
    uint32_t* G0T0_CTRL = (uint32_t*)0x6001F000;                        // Start ESP32S3's timer for rng
    uint32_t* G1T0_CTRL = (uint32_t*)0x60020000;

    *G0T0_CTRL |= (1<<31);
    *G1T0_CTRL |= (1<<31);
  #endif

  pinMode(down, INPUT);
  for(const auto &pin : btns)
    pinMode(pin, INPUT_PULLUP);
  
  drawTheme();
}

void loop() {

}

void drawTheme(){
  
  // decode theme
  for(uint8_t n,b,x,y = 0; y < 16; ++y)
    for(x = 0; x < 96; matrix.drawPixel((x - 1)/3, y, colors[n]))
      for(b = n = 0; b < 3; ++b, ++x)
        n |= ((theme[y][x/32] >> (x%32))&1) << b;

  int8_t t,dx{},dy = 1;
  uint8_t x{},y{},cm = 16;
  uint16_t clr{},pclr{};

  // snake around perimeter
  for(uint8_t rx{}; digitalRead(btns[0]); ++rx){
    while(!(rx%4) && clr == pclr)
      clr = colors[ar.getRandomByte()%8];

    pclr = clr;

    for(cm = 16*(1 + (rx&1)); digitalRead(btns[0]) && --cm; x += dx, y += dy){
      delay(50);
      matrix.drawPixel(x, y, clr);
    }

    t = dy, dy = -dx, dx = t;
  }

  // reset screen
  matrix.drawRect(0, 0, 32, 16, colors[7]);
}
