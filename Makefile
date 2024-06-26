CC = g++
CPPFLAGS = -Wall -O2 -pedantic -std=c++11

NP_SHELL_CMD_SRC = $(wildcard src/cmd/*.cpp)
NP_SHELL_CMD = $(patsubst src/cmd/%.cpp,work_template/bin/%,$(NP_SHELL_CMD_SRC))

FILE_NAME = $(wildcard src/file/*)
FILE = $(patsubst src/file/%,work_template/%,$(FILE_NAME))

BUILD_IN_CMD_NAME = ls cat wc
BUILD_IN_CMD = $(patsubst %,work_template/bin/%,$(BUILD_IN_CMD_NAME))

.PHONY: work

all: work
	@rm -rf work_dir
	@cp -r work_template work_dir

%: src/%.cpp
	$(CC) $< $(CPPFLAGS) -o $@

work: work_template/ $(FILE) $(NP_SHELL_CMD) $(BUILD_IN_CMD)

work_template/bin/%: src/cmd/%.cpp | work_template/bin/
	$(CC) $< $(CPPFLAGS) -o $@

work_template/bin/%: /bin/%
	cp -f $< $@

work_template/bin/:
	mkdir work_template/bin/

work_template/%: src/file/%
	cp $< $@

work_template/:
	mkdir work_template

clean:
	rm -f $(NP_SHELL)
	rm -rf work_dir
	rm -rf work_template
