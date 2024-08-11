
#include <iostream>
#include <bitset>
#include <time.h>
#include <cstring>

using std::cout;
using std::cin;

uint16_t objects4[3][4] = {
	{0x0039, 0x0096, 0x0138, 0x00d2},
	{0x003c, 0x0192, 0x0078, 0x0093},
	{0x003a, 0x00b2, 0x00b8, 0x009a}
}, 		 objects2[3][2] = {
	{0x00f0, 0x2222},	// 0xidk
	{0x00f0, 0x0132},	// 0x01c2
	{0x0033, 0x00b4}	// 0x0087
},		 obj_sq = 0x01b0;

uint16_t px, object, objset[4], objidx, objxor;

uint16_t getNewObject();
uint16_t rotate_object(const uint16_t);
void convert_object(const uint16_t);

int main(){
	
	srand(time(NULL));
	
	object = getNewObject();
	cout << std::hex;
	for(int i{}; i < 5; ++i){
		cout << "0x" << object << ", ";
		object = rotate_object(object);
	}
	
	
}

uint16_t getNewObject(){
	uint8_t c1 = rand()%7;
	
	if(c1 < 3){
		px = 0x0207;
		objidx = objxor = 0;
		memcpy(objset, objects4[c1], 8);
		return objset[0];
	}
	if(c1 < 6){
		c1 -= 3;
    
    if(!c1)
      px = 0x0306;
    else
      px = ((2 + !(objects2[c1][0]&1)) << 8) + 0x7;

		objxor = objects2[c1][0]^objects2[c1][1];
		return objects2[c1][0];
	}

	px = 0x0307;
	return obj_sq;
}

uint16_t rotate_object(const uint16_t object){
	
	if(object == obj_sq)
		return object;
	
	if(objxor)
		return object^objxor;
	
	return objset[++objidx %= 4];
}

void convert_object(const uint16_t object){
	uint8_t sz = (object >> 9) ? 4 : 3;
	for(uint16_t i = (1 << (sz*sz - 1)),j = sz*sz; j--; i >>= 1)
		cout << ((object&i) >> j) << (!(j%sz) ? '\n' : '\0');
	cout << '\n';
}
