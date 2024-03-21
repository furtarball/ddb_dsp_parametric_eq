all: parser.o ddb_dsp_parametric_eq.o
	g++ -I/usr/local/include -std=c99 -DDDB_WARN_DEPRECATED=1 -g -shared -O2 -lsox -o ddb_dsp_parametric_eq.so parser.o ddb_dsp_parametric_eq.o -fPIC -Wall -march=native
parser.o: parser.h parser.cpp
	g++ -g -c -o parser.o parser.cpp -fPIC -Wall -march=native
ddb_dsp_parametric_eq.o: ddb_dsp_parametric_eq.c
	cc -I/usr/local/include -std=c99 -DDDB_WARN_DEPRECATED=1 -c -g -O2 -lsox -o ddb_dsp_parametric_eq.o ddb_dsp_parametric_eq.c -fPIC -Wall -march=native
install:
	cp ddb_dsp_parametric_eq.so ~/.local/lib/deadbeef/
