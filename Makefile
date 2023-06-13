CC = gcc
CFLAGS = -Wall -g
OBJ_DIR = obj

svsfs: $(OBJ_DIR)/shell.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/disk.o
	$(CC) $(OBJ_DIR)/shell.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/disk.o -o svsfs

$(OBJ_DIR)/shell.o: src/shell.c
	$(CC) $(CFLAGS) -c src/shell.c -o $(OBJ_DIR)/shell.o

$(OBJ_DIR)/fs.o: src/fs.c include/fs.h
	$(CC) $(CFLAGS) -c src/fs.c -o $(OBJ_DIR)/fs.o

$(OBJ_DIR)/disk.o: src/disk.c include/disk.h
	$(CC) $(CFLAGS) -c src/disk.c -o $(OBJ_DIR)/disk.o

clean:
	rm -f svsfs $(OBJ_DIR)/*.o
