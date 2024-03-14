all:
	cc -I/usr/local/include -std=c99 -g -shared -O2 -lsox -o ddb_dsp_parametric_eq.so ddb_dsp_parametric_eq.c -fPIC -Wall -march=native
install:
	cp ddb_dsp_parametric_eq.so ~/.local/lib/deadbeef/
