override CFLAGS := -Wall -Werror $(CFLAGS)
LDLIBS = -lcurl -lz

all: zurl

clean:
	rm -f *.o out.* zurl

.PHONY: all clean
