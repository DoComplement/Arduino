
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
  0,      // new game
  A3,     // pause game
  A4,     // volume toggle
  A5      // rotate object
}, mv_btns[3] = {
  11,     // move right
  12,     // move left
  13      // move down
};

//                          red     orange  yellow  green   blue    cyan    purple  white   black
const uint16_t colors[9] = {0xf800, 0xfc80, 0xffe0, 0x07e0, 0x001f, 0x07ff, 0xf81f, 0xffff, 0x0000};

uint16_t board[32] = {
	0xffff,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0x8001,0xffff
},       objects4[3][4] = {
	{0x0039, 0x0096, 0x0138, 0x00d2},         // mirrored L
	{0x003c, 0x0192, 0x0078, 0x0093},         // L
	{0x003a, 0x00b2, 0x00b8, 0x009a}          // single-leg table? 2d round table?
}, 		   objects2[3][2] = {
	{0x0f00, 0x4444},	                        // straight leg, xor -> 0x4b44
	{0x00f0, 0x0132},	                        // squiggle 1,   xor -> 0x01c2
	{0x0033, 0x00b4}	                        // squiggle 2,   xor -> 0x0087
},
  obj_sq = 0x01b0, object, objset[4], objxor, objclr,
  px = 0x0108, npx,  // first 8-bits -> y, last 8-bits -> x
  moves[3] = {
   -1,          // +y, right
    1,          // -y, left
    0x0100      // +x, down
}, cutoff = 1000, period = 1000;

uint8_t objidx, objsz, rt;

void setup() {
  matrix.begin();
  
  #if defined(CONFIG_IDF_TARGET_ESP32S3)                                // enable the timers for ESP32-S3 rng
    uint32_t* G0T0_CTRL = (uint32_t*)0x6001F000;                        // Start ESP32S3's timer for rng
    uint32_t* G1T0_CTRL = (uint32_t*)0x60020000;

    *G0T0_CTRL |= (1<<31);
    *G1T0_CTRL |= (1<<31);
  #endif

  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT);

  for(const auto &pin : btns)
    pinMode(pin, INPUT_PULLUP);

  matrix.drawRect(0, 0, 32, 16, colors[7]);
  object = newObject();
  drawObject(1);
}

void loop() {

  for(npx = px; period && npx == px; --period, delay(1))
    for(const auto& mv_btn : mv_btns)
      if(mv_btn == 13 ? digitalRead(mv_btn) : !digitalRead(mv_btn))
        npx += moves[mv_btn - 11];

  if(!digitalRead(btns[1]))
    pauseGame();
  
  if(!period)
    npx += 0x0100, period = cutoff;

  // if "rotate" button is pressed and rotated object is valid
  if(!digitalRead(btns[3]) && validateMove(rotateObject(object), px)){
    object = rotateObject(object);                  // update object variable
    drawObject(1);                                  // draw rotated object on the board
  }

  // check if the next pixel origin, npx, is valid w.r.t the object
  if(validateMove(object, npx)){
    px = npx;                                    // update object origin pixel
    drawObject(1);                               // draw new object
  }

  checkRows();

  // check if there is an object beneath the object
  if(!validateMove(object, px + 0x0100)){
    checkFail();
    object = newObject();
  }

  drawObject(1);

  delay(100);
  period -= (period < 100 ? period : 100);
}

uint8_t checkRows(){
  uint8_t i{30};

  for(; board[i] == 0xffff; --i);

  // no rows are full
  if(i == 30)
    return 0;

  // swap and draw row
  for(uint8_t j{30},y; i > 1 && board[j] != 0x8001; --i, --j){

    if(board[j] == 0xffff)
      for(y = 1; y < 15; ++y)
        matrix.drawPixel(j, y, colors[7]), delay(50);

    board[j] = board[i];    // swap row

    // draw new row
    for(y = 1; y < 15; ++y)
      matrix.drawPixel(j, y, getBoardBit(i, y) ? colors[j%7] : 0);
  }
}

void checkFail(){
  uint8_t y;
  for(y = 1; y < 15; ++y)
    if(getBoardBit(1, y))
      break;

  if(y == 15)
    return;

  uint8_t ng;
  for(uint8_t x = 1; x < 31 && (ng = digitalRead(0)); ++x)
    if(getBoardBit(x, y))
      matrix.drawPixel(x, y, colors[7]), delay(50);
  
  while(ng)
    ng = digitalRead(0);
  
  delay(200);
  reset();
}

void pauseGame(){
  
  int8_t t,dx{},dy = 1;
  uint8_t x{},y{},cm = 16, ng;
  uint16_t clr{},pclr{};

  // snake around perimeter
  for(uint8_t rx{}; digitalRead(btns[3]) && (ng = digitalRead(btns[0])); ++rx){
    while(!(rx%4) && clr == pclr)
      clr = colors[ar.getRandomByte()%8];

    pclr = clr;

    for(cm = 16*(1 + (rx&1)); digitalRead(btns[3]) && (ng = digitalRead(btns[0])) && --cm; x += dx, y += dy)
      delay(50), matrix.drawPixel(x, y, clr);

    t = dy, dy = -dx, dx = t;
  }

  if(!ng)
    reset();
}

void reset(){
  for(auto &i : board)
    i = 0x8001;
  board[0] = board[31] = 0xffff;

  matrix.fillRect(1, 1, 30, 14, 0);

  object = newObject(), npx = 0;
  drawObject(1);
}

uint8_t validateMove(uint16_t tmpobj, uint16_t npx){

  // unset all the current object board bits to avoid overlapping sections of the next movement
  drawObject(0);

  // check all bits in the temp object
  for(uint8_t sz = objsz*objsz, i{}; i < sz; ++i)
    if((tmpobj&(1 << i)) && getBoardBit((npx >> 8) - i/objsz, (npx&15) + i%objsz)){
      drawObject(1);
      return false;
    }
  
  return true;
}

void drawObject(uint8_t use_clr){
  for(uint8_t sz = objsz*objsz, i{}; i < sz; ++i)
    if(object&(1 << i))
      setBoardBit((px >> 8) - i/objsz, (px&15) + i%objsz, use_clr);
}

uint16_t newObject(){
	uint8_t c1 = ar.getRandomByte()%7;
  objclr = colors[c1], objsz = (c1 == 3 ? 4 : 3);
	
	if(c1 < 3){
    px = 0x0207;
		objidx = objxor = 0;
		memcpy(objset, objects4[c1], 8);
		return objset[0];
	}
	if(c1 < 6){
		c1 -= 3;
    px = !c1 ? 0x0306 : ((2 + !(objects2[c1][0]&1)) << 8) + 0x7;

		objxor = objects2[c1][0]^objects2[c1][1];
		return objects2[c1][0];
	}

  px = 0x0307;
	return obj_sq;
}

uint16_t rotateObject(const uint16_t object){
	
	if(object == obj_sq)
		return object;
	
	if(objxor)
		return object^objxor;
	
	return objset[++objidx %= 4];
}

// bool has same quantity of bits with uint8_t, so might as well show the converion anyway
uint8_t getBoardBit(uint8_t x, uint8_t y){
  if(x >= 31 || y >= 15)
    return true;
  return (board[x] >> y)&1;
}

void setBoardBit(uint8_t x, uint8_t y, uint8_t b){
  b &= 1;                   // only take the first bit (should already be in this format)
  board[x] &= ~(1 << y);    // clear bit
  board[x] |=  (b << y);    // set bit

  matrix.drawPixel(x, y, b ? objclr : 0);
}
