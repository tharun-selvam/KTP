CC = gcc
CFLAGS = -Wall -g
AR = ar
ARFLAGS = rcs

# If DEBUG is set to 1, add -DDEBUG to CFLAGS.
ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG
endif

LIB = libksocket.a
OBJS = ksocket.o

all: initprocess user1 user2 user3 user4 user5 user6 usera userb test1 test2

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

# Build user3 executable.
user3: user3.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user3 user3.c $(LIB)

# Build user4 executable.
user4: user4.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user4 user4.c $(LIB)

# Build user5 executable.
user5: user5.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user5 user5.c $(LIB)

# Build user6 executable.
user6: user6.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o user6 user6.c $(LIB)

# Build usera executable.
usera: usera.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o usera usera.c $(LIB)

# Build userb executable.
userb: userb.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o userb userb.c $(LIB)

# Build test1 executable.
test1: test1.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o test1 test1.c $(LIB)

# Build test2 executable.
test2: test2.c ksocket.h $(LIB)
	$(CC) $(CFLAGS) -o test2 test2.c $(LIB)

# Clean up generated files.
clean:
	rm -f *.o $(LIB) initprocess user1 user2 user3 user4 user5 user6 usera userb test1 test2
	rm -rf *.dSYM
