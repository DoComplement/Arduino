
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

const uint8_t btns[6] = {
  0,      //  new game
  11,     //  move object right
  12,     //  move object left
  A3,     //  pause game
  A4,     //  volume toggle
  A5      //  rotate object
}, mv_btns[3] = {11,12,13};

//                          red     orange  yellow  green   blue    cyan    purple  white   black
const uint16_t colors[9] = {0xf800, 0xfc80, 0xffe0, 0x07e0, 0x001f, 0x07ff, 0xf81f, 0xffff, 0x0000};
uint16_t board[32] = {
	0xffff,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0xffff
};

// two ways, draw the board, or draw the pixel
// board -> ?
// pixel -> moving a single object

uint16_t px = 0x0108, npx,  // first 8-bits -> y, last 8-bits -> x
  moves[3] = {
   -1,          // +y, right
    1,          // -y, left
    0x0100      // +x, down
};

void setup() {
  Serial.begin(115200);
  matrix.begin();
  
  #if defined(CONFIG_IDF_TARGET_ESP32S3)                                // enable the timers for ESP32-S3 rng
    uint32_t* G0T0_CTRL = (uint32_t*)0x6001F000;                        // Start ESP32S3's timer for rng
    uint32_t* G1T0_CTRL = (uint32_t*)0x60020000;

    *G0T0_CTRL |= (1<<31);
    *G1T0_CTRL |= (1<<31);
  #endif

  pinMode(13, INPUT);
  for(const auto &pin : btns)
    pinMode(pin, INPUT_PULLUP);

  matrix.drawRect(0, 0, 32, 16, colors[7]);
  matrix.drawPixel(px >> 8, px&15, getColor(1, px));
  setBoardBit(px >> 8, px&15, 1);
}

uint16_t cutoff = 1000, period = 1000;

void loop() {

  for(npx = px; period && npx == px; --period, delay(1))
    for(const auto& mv_btn : mv_btns)
      if(mv_btn == 13 ? digitalRead(mv_btn) : !digitalRead(mv_btn))
        npx += moves[mv_btn - 11];

  if(!period)
    npx += 0x0100, period = cutoff;

  if(!getBoardBit(npx >> 8, npx&15))                                        // if next lower pixel is valid, not filled
    px = nextCell(px, npx);
  
  if(!checkRows())                                                          // check if any rows need to be cleared
    checkCols();

  if(getBoardBit(1 + (px >> 8), px&15))                                     // if cell below new pixel is filled, generate new pixel
    px = newCell(px);

  delay(10);
  period -= (period < 100 ? period : 100);
}

// bool has same quantity of bits with uint8_t, so might as well show the converion anyway
uint8_t getBoardBit(uint8_t x, uint8_t y){
  return (board[x] >> y)&1;
}

void setBoardBit(uint8_t x, uint8_t y, uint8_t b){
  b &= 1;                   // only take the first bit (should already be in this format)
  board[x] &= ~(1 << y);    // clear bit
  board[x] |=  (b << y);    // set bit
}

uint16_t getColor(uint8_t mode, uint16_t px){
  if(!mode)
    return colors[ar.getRandomByte()%7];                        // random

  return colors[mode == 1 ? (px&15)%7 : (px >> 8)%7];                // linear-x/y
}

uint16_t nextCell(uint16_t px, uint16_t npx){
  matrix.drawPixel(px >> 8, px&15, 0);                                // clear previous led on board
  setBoardBit(px >> 8, px&15, 0);                                     // clear previous bit in memory

  matrix.drawPixel(npx >> 8, npx&15, getColor(1, npx));               // set next led on board
  setBoardBit(npx >> 8, npx&15, 1);                                   // set next bit on board

  return npx;
}

uint16_t newCell(uint16_t px){
  px = 0x0108;

  matrix.drawPixel(px >> 8, px&15, getColor(1, px));                  // set new led on board
  setBoardBit(px >> 8, px&15, 1);                                     // set new bit in memory

  return px;
}

void checkCols(){

  uint8_t y{1};
  for(uint8_t x; y < 15; ++y){
    for(x = 1; x < 31 && getBoardBit(x, y); ++x);
    if(x == 31)
      break;
  }

  if(y == 15)
    return;

  uint8_t ng;
  for(uint8_t x{1}; x < 31 && (ng = digitalRead(btns[0])); ++x)
    matrix.drawPixel(x, y, colors[7]), delay(50);

  for(; ng; delay(1))
    ng = digitalRead(btns[0]);

  delay(90);
  reset();
}

uint8_t checkRows(){
  uint8_t i{30};

  for(; board[i] == 0xffff; --i);

  // no rows are full
  if(i == 30)
    return 0;

  // swap and draw row
  for(uint8_t j{30},y; i > 1 && board[j] != 0x8001; --i, --j){
    board[j] = board[i];    // swap row

    // draw new row
    for(y = 1; y < 15; ++y)
      matrix.drawPixel(j, y, getBoardBit(i, y) ? getColor(1, (j << 8)|y) : 0);
  }

  return 1;
}

void reset(){
  for(auto &i : board)
    i = 0x8001;
  board[0] = board[31] = 0xffff;

  matrix.fillRect(1, 1, 30, 14, 0);
  px = newCell(px);
}
