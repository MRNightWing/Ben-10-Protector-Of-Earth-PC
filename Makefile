# Makefile for Ben 10: Protector of Earth PC Port (Quantum Engine HAL)

CXX ?= C:/tools/w64devkit/bin/g++.exe
SDL2_DIR ?= C:/tools/SDL2/x86_64-w64-mingw32

CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Iinclude -I$(SDL2_DIR)/include
LDFLAGS  = -L$(SDL2_DIR)/lib -lmingw32 -lSDL2main -lSDL2 -mconsole

BUILD_DIR = build
BIN_DIR   = bin
TARGET    = $(BIN_DIR)/ben10_pc.exe

SRCS = src/main.cpp \
       src/core/engine.cpp \
       src/platform/psp_ctrl.cpp \
       src/platform/psp_display.cpp \
       src/platform/psp_kernel.cpp \
       src/video/renderer.cpp

OBJS = $(patsubfront src/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
# Manual obj list for reliable make handling across platforms
OBJS = $(BUILD_DIR)/main.o \
       $(BUILD_DIR)/core/engine.o \
       $(BUILD_DIR)/platform/psp_ctrl.o \
       $(BUILD_DIR)/platform/psp_display.o \
       $(BUILD_DIR)/platform/psp_kernel.o \
       $(BUILD_DIR)/video/renderer.o

all: dirs copy_dll $(TARGET)

dirs:
	@if not exist "$(BUILD_DIR)\core" mkdir "$(BUILD_DIR)\core"
	@if not exist "$(BUILD_DIR)\platform" mkdir "$(BUILD_DIR)\platform"
	@if not exist "$(BUILD_DIR)\video" mkdir "$(BUILD_DIR)\video"
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"

copy_dll:
	@if not exist "$(BIN_DIR)\SDL2.dll" copy "$(SDL2_DIR)\bin\SDL2.dll" "$(BIN_DIR)\SDL2.dll" >nul

$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo Build finished: $(TARGET)

clean:
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"

.PHONY: all dirs copy_dll clean
