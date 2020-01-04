DEPS := \
	Common.h \
	Cxbx.h \
	Error.h \
	Exe.h \
	Xbe.h
SRCS := \
	Common.cpp \
	Error.cpp \
	Exe.cpp \
	Logo.cpp \
	Xbe.cpp

OBJS := $(SRCS:.cpp=.o)

.PHONY: all
all: cxbe cdxt

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o '$@' '$<'

cxbe: Cxbe.cpp $(OBJS)
	$(CXX) -o '$@' $^

cdxt: Cdxt.cpp $(OBJS)
	$(CXX) -o '$@' $^

.PHONY: clean
clean:
	rm -f cxbe cdxt
