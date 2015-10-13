all:	ftpclient ftpserver

tcpclient: ftpclient.c
	gcc -Wall $< -o $@

tcpserver: ftpserver.c
	gcc -Wall $< -o $@

clean:
	rm -f ftpclient ftpserver *.o *~ core

