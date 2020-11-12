all:
	gcc -o fastertraceroute fastertraceroute.c -lpthread
	gcc findLongestCommonPath.c