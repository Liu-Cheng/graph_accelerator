CC = g++
#CFLAGS= -DDEBUG -g -Wall -gdwarf-2 
CFLAGS= -g -std=c++11 -O3
#CFLAGS= -g -pg -O3
TARGET = bfs 
SRCS = bfs.cpp graph.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ 2>&1 | c++filt

.cpp.o:
	$(CC) -c $(CFLAGS) $< -o  $@

clean:
	rm -f $(OBJS) $(TARGET) core.* gmon.out

exe:
	./$(TARGET)

analysis: exe 
	gprof ./$(TARGET) | gprof2dot -s | dot -Tpdf -o analysis.pdf

cacheanalysis:
	valgrind --tool=cachegrind --branch-sim=yes ./$(TARGET)

depend:
	makedepend $(SRCS)

# DO NOT DELETE


