all: haha

haha:
	gcc httpproxy.c -o proxy -lpthread
clean:
	rm proxy

