

const int8_t btns[] = {
  0, 11, 12, A3, A4, A5
};

const int8_t spkr = 1, btn = 13, sz = sizeof(btns);
int8_t x;

void setup() {

  // pinMode(spkr, OUTPUT);
  // pinMode(btn, INPUT); // will illuminate the LED regardless because the physical btn is tied to Vdd

  pinMode(btn, OUTPUT);
  for(const auto& pin : btns)
    pinMode(pin, INPUT_PULLUP);

}

void loop() {
  
  x = 1;
  for(const auto &pin : btns)
    x &= digitalRead(pin);

  digitalWrite(btn, !x);


  /*
  if(digitalRead(btn) | !x)
    tone(spkr, 1000, 1);
  */

}
