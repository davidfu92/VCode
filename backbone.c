#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>

#define QUEUE 10     //Queue Size
#define BUFSIZE 1024 //Default Buffer Size
#define PACKSIZE 511 //MAX Packet Size



//Prints Out Any Error To Terminal
void print_error(char * msg) {
	perror(msg);
	exit(1);

}


// Get Socket Address, IPv4 Only
char * get_IPv4(struct sockaddr_in hostaddr)
{
	return inet_ntoa(hostaddr.sin_addr);

}

int main(int argc, char**argv)
{

	int sockfd, tftp_fd;  // listen on sock_fd, and transfer file on tftp_fd
	int portno; //Port to Listen on Default 69 
	struct sockaddr_in serveraddr; // server's addr
	struct sockaddr_in clientaddr; // client addr
	struct hostent *hostp; // client host info
	char recv[BUFSIZE]; // message recv buf
	char *hostaddrp; // dotted decimal host addr string
	int optval; // flag value for setsockopt
	int msg_size; // message byte size
	int clientlen;
	char reply[BUFSIZE]; //msg reply buffer

	if (argc != 2) {
		fprintf(stdout, "WARNING:  DEFAULT Set to \n\n");
		portno = 69;
	} else {
		portno = atoi(argv[1]);
	}


	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if( sockfd < 0 )
		print_error("Error Opening Socket");

	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/* 
	 *    * bind: associate the parent socket with a port 
	 *       */
	if (bind(sockfd, (struct sockaddr *) &serveraddr, 
				sizeof(serveraddr)) < 0) 
		error("ERROR on binding");


	clientlen = sizeof(clientaddr);

	while (1) {

		/*
		 *      * recvfrom: receive a UDP datagram from a client
		 *           */
		bzero(reply, BUFSIZE);
		bzero(reply, BUFSIZE);
		msg_size = recvfrom(sockfd, reply, BUFSIZE, MSG_WAITALL, (struct sockaddr *) &clientaddr, &clientlen);
		//parsePackage(buf, n);
		if (msg_size < 0)
			error("ERROR in getting package");

		/* 
		 *      * gethostbyaddr: determine who sent the datagram
		 *           */
		hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		if (hostp == NULL)
			error("ERROR on Getting Host Name");
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
			error("ERROR on Getting Clinet IP\n");
		printf("%s:%d\n", hostaddrp, (unsigned short)&clientaddr.sin_port);

		/* 
		 *      * sendto: echo the input back to the client 
		 *           */
		//n = sendto(sockfd, defmsg, strlen(defmsg), 0, (struct sockaddr *) &clientaddr, clientlen);
		//if (n < 0) 
		//	error("ERROR in sendto");
	}

}
