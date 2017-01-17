

CC = g++
FLAGS = -lsimlib -lm

all:	Cementarna

Cementarna: Cementarna.cpp
	$(CC) -o $@ Cementarna.cpp $(FLAGS)

run:
	./Cementarna 1
	./Cementarna 2
	./Cementarna 3
	./Cementarna 4
	./Cementarna 5
	./Cementarna 6
	./Cementarna 7
	./Cementarna 8
	./Cementarna 9
