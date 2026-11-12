EXERCISES	  += forest

SRC = forest.c forest_main.c forest_hash.c forest_6_bit.c
INCLUDES = forest.h

# if you add extra .c files, add them here
SRC += 

# if you add extra .h files, add them here
INCLUDES +=

forest:	$(SRC) $(INCLUDES)
	$(CC) $(SRC) -o $@
