# Default target
all: game

# Compile game.cpp into executable "game"
game: game.cpp
	g++ game.cpp -o game -pthread -std=c++17 -Wall -lncurses

# Run the program
run: game
	./game

# Clean compiled files
clean:
	rm -f game *.o
