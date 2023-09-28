#definitions
DIR_INC = ./src
DIR_ABC_LIB = ./abc
DIR_ABC_INC = ./abc/src
DIR_ESP_LIB = ./espresso
DIR_ESP_INC = ./espresso/src
DIR_SRC = ./src
DIR_OBJ = ./obj
SOURCE  := $(wildcard ${DIR_SRC}/*.c) $(wildcard ${DIR_SRC}/*.cpp)
OBJS    := $(patsubst ${DIR_SRC}/%.c,${DIR_OBJ}/%.o,$(patsubst ${DIR_SRC}/%.cpp,${DIR_OBJ}/%.o,$(SOURCE)))
TARGET  := main

#compiling parameters
CC      := g++
LIBS    := -labc -lm -ldl -rdynamic -lreadline -ltermcap -lpthread -lstdc++ -lrt -lespresso
LDFLAGS := -L ${DIR_ABC_LIB} -L $(DIR_ESP_LIB)
DEFINES := $(FLAG) -DLIN64
INCLUDE := -I ${DIR_INC} -I ${DIR_ABC_INC} -I $(DIR_ESP_INC)
CFLAGS  := -g -Wall -O3 -std=c++17 $(DEFINES) $(INCLUDE)
CXXFLAGS:= $(CFLAGS)

#commands
.PHONY : all objs rebuild clean init ctag
all : init $(TARGET)

objs : init $(OBJS)

rebuild : clean all

clean :
	rm -rf $(DIR_OBJ)
	rm -f $(TARGET)
	rm -f tags

init :
	if [ ! -d obj ]; then mkdir obj; fi

ctag :
	ctags -R

$(TARGET) : $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

${DIR_OBJ}/%.o:${DIR_SRC}/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@
