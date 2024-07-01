#CXX=/home/user/HelinKern/scripts/helinroot/bin/i686-helin-g++
CXXFLAGS = -Iinclude
OBJECTS = src/main.o src/software_render.o src/sdl2_backend.o

all: $(OBJECTS)
	@$(CXX) $(OBJECTS) -o wm -lpng  -lz -lm
clean:
	rm -rf $(OBJECTS)
