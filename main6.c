
#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>
#include	<errno.h>	
#include    <sys/socket.h>
#include    <arpa/inet.h> /*inet_aton()*/
#include    <string.h>  /* memset() */
#include    <unistd.h>  /* read() write() select()*/
#include    <sys/types.h>/*recv()*/
#include    <termios.h>
#include    <sys/ioctl.h>/*ioctl()*/
#include    <net/if.h>   /*ifreq*/
#include    <time.h>

/*echo*/
#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL)

/* define macros*/
#define MAXBUF	1024
#define STDIN_FILENO	0
#define STDOUT_FILENO	1

/* define FTP reply code */
#define USERNAME	220
#define PASSWORD	331
#define LOGIN		230
#define PATHNAME	257
#define CLOSEDATA	226
#define ACTIONOK	250

#define PASV    1
#define PORT    0

#define BINARY  0
#define ASCII   1

/* DefinE global variables */
//char	*host;              /* hostname or dotted-decimal string */
//char	*port;
char	*rbuf, *rbuf1;		/* pointer that is malloc'ed */
char	*wbuf, *wbuf1;		/* pointer that is malloc'ed */
struct	sockaddr_in	servaddr;

int     cliopen(char *host, char *port);            
void	strtosrv(char *str, char *host, char *port);
void	srvtostr(char *str, int sockfd,char* port);
int     set_disp_mode(int fd,int option);
void	cmd_tcp(int sockfd);                        
void	ftp_list(int sockfd);                       
int		ftp_get(int sck, char *pDownloadFileName_s,int type);    
int		ftp_put (int sck, char *pUploadFileName_s,int type);     
void 	getFileName(char*,char* name_with_n);
void 	modifyFileName(char*);

int
main(int argc, char *argv[])
{
	int	fd;
     
	 if (2 != argc)
	{
		printf("%s\n","missing <hostname>");
		exit(0);
	}
 
    char *host = argv[1];
    char  *port = "21";
    
    
	/*****************************************************************
	Allocate the read and write buffers before open().
	*****************************************************************/
        rbuf = (char*)malloc(MAXBUF * sizeof(char));
        memset(rbuf, 0, MAXBUF * sizeof(char));
        rbuf1 = (char*)malloc(MAXBUF * sizeof(char));
        memset(rbuf1, 0, MAXBUF * sizeof(char));
        wbuf = (char*)malloc(MAXBUF * sizeof(char));
        memset(wbuf, 0, MAXBUF * sizeof(char));
        wbuf1 = (char*)malloc(MAXBUF * sizeof(char));
        memset(wbuf1, 0, MAXBUF * sizeof(char));
    //--------------------------------------------------------------//
    
    if ((fd = cliopen(host, port)) == -1) 
	{
		free(rbuf);
		free(rbuf1);
		free(wbuf);
		free(wbuf1);
        exit(1);
	}		
	cmd_tcp(fd);                

	exit(0);
}


/* Establish a TCP connection from client to server */
int
cliopen(char *host, char *port)
{
	/*************************************************************
	//Convert the host to ip address if it was an domain name
	*************************************************************/
    struct hostent* hostend;    
    struct in_addr* servip;     
    unsigned short   servport;   
    int             sockfd;   
    
    
    if(inet_aton(host, servip) == 0){
        hostend = gethostbyname(host);    //using getaddrinfo later
        servip  = (struct in_addr*)(*(hostend->h_addr_list));
        host = inet_ntoa(*servip);
    }
    
    printf("Connecting to %s ...\n", host);
    
    servport=atoi(port);
    
    
    memset(&servaddr, 0, sizeof(servaddr));         
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {    
        printf("Socket construction failed\n");
        return -1;
    }
    servaddr.sin_family     = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr(host);
    servaddr.sin_port       = htons(servport);
    
    
    if(connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) == -1)
    {
        printf("connecting address failed\n");
        return -1;
    }
    printf("TCP connection success\n");
    return sockfd;
}



void
strtosrv(char *str, char *host, char *port)
{
	/*************************************************************
	//3. code here
	*************************************************************/
    int addr[6];
	 sscanf(str, "%*[^(](%d,%d,%d,%d,%d,%d)",&addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
	 sprintf(host, "%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
	 sprintf(port, "%d",(addr[4]<<8)+addr[5]);
	 printf("ok\n");
}


int
active_connect(char *str, int sockfd)
{
    unsigned short portNum;
    struct  sockaddr_in servAddr;
    int     newFd;
    int num[6];

    srand((unsigned int)time(NULL));
    portNum=(rand()%(65535-1024+1))+1024;
    
    memset(&servAddr,0,sizeof(servAddr));
    if((newFd=socket(PF_INET,SOCK_STREAM,0))<0){
        printf("socket construction failed\n");
        return -1;
    }
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servAddr.sin_port   = htons(portNum);
    if(bind(newFd,(struct sockaddr*)&servAddr,sizeof(servAddr))<0){
        printf("bind() failed\n");
        return -1;
    }
    if(listen(newFd,1)<0){
        printf("listen() failed\n");
        return -1;
    }
    
    
    struct ifreq ifr;
    strcpy(ifr.ifr_name,"en0");
    if(ioctl(sockfd,SIOCGIFADDR,&ifr)<0)
        perror("ioctl");
    sscanf(inet_ntoa( ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr ),"%d.%d.%d.%d",&num[0],&num[1],&num[2],&num[3]);
    num[5]=portNum & 0xff;
    num[4]=portNum >> 8;
    sprintf(str,"PORT %d,%d,%d,%d,%d,%d\r\n",num[0],num[1],num[2],num[3],num[4],num[5]);
    printf("%s",str);
    return newFd;
}

int set_disp_mode(int fd,int option)
{
    int err;
    struct termios term;
    if(tcgetattr(fd,&term)==-1){
        perror("Cannot get the attribution of the terminal");
        return 1;
    }
    if(option)
        term.c_lflag|=ECHOFLAGS;
    else
        term.c_lflag &=~ECHOFLAGS;
    err=tcsetattr(fd,TCSAFLUSH,&term);
    if(err==-1 && err==EINTR){
        perror("Cannot set the attribution of the terminal");
        return 1;
    }
    return 0;
}



void
cmd_tcp(int sockfd)
{
	int    maxfdp1, nread, nwrite, fd,\
            replycode, mode=PASV, type=BINARY;
	char   host[16];
	char   port[6];
    fd_set  rset;       
    char filename[100];

	FD_ZERO(&rset);         
	maxfdp1 = sockfd + 1;
    
	for ( ; ; )
    {
		FD_SET(STDIN_FILENO, &rset);    
		FD_SET(sockfd, &rset);          

		if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0)
			printf("select error\n");
			
		
		if (FD_ISSET(STDIN_FILENO, &rset)) {

            memset(rbuf, 0, MAXBUF * sizeof(char));   
            nread=0;
            memset(wbuf, 0, MAXBUF * sizeof(char));   
            nwrite=0; 
            memset(rbuf1, 0, MAXBUF * sizeof(char));   
            memset(wbuf1, 0, MAXBUF * sizeof(char));   
			if ((nread = read(STDIN_FILENO, rbuf, MAXBUF * sizeof(char))) < 0) 
                printf("read error from stdin\n");
            
            nwrite = nread + 6;
			
				
			if (replycode == USERNAME)
            {
                strcpy(&rbuf[strlen(rbuf)-1],  "\r\n");
				sprintf(wbuf, "USER %s", rbuf);
    
				if (write(sockfd, wbuf, nwrite) != nwrite)
					printf("write error\n");
			}
                        

			 /*************************************************************
			 //send password
			 *************************************************************/
            if (replycode == PASSWORD)
            {
                strcpy(&rbuf[strlen(rbuf)-1],  "\r\n");
                sprintf(wbuf, "PASS %s", rbuf);
                if (write(sockfd, wbuf, nwrite) != nwrite)
				    printf("write error\n");
                
                set_disp_mode(STDIN_FILENO,1);
			}
            
				 
    		if (replycode==LOGIN || replycode==CLOSEDATA || replycode==PATHNAME || replycode==ACTIONOK)
    		{
    			
    			
    			if (strncmp(rbuf, "ls", 2) == 0) 
                {
                    if(mode==PASV)
                        sprintf(wbuf, "%s", "PASV\r\n");
                    else
                        fd = active_connect(wbuf, sockfd);
                      
                    write(sockfd, wbuf, strlen(wbuf));
                        
                    sprintf(wbuf1, "%s", "LIST -al\r\n");
                    nwrite = strlen(wbuf1);
                    continue;
                }
				
				/*************************************************************
				// 5. code here: cd - change working directory/
				*************************************************************/
                
                if (strncmp(rbuf, "cd" , 2) == 0)
                {
                    sprintf(wbuf,"%s","CWD");
                    strcat(wbuf,rbuf+2);
                    nwrite=nread+1;
                    if (write(sockfd, wbuf, nwrite) != nwrite)
                        printf("write error\n");
                    continue;
                }
                
				
				if (strncmp(rbuf, "pwd", 3) == 0) {
					sprintf(wbuf, "%s", "PWD\n");
					write(sockfd, wbuf, 4);
					continue;
				}

				/*************************************************************
				// 6. code here: quit - quit from ftp server
				*************************************************************/
                if(strncmp(rbuf,"quit",4)==0){
                    sprintf(wbuf, "%s", "QUIT\n");
                    write(sockfd, wbuf, 5);
                    continue;
                }


				/*************************************************************
				// 7. code here: get - get file from ftp server
				*************************************************************/
                if (strncmp(rbuf, "get",3)==0) {
                    getFileName(filename,rbuf+4);
                    if(mode==PASV)
                        sprintf(wbuf, "%s", "PASV\r\n");
                    else
                        fd = active_connect(wbuf, sockfd);
                    
                    write(sockfd, wbuf, strlen(wbuf));
                    
                    sprintf(wbuf1,"%s","RETR");
                    strcat(wbuf1,rbuf+3);
                    nwrite=nread+3;
                    continue;
                }

				/*************************************************************
				// 8. code here: put -  put file upto ftp server
				*************************************************************/
                if (strncmp(rbuf, "put",3)==0) {
                    getFileName(filename,rbuf+4);
                    FILE* f=fopen(filename,"r");
                    if (!f) {
                        fclose(f);
                        printf("file not exist\n");
                        continue;
                    }
                    fclose(f);
                    if(mode==PASV)
                        sprintf(wbuf, "%s", "PASV\r\n");
                    else
                        fd = active_connect(wbuf, sockfd);
                    
                    write(sockfd, wbuf, strlen(wbuf));
                    
                    sprintf(wbuf1,"%s","STOR");
                    strcat(wbuf1,rbuf+3);
                    nwrite=nread+1;
                    continue;
                }
                
    
                if (strncmp(rbuf, "passive",7)==0) {
                    mode=PASV;
                    printf("now is passive mode\n");
                    continue;
                }
                
                
                if (strncmp(rbuf, "port",4)==0) {
                    mode=PORT;
                    printf("now is port mode\n");
                    continue;
                }
                
    
                if (strncmp(rbuf,"mode",4)==0) {
                    printf("now is mode ");
                    if(mode==PASV)
                        printf("passive\n");
                    if(mode==PORT)
                        printf("port\n");
                    continue;
                }
                
                
                if(strncmp(rbuf,"ascii",5)==0){
                    type=ASCII;
                    sprintf(wbuf, "%s", "TYPE A\r\n");
                    write(sockfd, wbuf, strlen(wbuf));
                    continue;
                }
                
    
                if(strncmp(rbuf, "binary", 5)==0){
                    type=BINARY;
                    sprintf(wbuf, "%s", "TYPE I\r\n");
                    write(sockfd, wbuf, strlen(wbuf));
                    continue;
                }
                
				write(sockfd, rbuf, nread);
                
			}
            
		}

		
		if (FD_ISSET(sockfd, &rset)) {
            
            if ( (nread = recv(sockfd, rbuf, MAXBUF*sizeof(char), 0)) < 0)
				printf("recv error\n");
			else if (nread == 0)
				break;


			if (strncmp(rbuf, "220", 3)==0 || strncmp(rbuf, "530", 3)==0||strncmp(rbuf, "503", 3)==0){
				strcat(rbuf,  "your name: ");
				nread += 12;
				replycode = USERNAME;
			}

			/*************************************************************
			// 9. code here: handle other response coming from server
			*************************************************************/
            
            
          else if (strncmp(rbuf, "331", 3)==0){
                strcat(rbuf,  "your password: \n");
                nread += 16;
                replycode = PASSWORD;
                set_disp_mode(STDIN_FILENO,0);
          }
            
           else	if (strncmp(rbuf, "230", 3)==0){
                replycode = LOGIN;
           }
            
			
            
           else if (strncmp(rbuf, "227", 3)== 0||strncmp(rbuf,"200",3)==0) {
               if (strncmp(rbuf, "227", 3)== 0) {
                   memset(host, 0, 16);
                   memset(port, 0, 6);
                   strtosrv(rbuf, host, port);
                   fd = cliopen(host, port);
                   if(write(sockfd, wbuf1, nwrite)!=nwrite)
                       printf("write error\n");
                   nwrite = 0;
               }
               
               else if(strncmp(wbuf,"PORT",4)==0){
                   write(sockfd, wbuf1, strlen(wbuf1));
                   nwrite = 0;
               }
               
			}
            
           else if((strncmp(rbuf,"500",3)==0)){
               printf("unknown command\n");
               continue;
           }
			
			else if((strncmp(rbuf, "150", 3)) == 0){
                if(mode==PORT){
                    fd=accept(fd,0,0);
                }
                if (write(STDOUT_FILENO, rbuf, nread) != nread)
                    printf("write error to stdout\n");
                
                if(strncmp(wbuf1,"RETR",4)==0)
                    ftp_get(fd,filename,type);
                
                else if(strncmp(wbuf1,"STOR",4)==0)
                    ftp_put(fd,filename,type);
                
                else if(strncmp(wbuf1, "LIST",4)==0)
                    ftp_list(fd);
                continue;
           }
            else if((strncmp(rbuf,"500",3)==0)){
                printf("unknown command\n");
                continue;
            }
            else if((strncmp(rbuf,"221",3)==0)){
                printf("\n******************bye bye***********************\n\n");
                break;
            }
			if (write(STDOUT_FILENO, rbuf, nread) != nread)
				printf("write error to stdout\n");
		}
        
	}

	
	if (close(sockfd) < 0)
		printf("close error\n");
}



void
ftp_list(int sockfd)
{
	int	nread;

	for ( ; ; )
	{
		
		if ( (nread = recv(sockfd, rbuf1,MAXBUF*sizeof(char), 0)) < 0)
			printf("recv error\n");
		else if (nread == 0)
			break;

		if (write(STDOUT_FILENO, rbuf1, nread) != nread)
			printf("send error to stdout\n");
	}

	if (close(sockfd) < 0)
		printf("close error\n");
}


int
ftp_get(int sck, char *pDownloadFileName_s,int type)
{
    int		nread;
    int     fileLen;
    FILE*   f ;
    char    *tmpbuf;
    int     index,dex;
    
    tmpbuf=(char*)malloc(MAXBUF*sizeof(char));
    
    modifyFileName(pDownloadFileName_s);
    
    if(type==BINARY){
        f = fopen(pDownloadFileName_s,"rb");
        if(!f)
            f = fopen(pDownloadFileName_s,"wb");
       }
    else{
        f = fopen(pDownloadFileName_s,"r+");
        if(!f)
            f = fopen(pDownloadFileName_s,"w");
        }

    
    
    for ( ; ; )
    {
        memset(rbuf1, 0, MAXBUF*sizeof(char));
        
        if ( (nread = recv(sck, rbuf1,MAXBUF*sizeof(char), 0)) < 0)
            printf("recv error\n");
        else if (nread == 0)
            break;
        
        if (type == ASCII)
        {
           memset(tmpbuf, 0, MAXBUF*sizeof(char));
           dex = 0;
           for (index = 0; index < nread; index ++)
           {
               if (rbuf1[index] == '\r')
               {
                   continue;
               }
               else
               {
                   tmpbuf[dex] = rbuf1[index];
                   dex++;
               }
           }

           if(fwrite(tmpbuf, 1, dex, f) != dex)
            printf("write error\n");

        }
        else
        {
            if(fwrite(rbuf1, 1, nread, f) != nread)
                printf("write error\n");
        }
        
    }
        free(tmpbuf);
        tmpbuf=NULL;
		fclose(f);
    
    if (close(sck) < 0){
        printf("close error\n");
        return -1;
    }
    return 0;
}


int
ftp_put (int sck, char *pUploadFileName_s,int type)
{
    /*************************************************************
     // 11. code here
     *************************************************************/
    int fileLen;
    FILE*  f;//=fopen(pUploadFileName_s,"r");
    char *buffer;
    int size;
    int byte;
    size=1024;
    char *tmpbuf;
    int index,dex;
    buffer=(char*)malloc(size*sizeof(char));
    tmpbuf=(char*)malloc(size*sizeof(char));
      if(type==BINARY)
         f=fopen(pUploadFileName_s,"rb");
     else
        f=fopen(pUploadFileName_s,"r");
    
    if(!f){
        printf("can't find this file\n");
        return -1;
    }
    
    fseek(f,0,SEEK_SET);
    fseek(f,0,SEEK_END);
    
    
    fseek(f,0,SEEK_SET);
    memset(buffer, 0, size);
    
    
    for( ; ; )
    {
        byte = (buffer, 1, size, f);
        if (!byte){
            printf("upload file %s successfully\n", pUploadFileName_s);
            break;
        }
        if (type == ASCII)
        {
          memset(tmpbuf, 0, size);
           dex = 0;
           for (index = 0; index < byte; index ++)
           {
               if (buffer[index] == '\n')
               {
                  tmpbuf[dex]='\r';
                  dex++;
                  tmpbuf[dex]='\n';
                  dex++;
               }
               else
               {
                   tmpbuf[dex] = buffer[index];
                   dex++;
               }
            }
            if ( send(sck, tmpbuf, dex, 0) != dex)
             {
                printf("send error\n");
            
                if ( send(sck, tmpbuf, dex, 0) != dex )
                {
                    printf("Upload file %s failed\n", pUploadFileName_s);
                    break;
                }
             } 
          }            
        else
        {
            if ( send(sck, buffer, byte, 0) != byte)
            {
                printf("send error\n");
            
                if ( send(sck, buffer, byte, 0) != byte )
                {
                    printf("Upload file %s failed\n", pUploadFileName_s);
                    break;
                }
             }
        }
        if (byte < size)
        {
            printf("\nupload file %s successfully\n", pUploadFileName_s);
            break;
        }
    }
    
    free(buffer);
    buffer=NULL;
    free(tmpbuf);
    tmpbuf=NULL;
    
    fclose(f);
    if (close(sck) < 0)
        printf("close error\n");
    
    return 0;
}
void getFileName(char *name,char* name_with_n){
    int length=(int)strlen(name_with_n);
    strcpy(name, name_with_n);
    name[length-1]=0;
}
void modifyFileName(char* orgname){
    char  name[100];
    strcpy(name,orgname);
    FILE* f;
    while((f=fopen(name,"rb"))!=NULL){
        strcpy(orgname, name);
        strcpy(name, "cpy");
        strcat(name,orgname);
        fclose(f);
    }
    fclose(f);
    strcpy(orgname, name);
}