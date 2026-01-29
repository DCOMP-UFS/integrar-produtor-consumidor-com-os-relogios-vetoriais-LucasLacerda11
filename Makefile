# Makefile para Etapa 3 - Relógios Vetoriais + MPI + Threads

CC = mpicc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = etapa3

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	mpirun -np 3 ./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM

.PHONY: all run clean
