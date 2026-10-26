CXX      = g++
CXXFLAGS = -std=c++11 -O2 -Wall -I.
LIBS     = -lGLEW -lglfw -lGL

SRC = main.cpp Scene.cpp Effects.cpp CameraPath.cpp Camera.cpp Mesh.cpp ShaderProgram.cpp Texture2D.cpp
OBJ = $(SRC:.cpp=.o)

night_fair: $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# sous WSL, sans GALLIUM_DRIVER=d3d12 on tombe sur llvmpipe (rendu CPU, tres lent)
run: night_fair
	GALLIUM_DRIVER=d3d12 ./night_fair

clean:
	rm -f $(OBJ) night_fair

.PHONY: run clean
