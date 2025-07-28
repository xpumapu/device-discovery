CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++11 -O2
LDFLAGS ?= 
LIBS = -lsqlite3 -lpcap -ljson-c

TARGET = device-discovery
SOURCES = main.cpp device_discovery.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)

.PHONY: all clean install