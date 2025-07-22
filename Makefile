# Makefile for Vulkan + SDL2 Engine

CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g -Isrc
LDFLAGS := -lvulkan -lSDL2 -lfmt

SRC_DIR := src
BUILD_DIR := build
BIN := vulkan_engine

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean run

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)
