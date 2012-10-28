all: disease

disease: main.c
	gcc -o disease main.c `sdl-config --cflags --libs`

clean:
	rm -f disease
