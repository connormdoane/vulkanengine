# Makefile for Vulkan + SDL2 Engine

CXX := g++
CXXFLAGS := -std=c++20 -g -Isrc
LDFLAGS := -lvulkan -lSDL2 -lfmt

SRC_DIR := src
BUILD_DIR := build
BIN := vulkan_engine

# Shader setup
SHADER_DIR := shaders
SHADER_BUILD_DIR := $(BUILD_DIR)/shaders
COMP_SHADERS := $(wildcard $(SHADER_DIR)/*.comp)
SPV_FILES := $(patsubst $(SHADER_DIR)/%.comp,$(SHADER_BUILD_DIR)/%.comp.spv,$(COMP_SHADERS))

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Shader compilation rules
$(SHADER_BUILD_DIR)/%.comp.spv: $(SHADER_DIR)/%.comp
	@mkdir -p $(SHADER_BUILD_DIR)
	glslangValidator -V $< -o $@

.PHONY: clean run

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)
