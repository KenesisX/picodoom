# Raspberry Pi 5 / any Linux:  sudo apt install build-essential libsdl2-dev && make && ./picodoom
picodoom: pi_main.cpp ../../main.cpp
	g++ -O2 -std=c++17 -I. -I../.. -DPD_PORT -DPD_SAVE=0 -DPD_SOUND=0 pi_main.cpp ../../main.cpp -o picodoom $(shell sdl2-config --cflags --libs)
