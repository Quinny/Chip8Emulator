all:
	g++ -std=c++17 main.cc -lsdl2 -Wall

debug:
	g++ -std=c++17 main.cc -lsdl2 -Wall -D DEBUG
