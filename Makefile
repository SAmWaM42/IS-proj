cmp := g++
cflags := -Wall -Wextra -std=c++17 -MMD -MP
flags := -lraylib -lGL -lpthread -ldl -lrt -lX11
output := main

src := $(shell find . -type f -name "*.c++" -not -path "./mapmaker/*")

src := $(patsubst ./%,%,$(src))
objs := $(src:.c++=.o)
deps := $(src:.c++=.d)

all: $(output)

$(output): $(objs)
	$(cmp) $(objs) $(flags) -o $(output)

%.o: %.c++
	$(cmp) $(cflags) -c $< -o $@

-include $(deps)

.PHONY: clean
clean:
	rm -f $(objs) $(output) $(deps)