OBJECTS = camfilter capture capturevideo findBallse fiteellipse_cam fiteellipse_camfilter calibration
CC = g++
CFLAGS = -ggdb `pkg-config --cflags opencv`
LDFLAGS = `pkg-config --libs opencv`
PREFIX = $(HOME)

all: $(OBJECTS)

clean:
	rm -f $(OBJECTS)

install:
	install -m 0755 findBallse $(PREFIX)/bin/krobot-find-objects
