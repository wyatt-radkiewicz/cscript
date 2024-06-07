test: tester bs.h tester.c
	gcc -ansi -pedantic -g -O0 tester.c -o tester
	./tester

