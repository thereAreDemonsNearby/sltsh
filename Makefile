release: shell.o expand.o calc_aslib.o nodes.o parse.o context.o
	g++ -o sltsh -g shell.o expand.o calc_aslib.o nodes.o parse.o context.o
shell.o: executor.h context.h expand.h parse.h shell.cpp
	g++ -std=c++17 -c -g shell.cpp -o shell.o
expand.o: expand.h expand.cpp context.h calc_aslib.h
	g++ -std=c++17 -c -g expand.cpp -o expand.o
calc_aslib.o: calc_aslib.h calc_aslib.cpp
	g++ -std=c++17 -c -g calc_aslib.cpp -o calc_aslib.o
nodes.o: nodes.h nodes.cpp executor.h
	g++ -std=c++17 -c -g nodes.cpp -o nodes.o
parse.o: parse.h nodes.h parse.cpp
	g++ -std=c++17 -c -g parse.cpp -o parse.o
context.o: context.h context.cpp
	g++ -std=c++17 -c -g context.cpp -o context.o

debug: shell.debug.o expand.debug.o calc_aslib.debug.o nodes.debug.o parse.debug.o context.debug.o
	g++ -o sltsh.debug -g shell.debug.o expand.debug.o calc_aslib.debug.o nodes.debug.o parse.debug.o context.debug.o
shell.debug.o: executor.h context.h expand.h parse.h shell.cpp
	g++ -std=c++17 -c -g shell.cpp -o shell.debug.o
expand.debug.o: expand.h expand.cpp context.h calc_aslib.h
	g++ -std=c++17 -c -g expand.cpp -o expand.debug.o
calc_aslib.debug.o: calc_aslib.h calc_aslib.cpp
	g++ -std=c++17 -c -g calc_aslib.cpp -o calc_aslib.debug.o
nodes.debug.o: nodes.h nodes.cpp executor.h
	g++ -std=c++17 -c -g nodes.cpp -o nodes.debug.o
parse.debug.o: parse.h nodes.h parse.cpp
	g++ -std=c++17 -c -g parse.cpp -o parse.debug.o
context.debug.o: context.h context.cpp
	g++ -std=c++17 -c -g context.cpp -o context.debug.o

clean:
	trash *.o *~ sltsh sltsh.debug
