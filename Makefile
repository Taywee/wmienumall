#DEBUG := TRUE
LIBS = 
FLAGS += -std=c++17 -MMD -MP -Wall -Wextra -municode
CFLAGS += -D_WIN32_WINNT=0x0700 -DWINVER=0x0700 -DBUILDING_WMIENUMALL_DLL
LDFLAGS += -static-libgcc -static-libstdc++ -lwbemuuid -lkernel32 -lole32 -loleaut32 -shared

ifdef DEBUG
FLAGS += -O0 -ggdb
CFLAGS += -DDEBUG
else
FLAGS += -O2
CFLAGS += -DNDEBUG
LDFLAGS += -s
endif

CXX = x86_64-w64-mingw32-g++
STRIP = x86_64-w64-mingw32-strip

COMPILE = $(CXX) $(CFLAGS) $(FLAGS) -c
LINK = $(CXX) $(LDFLAGS) $(FLAGS)

SOURCES = wmienumall.cxx
OBJECTS =  $(SOURCES:.cxx=.o)
DEPENDENCIES = $(OBJECTS:.o=.d)

LIBRARY = wmienumall.dll

.PHONY: all clean

all: $(LIBRARY)
-include $(DEPENDENCIES)
clean:
	-rm -v $(OBJECTS) $(DEPENDENCIES) $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(CXX) -o$@ $^ $(LDFLAGS) $(FLAGS)

%.o : %.cxx
	$(CXX) -c -o $@ $< $(CFLAGS) $(FLAGS)
