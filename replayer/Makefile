all: 
	gcc -o replayer replay.c -pthread
	gcc -o preallocate preallocate.c -pthread

replayer: replayer.c
	gcc -o replayer replay.c -pthread

preallocate: preallocate
	gcc -o preallocate preallocate.c -pthread
