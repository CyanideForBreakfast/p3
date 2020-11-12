all:
	gcc -o fastertraceroute fastertraceroute.c -lpthread
	gcc -o findLongestCommonPath findLongestCommonPath.c