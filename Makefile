CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lpthread

all: fileserver

fileserver: main.o mainfunctions.o
	$(CXX) main.o mainfunctions.o -o fileserver $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o fileserver server
