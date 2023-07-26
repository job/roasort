all:
	cc -O2 -pipe -Wall -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wsign-compare -Werror-implicit-function-declaration -MD -MP -o roasort roasort.c -lc
	mandoc -Tlint roasort.8

install:
	install -c -s -o root -g bin -m 555 roasort /usr/local/bin/roasort
	install -c -o root -g bin -m 444 roasort.8 /usr/share/man/man8/roasort.8

clean:
	-rm -f roasort roasort.d

readme:
	mandoc -T markdown roasort.8 > README.md
