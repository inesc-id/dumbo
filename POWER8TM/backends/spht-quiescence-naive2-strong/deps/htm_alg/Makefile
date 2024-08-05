all:
	g++ -std=c++11 -c -O3 src/htm.cpp -o src/htm.o -I ../arch_dep/include -I ./include 
	ar rcs libhtm_sgl.a src/htm.o

clean:
	rm -f src/*.o *.a
