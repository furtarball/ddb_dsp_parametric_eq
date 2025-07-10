CC ?= g++
CPPFLAGS = -DDDB_WARN_DEPRECATED=1 -g -fPIC -Wall -march=x86-64 -std=c++23 -static
SOX_CFLAGS = $(shell pkg-config --cflags sox)
SOX_LIBS = $(shell pkg-config --libs sox)
OBJS = parser.o ddb_dsp_parametric_eq.o
DESTDIR = ~/.local/lib/deadbeef/

all: $(OBJS)
	$(CC) $(CPPFLAGS) -shared -o ddb_dsp_parametric_eq.so parser.o ddb_dsp_parametric_eq.o $(SOX_LIBS)

parser.o: parser.h parser.cpp
	$(CC) $(CPPFLAGS) -c -o parser.o parser.cpp

ddb_dsp_parametric_eq.o: ddb_dsp_parametric_eq.cpp
	$(CC) $(SOX_CFLAGS) $(CPPFLAGS) -c -o ddb_dsp_parametric_eq.o ddb_dsp_parametric_eq.cpp

install:
	cp ddb_dsp_parametric_eq.so $(DESTDIR)

clean:
	rm $(OBJS) ddb_dsp_parametric_eq.so
