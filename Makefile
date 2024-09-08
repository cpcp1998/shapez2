ALL : search4 lookup4 search5 lookup5

search4 : search.cpp shapez.hpp
	g++ -o search4 search.cpp -std=c++23 -O3

lookup4 : lookup.cpp shapez.hpp
	g++ -o lookup4 lookup.cpp -std=c++23 -O3

search5 : search.cpp shapez.hpp
	g++ -o search5 search.cpp -std=c++23 -O3 -DCONFIG_LAYER=5

lookup5 : lookup.cpp shapez.hpp
	g++ -o lookup5 lookup.cpp -std=c++23 -O3 -DCONFIG_LAYER=5

clean:
	rm search4 lookup4 search5 lookup5
