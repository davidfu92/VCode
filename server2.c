//
//  server.c
//
//
//  Created by Jan Anthony Miranda on 4/7/15.
//
//	tftp server that binds to localhost:69
//	parses and outputs request
//	sends error packet back to client
//

//#include "unp.h"
#include 	<stdio.h>
#include    <unistd.h>
#include	<sys/socket.h>	/* basic socket definitions */
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<string.h>

#define MAXDATASIZE 1024 /* Maximum data size allowed */

//Function Prototypes
void sendFile(int sockfd, char *file);

int
main(int argc, char **argv)
{
    if(argc!=2){
        printf("Please specify port number\n");
        exit(0);
    }
    int	sockfd, newfd;
    struct sockaddr_in	servaddr, cliaddr;
    
    char		mesg[6144]; //request buffer
    char* buffindex; //buffer index
    char op[10]; //operation
    char address[12]; //client ip address
    int port; //client port num
    char filename[196]; //filename
    char mode[12]; //mode
    char sendmesg[100]; //error message
    int read =0;
	char username[100];
	FILE *fp;
    
    //set up socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1])); //port 69
    
    //bind
    if(-1==bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))){
        perror("Bind");
        exit(0);
    }
    //loop
    while(1) {
        int len = sizeof(cliaddr);
        // receive request
        int n = recvfrom(sockfd, mesg, sizeof(mesg), 0, (struct sockaddr *) &cliaddr, &len);
        if(n==-1){
            perror("recv");
            exit(0);
        }
		//create new socket and connect it to client
        newfd = socket(AF_INET, SOCK_DGRAM,0);
        if((connect(newfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)))==-1){
            perror("connect");
            exit(0);
        }
		
        //parse stuff
        buffindex=mesg;

        //ip and port
        sprintf(address, "%s\0", inet_ntoa(cliaddr.sin_addr));
        port = ntohs(cliaddr.sin_port);
        // op
        int opcode = *buffindex++;
        switch(opcode){
            case 1:
                sprintf(op, "%s\0", "RRQ"); //read request
                read=1;
                break;
            case 2:
                sprintf(op, "%s\0", "WRQ"); //write request
                break;
            default:
                sprintf(op, "%s\0", "Invalid OP code");
                continue;
        }
		buffindex++;
        //user
        strncpy(username, buffindex, sizeof (username)-1);
        buffindex+=strlen(username)+1;
        //file
        strncpy(filename, buffindex, sizeof(filename) -1);
        buffindex+=strlen(filename)+1;

        //output result
        printf("%s %s %s, %s:%d\n", op, username, filename, address, port);
        char filebuf[6144];
        //check if file exists
        if(access( filename, F_OK ) != -1 && strncasecmp (mode, "octet", 5)==0) {
            printf("File Exists\n");
            // file exist
			if(read==1){
            	sendFile(newfd, filename);
				close(newfd);
			}else{
				fp=fopen(filename,"w");
				memcpy((char*) filebuf, buffindex, n-(3+strlen(filename)));
    			if((fwrite(filebuf, 1, n-(3+strlen(filename)), fp))!=n-(3+strlen(filename))){
        			printf("Error writing to file\n");
        			return;
    			}
				fclose(fp);
				close(newfd);
			}
        } else {
            // file doesn't exist
            //send error
            int slen = sprintf(sendmesg, "%c%c%c%cCan't find file%c",0x00,0x05,0x00,0x01,0x00);
            if(-1==send(newfd, sendmesg, slen, 0)){
                perror("Sendto");
                exit(0);
            }
			close(newfd);
        }
    }
}
//send file to client
void sendFile(int sockfd, char *file){
    FILE *fp;
    char filename[128];
    char sendmesg[128];
    unsigned char filebuf[MAXDATASIZE + 1];
    unsigned char packetbuf[MAXDATASIZE + 12];
    int ssize=0, len;
    strcpy(filename, file); //copy file name to filename
    fp=fopen(filename, "r");
    if(fp==NULL){
        
        int slen = sprintf(sendmesg, "%c%c%c%cCan't open file%c",0x00,0x05,0x00,0x01,0x00);
        if(-1==send(sockfd, sendmesg, slen, 0)){
            perror("Sendto");
            exit(0);
        }
        return;
    }
    memset (filebuf, 0, sizeof (filebuf));
    ssize=fread(filebuf,1,MAXDATASIZE,fp);
    printf("sending file\n");
    
    sprintf ((char *) packetbuf, "%c%c%s%c", 0x02, 0x00,filename, 0x00);	/* build data packet but write out the count as zero */
    memcpy ((char *) packetbuf + 4, filebuf, ssize);
    len = 4 + ssize;
    packetbuf[2] = (1 & 0xFF00) >> 8;	//fill in the count (top number first)
    packetbuf[3] = (1 & 0x00FF);	//fill in the lower part of the count
    if (send (sockfd, packetbuf, len, 0) != len)	/* send the data packet */
    {
        perror("sendto");
        return;
    }
    printf("Sent File\n");
}
