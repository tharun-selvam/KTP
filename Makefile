CC = gcc
CFLAGS = -Wall -g
AR = ar
ARFLAGS = rcs

LIB = libksocket.a
OBJS = ksocket.o

all: initprocess user1 user2

# Compile ksocket.c into an object file.
ksocket.o: ksocket.c ksocket.h
	$(CC) $(CFLAGS) -c ksocket.c

# Build the static library.
$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $(LIB) $(OBJS)

# Build initprocess (the init process that starts threads R and S).
initprocess: initksocket.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o initprocess initksocket.c $(LIB) -lpthread

# Build user1 executable.
user1: user1.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user1 user1.c $(LIB)

# Build user2 executable.
user2: user2.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user2 user2.c $(LIB)

# Clean up generated files.
clean:
	rm -f *.o $(LIB) initprocess user1 user2
