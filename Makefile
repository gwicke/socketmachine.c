all:	 
	gcc -Wall -lev -O2 -fomit-frame-pointer -o socketmachine evaio.c socketmachine.c
debug:	
	gcc -Wall -lev -O0 -DDEBUG -gdwarf-2 -g3 -o socketmachine evaio.c socketmachine.c && gdb socketmachine
perf:	
	gcc -Wall -lev -Os -fomit-frame-pointer -o socketmachine evaio.c socketmachine.c
perfm:	 
	gcc -Wall -lev -Os -fomit-frame-pointer -mtune=pentium-m -o socketmachine evaio.c socketmachine.c
perf3:	
	gcc -Wall -lev -O3 -fomit-frame-pointer -finline-limit=36 -o socketmachine evaio.c socketmachine.c


list.run: list
	./list
list: list.c
	#gcc  -O3 -fomit-frame-pointer -finline-limit=36  -o list list.c
	gcc  -O3 -fomit-frame-pointer -finline-limit=36 -fmodulo-sched  -o list list.c

list_template: list_template.c
	#gcc  -O3 -fomit-frame-pointer -finline-limit=36  -o list list.c
	gcc  -O3 -fomit-frame-pointer -finline-limit=36 -fmodulo-sched  -o list list_template.c
