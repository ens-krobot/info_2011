OBJECTS = camfilter capture capturevideo findBallse fiteellipse_cam fiteellipse_camfilter
CC = g++
CFLAGS = -ggdb `pkg-config --cflags opencv`
LDFLAGS = `pkg-config --libs opencv`

all: $(OBJECTS)

clean:
	rm -f $(OBJECTS)
