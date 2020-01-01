INCLUDES := \
	Common.h \
	Cxbx.h \
	Error.h \
	Exe.h \
	Xbe.h
SRCS := \
	Main.cpp \
	Common.cpp \
	Error.cpp \
	Exe.cpp \
	Logo.cpp \
	Xbe.cpp

.PHONY: all
all: cxbe cdxt

cxbe: $(SRCS) $(INCLUDES)
	$(CXX) -o '$@' $(SRCS)

cdxt: Cdxt.cpp $(SRCS) $(INCLUDES)
	$(CXX) -o '$@' $^

.PHONY: clean
clean:
	rm -f cxbe cdxt
