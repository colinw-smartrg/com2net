
#CC = gcc
CFLAGS += -fPIC -Wall -O0 -g -I. -I../libappf

TARGET=com2net
SRC = com2net.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

all: $(TARGET)

$(DEP):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(DEP)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ -L../libappf -lappf

clean:
	-$(RM) $(TARGET) $(OBJ) $(DEP)
