#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>

#define BUFSIZE 1024
#define port 52015



char* left(char *string, const int length);
void malloc_error(void);
int texteditor(char *text, int maxchars, const int startrow, const int startcol,int maxrows,const int maxcols,const int displayrows,const int returnhandler,const char *permitted, bool ins, const bool allowcr, int sx, int sy);
void *setupeditor(FILE *fp);
void sendFile(char* buf);
void requestFile();
void getFile(char* rec);

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int sockfd;
struct sockaddr_in serveraddr2;
int serverlen2;
char filename[100];
char *username;
int posx,posy;
FILE *fp;

int main(int argc, char **argv)
{
    int portno, n, serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE], rec[6144];
    pthread_t tid = -1;
    
    /* check command line arguments */
    if (argc < 4) {
        fprintf(stderr,"usage: %s <username> <hostname> <port>\n", argv[0]);
        exit(0);
    }
    username = argv[1];
    hostname = argv[2];
    if(argc>3)
        portno = atoi(argv[2]);
    else
        portno = port;
    
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
    printf("Please enter filename: ");
    fgets(buf, BUFSIZE, stdin);
    buf[strlen(buf)-1]='\0';
	//copy to global variable
    strncpy(filename, buf, sizeof(filename));
    
    /* for testing
    fp = fopen(buf, "r+");
    if(!fp){
        printf("Can't open file\n");
        return 0;
    }*/

	//initial file request
    requestFile();
    
    //listen for packets
    while(true){
        n=recvfrom (sockfd, rec, sizeof(rec), 0, (struct sockaddr*) &serveraddr2, &serverlen2);
        if(n==-1)
            error("Error receiving packet from server\n");
        getFile(rec);//get the file
        //updated file, kill child
        if(tid!=-1)
            pthread_kill(tid,SIGKILL);
        //update display
        pthread_create(&tid, NULL, setupeditor, fp);
    }
    
    return 0;
}

void requestFile(){
    char packet[1024];
    
    int len = sprintf(packet, "%c%c%s%c%s%c", 0x01, 0x00, filename, 0x00, username ,0x00);
    if(len==0)
        error("Error in creating read packet\n");
    if((sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr*) &serveraddr2, serverlen2))<0)
        error("Error sending file\n");
    
}
void getFile(char* rec){
    char recfilename[100];
    int n;
    char filebuf[6144];
    char *bufindex;
    
    fp=fopen(filename, "w");
    
    bufindex=rec;
    if(bufindex++[0] != 0x02)
        error("Received wrong packet from server\n");
    
    bufindex++; //skip secondary code
    //check filename
    bzero(recfilename, sizeof(recfilename));// set to zero
    strncpy(recfilename, bufindex, sizeof (recfilename)-1);
    if(strcmp(filename,recfilename)!=0){
        printf("Received wrong file from server\n");
		exit(0);
        return;
    }
    //get file
    bufindex+=strlen(recfilename)+1;
    memcpy((char*) filebuf, bufindex, n-(3+strlen(recfilename)));
    if((fwrite(filebuf, 1, n-(3+strlen(recfilename)), fp))!=n-(3+strlen(recfilename))){
        printf("Error writing to file\n");
        return;
    }
    fclose(fp);
}

void sendFile(char* buf){
    char packet[2048];
    int len = sprintf(packet, "%c%c%s%c%s%c%s%c", 0x01, 0x00, filename, 0x00, username ,0x00, buf, 0x00);
    if(len==0)
        error("Error in creating write packet\n");
    
    if((sendto(sockfd, packet, len+1, 0, (struct sockaddr*) &serveraddr2, serverlen2))<0)
        error("Error sending file\n");
}

void *setupeditor(FILE* fp){
    char *message;
    int maxchars=40*80+1,startrow=0,startcol=0,maxrows=40,maxcols=80,displayrows=20,returnhandler=1;
    //char *message;
    bool ins=false,allowcr=true;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr,TRUE);
    
    move(20,0);
    hline(ACS_CKBOARD,80);
    mvaddstr(21,0,"Press TAB or BACK TAB to move between fields");
    mvaddstr(21,50,"Press F3 to Clear Text");
    mvaddstr(22,0,"Press CTRL-Y to Delete Current Line");
    
	fp=fopen(filename, "r");
    size_t fileSize;
    //Seek to the end of the file to determine the file size
    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    
    //Allocate enough memory (add 1 for the \0, since fread won't add it)
    message = malloc(maxchars*sizeof(char));
    
    //Read the file
    size_t size=fread(message,1,fileSize,fp);
    message[size]='\0'; // Add terminating zero.
    fclose(fp);
    //Print it again for debugging
    //printf("%s\n", contents);
    
    
    //strcpy(message,"Multi-line Text Editor Demo");
    
    texteditor(message,maxchars,startrow,startcol,maxrows,maxcols,displayrows,returnhandler,NULL,ins,allowcr,posx,posy);
    
    clear();
    addstr("Exited text editor");
    getch();
    endwin();
    printf("Message\n-------\n\"%s\"",message);
    free(message);
}

int texteditor(char *text, int maxchars, const int startrow, const int startcol,int maxrows,const int maxcols,const int displayrows,const int returnhandler,const char *permitted, bool ins, const bool allowcr, int sx, int sy)
/*
 *  PARAMETERS
 * ----------
 * char * text = text string to edit
 * const int maxchars = maximum number of characters to allow in text string, including final \0 (example: string "ABC"=maxchars 4)
 * 			NOTE: If maxchars==0 then maxchars=maxrows*maxcols+1
 * const int startrow = top row for the text entry window
 * const int startcol = top column number for the text entry window
 * int maxrows = maximum number of rows (lines) to allow in the text string.
 * 			NOTE:  If maxrows > displayrows, the text entry window will scroll vertically
 *                 If maxrows == displayrows, text entry window scrolling is disabled
 * 				   If maxrows ==0 then maxrows=maxchars-1, i.e one character per line
 * const int maxcols = width of the text entry window
 * const int displayrows = number of rows in the text entry window, if < maxrows the text entry window will scroll
 * 			NOTE:  A row does not necessarily end with \n, it may end with a space
 * 				   or simply wrap to the next row if the continuous length of the word exceeds maxcols
 * const int returnhandler = 0 ignore cr
 * 							 1 cr=\n
 * 							 2 replace cr with ' '
 * 							 3 exit function when cr is received
 * const char *permitted = NULL all regular text characters accepted
 * 						   if a string, accept only characters included in the string
 * bool ins = initial insert mode on/off (user changable by hitting the ins key while editing)
 * const bool allowcr = allow a \n as the first character in the string, i.e. a blank line at the top of the entered string
 *
 * EDITING CONTROL KEYS
 * INS = insert mode on/off
 * PgUp = goto first character in text string
 * PgDn = goto last character in text string
 * Home = goto first character on current row (line)
 * End = goto last character on current row (line)
 * ctrl-Y = delete current line
 * F3 = delete all text
 * F1 = exit function and discard all changes to the text
 * ESC (twice) = same as F1
 * tab = exit function
 * back tab (shift-tab) = exit function
 * return = exit function if returnhandler==3
 *
 * RETURN VALUE
 * ------------
 * Describes the key that was used to exit the function
 * tab=6
 * back tab (shift-tab) = 4
 * F1 or ESC (twice)= 27
 * return =0 (if returnhandler==3)
 */
{
    int ky,position=0,row,col,scrollstart=0;
    char *where, *original, *savetext,**display;
    bool exitflag=false;
    
    if (!maxchars)
        maxchars=maxrows*maxcols+1;
    
    if (!maxrows || maxrows > maxchars-1)
        maxrows=maxchars-1;
    
    if (ins)
        curs_set(2);
    else
        curs_set(1);
    
    if ((display = malloc(maxrows * sizeof(char *))) ==NULL)
        malloc_error();
    for(ky = 0; ky < maxrows; ky++)
        if ((display[ky] = malloc((maxcols+1) * sizeof(char)))==NULL)
            malloc_error();
    
    if ((original=malloc(maxchars*sizeof(char)))==NULL)
        malloc_error();
    strcpy(original,text);
    
    if ((savetext=malloc(maxchars*sizeof(char)))==NULL)
        malloc_error();
    
    
    while (!exitflag)
    {
        int counter;
        do
        {
            counter=0;
            where=text;
            
            for (row=0; row < maxrows; row++)
            {
                display[row][0]=127;
                display[row][1]='\0';
            }
            
            row=0;
            while ((strlen(where) > maxcols || strchr(where,'\n') != NULL) &&  (display[maxrows-1][0]==127 || display[maxrows-1][0]=='\n'))
            {
                strncpy(display[row],where,maxcols);
                display[row][maxcols]='\0';
                if (strchr(display[row],'\n') != NULL)
                    left(display[row],strchr(display[row],'\n')-display[row]);
                else
                    left(display[row],strrchr(display[row],' ')-display[row]);
                if (display[maxrows-1][0]==127 || display[maxrows-1][0]=='\n')
                {
                    where+=strlen(display[row]);
                    if (where[0]=='\n' || where[0]==' ' || where[0]=='\0')
                        where++;
                    row++;
                }
            }
            if (row == maxrows-1 && strlen(where) > maxcols) // undo last edit because wordwrap would make maxrows-1 longer than maxcols
            {
                strcpy(text,savetext);
                if (ky==KEY_BACKSPACE)
                    position++;
                counter=1;
            }
        }
        while (counter);
        
        strcpy(display[row],where);
        
        ky=0;
        if (strchr(display[maxrows-1],'\n') != NULL) // if we have a \n in the last line then we check what the next character is to insure that we don't write stuff into the last line+1
            if (strchr(display[maxrows-1],'\n')[1] != '\0')
                ky=KEY_BACKSPACE; // delete the last character we entered if it would push the text into the last line +1
        
        col=position;
        row=0;
        counter=0;
        while (col > strlen(display[row]))
        {
            col-=strlen(display[row]);
            counter+=strlen(display[row]);
            if (text[counter] ==' ' || text[counter]=='\n' || text[counter]=='\0')
            {
                col--;
                counter++;
            }
            row++;
        }
        if (col > maxcols-1)
        {
            row++;
            col=0;
        }
        
        if (!ky) // otherwise  ky==KEY_BACKSPACE and we're getting rid of the last character we entered to avoid pushing text into the last line +1
        {
            if (row < scrollstart)
                scrollstart--;
            if (row > scrollstart+displayrows-1)
                scrollstart++;
            for (counter=0;counter < displayrows; counter++)
            {
                mvhline(counter+startrow,startcol,' ',maxcols);
                if (display[counter+scrollstart][0] != 127)
                    mvprintw(counter+startrow,startcol,"%s",display[counter+scrollstart]);
            }
            move(row+startrow-scrollstart,col+startcol);
            
            ky=getch();
        }
        move(sy,sx);
        switch(ky)
        {
            case KEY_F(1): // function key 1
                strcpy(text,original);
            case 9: // tab
            case KEY_BTAB: // shift-tab
                exitflag=true;
                break;
            case 27: //esc
                // esc twice to get out, otherwise eat the chars that don't work
                //from home or end on the keypad
                ky=getch();
                if (ky == 27)
                {
                    strcpy(text,original);
                    exitflag=true;
                }
                else
                    if (ky=='[')
                    {
                        getch();
                        getch();
                    }
                    else
                        ungetch(ky);
                break;
            case KEY_F(3):
                memset(text,'\0',maxchars);
                position=0;
                scrollstart=0;
                break;
            case KEY_HOME:
                if (col)
                {
                    position=0;
                    for (col=0; col < row; col++)
                    {
                        position += strlen(display[col]);
                        if ((strchr(display[row],'\n') != NULL) || (strchr(display[row],' ') != NULL))
                            position++;
                    }
                }
                break;
            case KEY_END:
                if (col < strlen(display[row]))
                {
                    position=-1;
                    for (col=0; col <=row; col++)
                    {
                        position+=strlen(display[col]);
                        if ((strchr(display[row],'\n') != NULL) || (strchr(display[row],' ') != NULL))
                            position++;
                    }
                }
                break;
            case KEY_PPAGE:
                position=0;
                scrollstart=0;
                break;
            case KEY_NPAGE:
                position=strlen(text);
                for (counter=0; counter < maxrows; counter++)
                    if(display[counter][0]==127)
                        break;
                scrollstart=counter-displayrows;
                if (scrollstart < 0)
                    scrollstart=0;
                break;
            case KEY_LEFT:
                if (position)
                    position--;
                break;
            case KEY_RIGHT:
                if (position < strlen(text) && (row != maxrows-1 || col < maxcols-1))
                    position++;
                break;
            case KEY_UP:
                if (row)
                {
                    if (col > strlen(display[row-1]))
                        position=strlen(display[row-1]);
                    else
                        position=col;
                    ky=0;
                    for (col=0; col < row-1; col++)
                    {
                        position+=strlen(display[col]);
                        ky+=strlen(display[col]);
                        if ((strlen(display[col]) < maxcols) || (text[ky]==' ' && strlen(display[col])==maxcols))
                        {
                            position++;
                            ky++;
                        }
                    }
                }
                break;
            case KEY_DOWN:
                if (row < maxrows-1)
                    if (display[row+1][0] !=127)
                    {
                        if (col < strlen(display[row+1]))
                            position=col;
                        else
                            position=strlen(display[row+1]);
                        ky=0;
                        for (col=0; col <= row; col++)
                        {
                            position+=strlen(display[col]);
                            ky+=strlen(display[col]);
                            if ((strlen(display[col]) < maxcols) || (text[ky]==' ' && strlen(display[col])==maxcols))
                            {
                                position++;
                                ky++;
                            }
                        }
                        
                    }
                break;
            case KEY_IC: // insert key
                ins=!ins;
                if (ins)
                    curs_set(2);
                else
                    curs_set(1);
                break;
            case KEY_DC:  // delete key
                if (strlen(text))
                {
                    strcpy(savetext,text);
                    memmove(&text[position],&text[position+1],maxchars-position);
                }
                break;
            case KEY_BACKSPACE:
                if (strlen(text) && position)
                {
                    strcpy(savetext,text);
                    position--;
                    memmove(&text[position],&text[position+1],maxchars-position);
                }
                break;
            case 25: // ctrl-y
                if (display[1][0] != 127)
                {
                    position-=col;
                    ky=0;
                    do
                    {
                        memmove(&text[position],&text[position+1],maxchars-position);
                        ky++;
                    }
                    while (ky < strlen(display[row]));
                }
                else
                    memset(text,'\0',maxchars);
                break;
            case 10: // return
                switch (returnhandler)
            {
                case 1:
                    if (display[maxrows-1][0] == 127 || display[maxrows-1][0] == '\n')
                    {
                        memmove(&text[position+1],&text[position],maxchars-position);
                        text[position]='\n';
                        position++;
                    }
                    break;
                case 2:
                    ky=' ';
                    ungetch(ky);
                    break;
                case 3:
                    exitflag=true;
            }
                break;
            default:
                if (((permitted==NULL && ky > 31 && ky < 127) || (permitted != NULL && strchr(permitted,ky))) && strlen(text) < maxchars-1 && (row !=maxrows-1 || (strlen(display[maxrows-1]) < maxcols || (ins && (row!=maxrows-1 && col < maxcols )))))
                {
                    if (ins || text[position+1]=='\n' || text[position]== '\n')
                        memmove(&text[position+1],&text[position],maxchars-position);
                    text[position]=ky;
                    if (row != maxrows-1 || col < maxcols-1)
                        position++;
                }
        }//end switch
        if(!allowcr)
            if (text[0]=='\n')
            {
                memmove(&text[0],&text[1],maxchars-1);
                if (position)
                    position--;
            }
        //send update
        getyx(stdscr,posy,posx);
        sendFile(text);
    }
    
    free(original);
    free(savetext);
    for(scrollstart = 0; scrollstart < maxrows; scrollstart++){
        free(display[scrollstart]);
    }
    free(display);
    
    switch(ky)
    {
        case 9: // tab
        case KEY_BTAB:
            return 4;
        case KEY_F(1):
        case 27: // esc
            return 5;
    }
    return 0; // we hit the return key and returnhandler=3
}


char* left(char *string, const int length)
{
    if (strlen(string) > length)
        string[length]='\0';
    return string;
}

void malloc_error(void)
{
    endwin();
    fprintf(stderr, "malloc error:out of memory\n");
    exit(EXIT_FAILURE);
}
