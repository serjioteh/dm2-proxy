all:
	 g++ -std=c++11 -o proxy proxy.cpp config_parser.cpp -I/usr/local/include -L/usr/local/lib -levent

clean:
	rm echo

