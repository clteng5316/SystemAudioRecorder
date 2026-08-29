CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -pthread
LDFLAGS ?= -pthread
TARGET = SystemAudioRecorder
SOURCES = main.cpp audio.cpp spectrum.cpp ffmpeg.cpp downloader.cpp queue.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(OBJECTS) $(TARGET)
.PHONY: all clean
