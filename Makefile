CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -O2
TARGET := m25p_sim
SRCS := main.cpp config_parser.cpp flash_hardware.cpp flash_event.cpp flash_core.cpp ftl.cpp flash_test.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) storage_m25p.bin
