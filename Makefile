# Compiler and flags
BUILD ?= debug
CXX = g++
ifeq ($(BUILD),release)
	CXXFLAGS = -std=c++20 -O3 -DNDEBUG -Isrc -DGLM_FORCE_DEPTH_ZERO_TO_ONE
else
	CXXFLAGS = -std=c++20 -g -O0 -Isrc -DGLM_FORCE_DEPTH_ZERO_TO_ONE
endif
LDFLAGS = -lvulkan -lSDL2 -lfmt -lsimdjson

# Directories
SRC_DIR = src
BUILD_DIR = build/$(BUILD)
TARGET = vulkan_engine

# Source files in src/
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))

# FastGLTF source files
FASTGLTF_SRC := fastgltf base64
FASTGLTF_OBJS := $(addprefix $(BUILD_DIR)/fastgltf_, $(addsuffix .o, $(FASTGLTF_SRC)))

# Default target
all: $(TARGET)
	./build-shaders.sh

# Link everything into the final binary
$(TARGET): $(OBJ_FILES) $(FASTGLTF_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile regular engine source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile FastGLTF source files (manually listed)
$(BUILD_DIR)/fastgltf_%.o: $(SRC_DIR)/fastgltf/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(shell mkdir -p $(BUILD_DIR))

# Run target
run: all
	./$(TARGET)

# Clean target
clean:
	rm -f $(BUILD_DIR)/*.o $(TARGET)
