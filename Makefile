CC = gcc
CFLAGS = -Wall -g
AR = ar
ARFLAGS = rcs

LIB = libksocket.a
OBJS = ksocket.o

all: initprocess user1 user2 user3 user4 user5 user6 usera userb

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

# Build user2 executable.
user3: user3.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user3 user3.c $(LIB)

# Build user2 executable.
user4: user4.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user4 user4.c $(LIB)

# Build user2 executable.
user5: user5.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user5 user5.c $(LIB)

# Build user2 executable.
user6: user6.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user6 user6.c $(LIB)

# Build user2 executable.
usera: usera.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o usera usera.c $(LIB)

# Build user2 executable.
userb: userb.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o userb userb.c $(LIB)

# Clean up generated files.
clean:
	rm -f *.o $(LIB) initprocess user1 user2 user3 user4 user5 user6 usera userb
	rm -rf *.dSYM