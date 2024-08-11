
#include <iostream>
#include <fstream>
#include <time.h>

//                          red     orange  yellow  green   blue    cyan    purple  black   white
const uint16_t colors[9] = {0xf800, 0xfc80, 0xffe0, 0x07e0, 0x001f, 0x07ff, 0xf81f, 0x0000, 0xffff};

uint8_t convert_board(std::ifstream&);
uint8_t convert_theme(std::ifstream&);

int main(){
	
	srand(time(NULL));
	
	std::ifstream sx("board_fmt.txt");
	
	std::string file,name;
	uint8_t choice{};
	
	for(; !choice; std::cin >> choice)
		std::cout << "Convert to (1) Board, (2) Theme: ";
		
	if(choice == '1')
		convert_board(sx), file = "_board_";
	else
		convert_theme(sx), file = "_theme_";
	
	
	sx.close();
	
	std::cout << "backup filename: ";
	std::cin >> name;
	
	std::ifstream src(file.substr(1, 5) + ".txt", std::ios::binary); 
	std::ofstream dst("backups\\" + name + file + std::to_string(rand()) + ".txt", std::ios::binary);

	dst << src.rdbuf();
	
	src.close(), dst.close();

}

uint8_t convert_board(std::ifstream& sx){
	
	uint16_t board[32]{};
	std::string s;
	
	for(uint8_t y{},x{}; getline(sx, s); ++y, x = 0)
		for(uint8_t c : s)
			board[x] |= ((c < 56) << y), ++x;
			
	FILE *fx;
	fx = fopen("board.txt", "w");		
	
	const char* fmt = "0x%x%s";
	fputs("uint16_t board[32] = {\n\t", fx);
	
	for(uint8_t x{}; x < 32; ++x)
		fprintf(fx, fmt, board[x], x < 31 ? "," : "\n};");

	return fclose(fx);
}

uint8_t convert_theme(std::ifstream& sx){
	
	uint32_t theme[16][3]{};
	std::string s;
	
	for(uint8_t y{},x{},b; getline(sx, s); ++y, x = 0)
		for(uint8_t c : s)
			for(b = 0, c &= 7; b < 3; ++b, c >>= 1, ++x)
				theme[y][x/32] |= (c&1) << (x%32);
	
	FILE *fx;
	fx = fopen("theme.txt", "w");
	
	const char* fmt = "{0x%x,0x%x,0x%x%s";
	fputs("const uint32_t theme[16][3] = {\n\t", fx);
	for(uint8_t x{}; x < 16; ++x)
		fprintf(fx, fmt, theme[x][0], theme[x][1], theme[x][2], x == 15 ? "}\n};" : "},");
	
	return fclose(fx);
}

