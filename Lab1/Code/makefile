edit: iss.o mapper.o
	gcc -o iss bin\iss.o bin\mapper.o

iss.o: iss.c mapper.h
	gcc -c iss.c -o bin\iss.o

mapper.o: mapper.c
	gcc -c mapper.c -o bin\mapper.o

clean:
	rm edit bin\iss.o bin\mapper.o
