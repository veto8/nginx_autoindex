.PHONY: api

build:
	gcc -O2 -o   files.cgi files.cgi.c
release:
	gcc -O2 -o   files.cgi files.cgi.c
default: build
