all: main.o

main.o: main.cpp
	g++ main.cpp -o main -pthread -std=c++17 -Wall

run: all
	./main

clean:
	rm -rf *.o main
