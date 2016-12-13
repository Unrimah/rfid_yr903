.PHONY: all clean

all: yr903

clean:
	rm -rf yr903 *.o *~

yr903: yr903_main.cpp yr903.cpp
	g++ -Wall -Wextra -o yr903 yr903_main.cpp yr903.cpp
