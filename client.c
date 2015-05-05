
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024

void requestFile(int sock, char* name, struct sockaddr_in server, struct sockaddr_in server2, FILE* fp);
void setupWin(FILE* fp);

int datasize = 512;
/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct sockaddr_in serveraddr2;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
	FILE *fp;
    
    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    
    /* build the server's Internet address */
    bzero(&serveraddr2, sizeof(serveraddr2));
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    
    /* get a message from the user */
    bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);
    
    requestFile(sockfd, buf, serveraddr, serveraddr2, fp);
    setupWin(fp);
    
    return 0;
}

void setupWin(FILE* fp){
	
}

void requestFile(int sock, char* name, struct sockaddr_in server, struct sockaddr_in server2, FILE * fp){
    char buf[100], rec[BUFSIZE], *bufindex;
    int n,servlen;
    char filename[128], recfilename[128], filebuf[BUFSIZE];
    
    strcpy(filename, name);
    
    int len = sprintf(buf, "%c%c%s%c", 0x01, 0x00, filename, 0x00);
    if(len==0){
        error("Error in creating request packet\n");
    }
    //send request
    if(sendto(sock, buf, len, 0, (struct sockaddr*) &server, sizeof(server)) !=len)
        error("sendto error\n");
    //receive ack
    n=recvfrom(sock, rec, sizeof(rec), 0, (struct sockaddr*) &server2, &servlen);
    if(n==-1)
        error("Error receiving ACK from server\n");
    //check if ACK
    bufindex=rec;
    if(bufindex[0] != 0x05){
        error("Received wrong packet from server\n");
    }
    //receive file
    fp = fopen(filename, "w");
    if(fp==NULL){
        error("Can't open/create file\n");
    }
    memset(filebuf, 0, sizeof(filebuf));
    n=recvfrom (sock, rec, sizeof(rec), 0, (struct sockaddr*) &server2, servlen);
    if(n==-1)
        error("Error receiving file from server\n");
    bufindex=rec;
    if(bufindex++[0] != 0x02){
        error("Received wrong packet from server\n");
    }
    bufindex++; //skip secondary code
    //filename
    bzero(recfilename, sizeof(recfilename));// set to zero
    strncpy(recfilename, bufindex, sizeof (recfilename)-1);
    if(strcmp(filename,recfilename)!=0){
        printf("Received wrong file from server\n");
        return;
    }
    bufindex+=strlen(recfilename)+1;
    memcpy((char*) filebuf, bufindex, n-(3+strlen(recfilename)));
    if((fwrite(filebuf, 1, n-(3+strlen(recfilename)), fp))!=n-(3+strlen(recfilename))){
        printf("Error writing to file\n");
        return;
    }
//send ACK
	len = sprintf(buf, "%c%c", 0x05, 0x00);
    if(len==0){
        error("Error in creating ACK packet\n");
    }
	if(sendto(sock, buf, len, 0, (struct sockaddr*) &server2, servlen) !=len)
        error("sendto error\n");
}
