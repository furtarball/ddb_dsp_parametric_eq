all: parser.o ddb_dsp_parametric_eq.o
	g++-13 -I/usr/local/include -DDDB_WARN_DEPRECATED=1 -g -shared -O3 -lsox -o ddb_dsp_parametric_eq.so parser.o ddb_dsp_parametric_eq.o -fPIC -Wall -march=native
parser.o: parser.h parser.cpp
	g++-13 -g -c -o parser.o parser.cpp -fPIC -Wall -march=native
ddb_dsp_parametric_eq.o: ddb_dsp_parametric_eq.cpp
	g++-13 -I/usr/local/include -DDDB_WARN_DEPRECATED=1 -std=c++23 -c -g -O3 -lsox -o ddb_dsp_parametric_eq.o ddb_dsp_parametric_eq.cpp -fPIC -Wall -march=native
install:
	cp ddb_dsp_parametric_eq.so ~/.local/lib/deadbeef/
