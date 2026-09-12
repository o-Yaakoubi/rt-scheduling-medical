CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
LDFLAGS = -lpthread -lm

TARGET  = medical_rtos
SRCS    = sensor.c scheduler_queue.c scheduler_fifo.c scheduler_edf.c metrics.c main.c
OBJS    = $(SRCS:.c=.o)
HDRS    = sensor.h scheduler.h metrics.h

.PHONY: all clean run-fifo run-edf

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

run-fifo: $(TARGET)
	./$(TARGET) --scheduler=FIFO --duration=10 --output=test_fifo.csv

run-edf: $(TARGET)
	./$(TARGET) --scheduler=EDF --duration=10 --output=test_edf.csv

clean:
	rm -f $(OBJS) $(TARGET) *.csv
