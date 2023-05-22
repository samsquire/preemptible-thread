
main: preemptible-thread-evented.c 
	gcc -g -march=native -pthread preemptible-thread-evented.c -lm -o main ; \
	rm /tmp/core* || true

main2: preemptible-thread-evented2.c 
	gcc -g -march=native -pthread preemptible-thread-evented2.c -lm -o main2 
