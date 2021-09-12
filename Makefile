CXX = gcc-10
OBJS = build/main.o

mmake: build-dir $(OBJS)
	$(CXX) $(OBJS) -o mmake -llua5.3 -ldl -lm
	rm build/main.o

build/%.o: src/%.c
	$(CXX) -c $^ -o $@ -std=c11 -Iinclude

build-dir:
	mkdir -p build

clean:
	rm build/*
