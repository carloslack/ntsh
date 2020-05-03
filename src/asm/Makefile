
test64:
	gcc -c -o test64.o test64.S
	gcc -c -o jmp64.o jmp64.S
	ld -o test64 test64.o
	ld -o jmp64 jmp64.o

.PHONY: test64

clean:
	rm -f test64 jmp64 *.o
