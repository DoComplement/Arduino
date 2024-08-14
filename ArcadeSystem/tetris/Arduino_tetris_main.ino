
#include <RGBmatrixPanel.h>				// RGB matrix panel	-	https://github.com/adafruit/RGB-matrix-Panel
#include <AlmostRandom.h>					// AlmostRandom			- https://github.com/cygig/AlmostRandom

#define CLK  8 
#define OE   9
#define LAT 10
#define A   A0
#define B   A1
#define C   A2

AlmostRandom ar;
RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, false);

const uint8_t 
  reset_btn  = 0,
  pause_btn  = A3,
  volume_tgl = A4,
  rotate_btn = A5,
  move_right = 11,
  move_left  = 12,
  move_down  = 13;


const uint16_t//  red  orange  yellow   green    blue    cyan  purple   black   white 
  colors[9] = {0xf800, 0xfda0, 0xffe0, 0x07e0, 0x001f, 0x07ff, 0xf81f, 0x0000, 0xffff},
  shadow[7] = {0x2000, 0xfa40, 0xfa40, 0x0120, 0x0004, 0x0004, 0x4809},
  objects4[3][4] = {                              // "four states of" ... (object)
	{0x0039, 0x0096, 0x0138, 0x00d2},               // mirrored L
	{0x003c, 0x0192, 0x0078, 0x0093},               // L
	{0x003a, 0x00b2, 0x00b8, 0x009a}                // single-leg table? 2d round table?
}, 
  objects2[3][2] = {                              // "two states of" ... (object)
	{0x00f0, 0x2222},	                              // straight leg, xor -> 0x22d2
	{0x00f0, 0x0132},	                              // squiggle 1,   xor -> 0x01c2
	{0x0033, 0x00b4}	                              // squiggle 2,   xor -> 0x0087
},
  obj_sq = 0x01b0;

const uint32_t theme[16][3] = {
	{0xffffffff,0xffffffff,0xffffffff},{0xffffffff,0xffffffff,0xffffffff},{0xffffffff,0xffffffff,0xffffffff},{0x49e001ff,0xf6df492e,0xffedbdbf},{0x49e001ff,0xbedf492e,0xffedbdbf},{0xf9fc0fff,0xbedfe97f,0xffffbdbf},{0xf9fc0fff,0xbedfe97f,0xffffbdbf},{0x49fc0fff,0xf6dfe97e,0xfffdbdbf},{0xc9fc0fff,0xfedfe97f,0xffedfdbf},{0xf9fc0fff,0xf6dfe97f,0xffeffdbf},{0xf9fc0fff,0xbedfe97f,0xffeffdbf},{0x49fc0fff,0xbedfe97e,0xffedbdbf},{0x49fc0fff,0xbedfe972,0xffedbdbd},{0xffffffff,0xffffffff,0xffffffff},{0xffffffff,0xffffffff,0xffffffff},{0xffffffff,0xffffffff,0xffffffff}
};

uint16_t 
  board[32]{},                                                    // matrix cell states
  object, objset[4]{}, objxor, objclr, sclr,                      // object variables
  px, spx, npx,                                                   // first 8-bits -> y, last 8-bits -> x
  period, cutoff, qck_prd;                                        // period variables

uint8_t dwn, dly_prd, objidx, objsz, rt_period, delay_period{100};

void setup() {
  matrix.begin();
  
  #if defined(CONFIG_IDF_TARGET_ESP32S3)                                // enable the timers for ESP32-S3 rng
    uint32_t* G0T0_CTRL = (uint32_t*)0x6001F000;                        // Start ESP32S3's timer for rng
    uint32_t* G1T0_CTRL = (uint32_t*)0x60020000;

    *G0T0_CTRL |= (1<<31);
    *G1T0_CTRL |= (1<<31);
  #endif

  pinMode(reset_btn,  INPUT_PULLUP);
  pinMode(pause_btn,  INPUT_PULLUP);
  pinMode(volume_tgl, INPUT_PULLUP);
  pinMode(rotate_btn, INPUT_PULLUP);

  pinMode(move_right, INPUT_PULLUP);
  pinMode(move_left,  INPUT_PULLUP);
  pinMode(move_down,  INPUT);

  drawTheme();                                                          // draw main theme
}

void loop() {

  npx = inputPeriod(period, npx, px, 1);                                              // get user input during input period

  if(!period)                                                                         // if period is empty -> user gave no input
    npx += 0x0100, period = cutoff;                                                   // drop next origin pixel down one pixel, reset period
  else
    dwn = (npx >> 8) > (px >> 8);                                                     // check if user moved the object down

  // check if the next pixel origin, npx, is valid w.r.t the object
  if(validateMove(object, npx, true)){
    clearShadow(object, spx);                                                         // clear current shadow
    drawObject(object, px = npx, true);                                               // update object origin pixel and draw new object
  }

  // if the object can't move down anymore
  if(!validateMove(object, px + 0x0100, true)){

    clearShadow(object, spx);                                                         // clear current shadow
    npx = inputPeriod(qck_prd = delay_period, npx, px, false);                        // allow user to move either left or right within {50 <= qck_prd <= 100}
    
    if(validateMove(object, npx, true))
      drawObject(object, npx, true);

    if(!checkRows())                                                                  // check if any rows are full
      checkFail();                                                                    // check if any columns are full (if zero rows were cleared)
    
    object = newObject();                                                             // update 'object' variable with new object
  }

  drawObject(object, px, true);                                                       // draw object regardless

  // if the user successfully moved the object down
  dly_prd = (dwn && px == npx) ? 20 : delay_period;                                   // 50 <= delay_period <= 100

  delay(dly_prd);
  period -= (period < dly_prd ? period : dly_prd);
}

// main input period for user to move the object
uint16_t inputPeriod(uint16_t& period, uint16_t& npx, uint16_t& px, uint8_t read_down){
  for(npx = px, rt_period = 0; period && npx == px; --period, rt_period -= (rt_period > 0), delay(1)){
    if(!digitalRead(pause_btn))                                                                           // if pause button is pressed
      pauseGame();

    // if "rotate" button is pressed and rotated object is valid
    if(!rt_period && !digitalRead(rotate_btn) && validateMove(rotateObject(object), px, true)){
      clearShadow(object, spx);                                                                           // clear current shadow
      object = rotateObject(object);                                                                      // update object variable
      drawObject(object, px, true);                                                                       // draw rotated object on the board
      rt_period = 500, ++objidx %= 4;                                                                     // clear rotate_period, increment current object sequence idx
    }

    npx -= !digitalRead(move_right);
    npx += !digitalRead(move_left);

    if(read_down && digitalRead(move_down))
      npx += 0x0100;
  }
  
  return npx;
}


// check if a row is full, will only be called when a new object is being generated -> current object can't move down
uint8_t checkRows(){

  uint8_t changed = false;

  for(uint8_t i{3},ti,j,y; i < 31; ++i){
    if(board[i] != 0xffff)                                                              // if row is not full, continue
      continue;

    changed = true;                                                                     // indicates row is cleared
    delay_period -= (delay_period > 50);                                                // decrease delay period
    cutoff -= (cutoff > 200 ? 5 : 0);                                                   // decrease input period

    for(y = 1; y < 15; ++y)                                                             // make row white -> indicates it is being cleared
      matrix.drawPixel(i, y, colors[8]), delay(50);

    for(ti = i, j = i - 1; j > 1 && board[ti] != 0x8001; --ti, --j){                    // swap and draw rows above full row
      board[ti] = board[j];                                                             // swap row

      for(y = 1; y < 15; ++y)                                                           // draw above row on current row
        matrix.drawPixel(ti, y, getBoardBit(j, y) ? colors[ti%7] : 0);
    }
  }

  return changed;
}

// will check if a column is full -> game over
void checkFail(){
  uint8_t y;
  for(y = 1; y < 15; ++y)                                                         // check if any cell is set in the top row
    if(getBoardBit(1, y))                                                         // break if a cell is set -> y < 15
      break;

  if(y == 15)                                                                     // y == 15 -> no cell is set
    return;

  uint8_t ng;
  for(uint8_t x = 1; x < 31 && (ng = digitalRead(reset_btn)); ++x)                // draw specific column that caused the game to end
    if(getBoardBit(x, y))
      matrix.drawPixel(x, y, colors[8]), delay(50);
  
  while(ng)                                                                       // wait until the user starts a new game (pushed new game button)
    ng = digitalRead(reset_btn);
  
  delay(200);                                                                     // delay before new game
  reset();                                                                        // start new game
}

// shows a snake around the perimeter (while) to indicate the game is paused
void pauseGame(){
  
  while(!digitalRead(pause_btn));                                            // wait until the pause button is released
  
  int8_t t,dx{},dy = 1;
  uint8_t x{},y{},cm = 16, ng{1};
  uint16_t clr{},pclr{};

  // snake around perimeter
  for(uint8_t rx{}; digitalRead(pause_btn) && (ng = digitalRead(reset_btn)); ++rx){
    while(!(rx%4) && clr == pclr)                                           // get new color every loop
      clr = colors[ar.getRandomByte()%8];

    pclr = clr;                                                             // update previoud color for next color generation

    for(cm = 16*(1 + (rx&1)); digitalRead(pause_btn) && (ng = digitalRead(reset_btn)) && --cm; x += dx, y += dy)
      delay(50), matrix.drawPixel(x, y, clr);                               // draw pixel

    t = dy, dy = -dx, dx = t;                                               // update dx,dy variables
  }

  while(!digitalRead(pause_btn));                                           // wait until the pause button is released

  matrix.drawRect(0, 0, 32, 16, colors[8]);                                 // reset the perimeter
  if(!ng)                                                                   // if the new game button was pressed during the pause period 
    reset();
}

// will start a new game
void reset(){
  for(auto &i : board)
    i = 0x8001;                                               // reset all board elements to the boundary
  board[0] = board[31] = 0xffff;                              // reset last and first board elements

  matrix.drawRect(0, 0, 32, 16, colors[8]);                   // draw white perimeter -> boundary
  matrix.fillRect(1, 1, 30, 14, 0);                           // clear center of board

  cutoff = 1000, delay_period = 100;                          // reset periods
  object = newObject();                                       // get new object
  drawObject(object, px, true);                               // draw new object
}

// checks if a move on tmpobj is valid -> if tmpobj at origin pixel npx is valid (doesn't overlap with other objects)
uint8_t validateMove(const uint16_t& tmpobj, const uint16_t npx, const uint8_t clear_object){

  if(clear_object)
    drawObject(object, px, false);                                                              // clear object on board if indicated to do so

  // check all bits in the temp object
  for(uint8_t sz = objsz*objsz, i{}, x = npx >> 8, y = npx&15; i < sz; ++i)
    if((tmpobj&(1 << i)) && getBoardBit(x - i/objsz, y + i%objsz)){                             // if cell is set both the object and the board (overlap)
      if(clear_object)
        drawObject(object, px, true);                                                           // re-draw object if object was previously cleared
      return false;                                                                             // indicate the move is not valid
    }
  
  return true;                                                                                  // indicate the move is valid
}

// draws the object on the 
void drawObject(const uint16_t& object, const uint16_t& px, uint8_t use_clr){
  if(use_clr)                                                                                   // if the object is being drawn instead of cleared
    spx = drawShadow(object, px);                                                               // show where the object will land

  for(uint8_t sz = objsz*objsz, i{}, x = px >> 8, y = px&15; i < sz; ++i)                       // loop through every cell in the object
    if(object&(1 << i))                                                                         // if cell is set in the object
      setBoardBit(x - i/objsz, y + i%objsz, use_clr);                                           // set cell on the board
}

// will show where an object will land (lowest valid y-position an object can occupy)
uint16_t drawShadow(const uint16_t& object, uint16_t tpx){

  while(validateMove(object, tpx + 0x0100, false))                                                // check if the object can move down (without clearing the object)
    tpx += 0x0100;                                                                                // increment the origin pixel one cell downward

  for(uint8_t sz = objsz*objsz, i{}, x = tpx >> 8, y = tpx&15; i < sz; ++i)                       // loop through every cell in the object
    if(object&(1 << i))                                                                           // if cell is set in the object
      matrix.drawPixel(x - i/objsz, y + i%objsz, sclr);                                           // draw the pixel on the board (without setting)

  return tpx;                                                                                     // return the origin pixel of the shadow for later clearing
}

// clears all the unset leds of an object w.r.t to the shadow pixel origin
void clearShadow(const uint16_t& object, const uint16_t& spx){
  for(uint8_t sz = objsz*objsz, i{}, x = spx >> 8, y = spx&15; i < sz; ++i)
    if((object&(1 << i)) && !getBoardBit(x - i/objsz, y + i%objsz))                               // if bit is set in the object and bit is not set on the board
      matrix.drawPixel(x - i/objsz, y + i%objsz, 0);                                              // clear the pixel on the board
}

// will generate a new object and return that object
uint16_t newObject(){
	uint8_t c1 = ar.getRandomByte()%7;                                          // random number between 0-7
  objclr = colors[c1], sclr = shadow[c1], objsz = (c1 == 3 ? 4 : 3);          // object color, shadow color, object cell size (3x3, 4x4)
	
	if(c1 < 3){                                                                 // objects with four rotational states
    px = 0x0207;                                                              // origin cell (0x02, 0x07)
		objidx = objxor = 0;                                                      // rotational state = 0, xor = 0 -> rotational state object
    memcpy(objset, objects4[c1], 8);                                          // copy rotational states to 'objset'
		return objset[0];                                                         // return first rotational state
	}
	if(c1 < 6){                                                                 // objects with two rotational states
		c1 -= 3;                                                                  // offset
    px = !c1 ? 0x0206 : ((2 + !(objects2[c1][0]&1)) << 8) + 0x7;              // calculating the origin cell

		objxor = objects2[c1][0]^objects2[c1][1];                                 // xor between rotational states
		return objects2[c1][0];                                                   // return first rotational state
	}

  px = 0x0307;                                                                // origin cell for square
	return obj_sq;                                                              // square
}

// return a rotated version of the object
uint16_t rotateObject(const uint16_t& object){
	
	if(object == obj_sq)                                  // square
		return object;
	
	if(objxor)                                            // object has only two states
		return object^objxor;
	
	return objset[objidx];                                // return object's next rotated state
}

// bool has same quantity of bits with uint8_t, so might as well show the converion anyway
uint8_t getBoardBit(const uint8_t x, const uint8_t y){
  if(x >= 31 || y >= 15)                                    // true if index is out of bounds
    return true;
  return (board[x] >> y)&1;                                 // return the bit state on the board
}

void setBoardBit(const uint8_t x, const uint8_t y, uint8_t b){
  b &= 1;                                                   // only take the first bit (should already be in this format)
  board[x] &= ~(1 << y);                                    // clear bit
  board[x] |=  (b << y);                                    // set bit

  matrix.drawPixel(x, y, b ? objclr : 0);                   // draw/clear led on board
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
  for(uint8_t rx{}; digitalRead(reset_btn); ++rx){
    while(!(rx%4) && clr == pclr)
      clr = colors[ar.getRandomByte()%8];

    pclr = clr;

    for(cm = 16*(1 + (rx&1)); digitalRead(reset_btn) && --cm; x += dx, y += dy){
      delay(50);
      matrix.drawPixel(x, y, clr);
    }

    t = dy, dy = -dx, dx = t;
  }

  reset();
}
