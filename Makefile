all:
	g++ -std=c++17 main.cc -lsdl2 -lsdl2_ttf -Wall

debug:
	g++ -std=c++17 main.cc -lsdl2 -lsdl2_ttf -Wall -D DEBUG
