//proof of concept
//tail -fn0 /var/log/nginx/access.log | \
//while read line; do
//echo $line | sed -e 's/T[0-9][0-9]:[0-9][0-9]:[0-9][0-9][+][0-9][0-9]:[0-9][0-9]//' -e 's/[+][0-9][0-9]:[0-9][0-9]//' -e 's/./ /62' | \
//POST 'http://default:@10.10.10.66:8123/?query=INSERT INTO nginx.nginx_streamer FORMAT JSONEachRow'
//done
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#define LPATH "/var/log/nginx/tv.log"
#define BUFSIZE 32768
#define BUFSIZE2 65536
#define OK 1
#define NO 0


#define VER "-31.05"

FILE* reopen(FILE* f, char lpath[])
{
	if (f) fclose(f);
	while ((f=fopen(lpath,"r"))==NULL) {
		printf("can't open %s\n",lpath);perror("fopen()");sleep(10);
	}
	printf("processing %s\n",lpath);
	return f;
}
int reconnect(int sock, char* ip, int port) 
{
	struct sockaddr_in addr;
	struct timeval timeout;

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &addr.sin_addr);

	if (sock) {close(sock); sleep(5);}
	if ((sock=socket(AF_INET,SOCK_STREAM,0))<0) {perror("socket()");exit(-1);}
	if (setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(void*)&timeout,sizeof(timeout)) < 0) {
			perror("setsockopt-SO_SNDTIMEO()\n");
			exit(-1);
	}
	
	while(1) {
		printf("connecting to %s:%d ...",ip,port); fflush(stdout);
		if (connect(sock,(struct sockaddr*)&addr,sizeof(addr))==0) 
			break; 
		perror(" failed");
		printf("waiting for 20s... "); fflush(stdout);
		sleep(20);
	}
	puts("(re)connected\n"); fflush(stdout);
	return sock;
}

int post(char *header, int lenght, char *ip, int port)
{
	if (0 >= snprintf(header, BUFSIZE,
"POST /?query=INSERT%%20INTO%%20nginx.nginx_streamer%%20FORMAT%%20JSONEachRow HTTP/1.1\n"
"TE: deflate,gzip;q=0.3\n"
"Connection: TE, close\n"
"Host: %s:%d\n"
"User-Agent: lwp-request/6.03 libwww-perl/6.04\n"
"Content-Length: %d\n"
"Content-Type: application/x-www-form-urlencoded\n\n"
	, ip,port,lenght)) return NO; else return strnlen(header,BUFSIZE); 
}

int replace(char *src, char *dst)
{
	int j,i,q=0;
	if (!src || !dst) return NO;
	if (src[0]!='{') return NO;
	//memcpy(dst,src,BUFSIZE);
	//{ "requestDate": "2017-07-18T02:12:54+03:00", "requestDateTime": "2017-07-18T02:12:54+03:00"
	//{ "requestDate": "2017-07-18", "requestDateTime": "2017-07-18 02:12:54"
	for (i=j=0;i<(BUFSIZE-1)&&src[i];i++) {
		if (src[i]=='"') q++;
		if (i==76) {dst[j++]=' '; continue;}
		if ((i>=28 && i<=42)||(i>=85 && i<=90)) continue;
		dst[j++]=src[i];
	}
	if(q!=36) return NO; //quotes check
	if(j<BUFSIZE) if(j>80) if(dst[j-2]!='}') return NO; else {dst[j]='\n';dst[j+1]=0; return j+1;}
	return NO;
}


//char	buf[BUFSIZE], data[BUFSIZE], header[BUFSIZE];
char *buf, *data, *header, *packet;

int main(int argc, char **argv)
{
	//char	buf[BUFSIZE], data[BUFSIZE]={0}, header[BUFSIZE]={0};
	int	sock=0,
		n,nn,e=0,
		port=8123;			/* default port */
	char	ip[256]="127.0.0.1",		/* default ip */
		lpath[1024]=LPATH;
	FILE*	f=0;
	struct stat st;

	if (argc==1) {
		puts(VER);
		puts("usage\n j2ch remoteIP remotePort\n");
		exit(0);
	}

	buf=calloc(BUFSIZE,1);
	data=calloc(BUFSIZE,1);
	header=calloc(BUFSIZE,1);
	packet=calloc(BUFSIZE2,1);

	if (argc>=3) {	
		strncpy(ip,argv[1],256); port=atoi(argv[2]);
		if (argc==4) strncpy(lpath,argv[3],sizeof(lpath));
	}

	f= reopen(f,lpath);
	sock= reconnect(sock,ip,port);

	for (;;) {
		printf("."); fflush(stdout);
		if (fgets(buf,BUFSIZE-1,f)<=0) {
			printf("_"); fflush(stdout);
			if (stat(lpath,&st)==-1) {
				printf("\n%s ",lpath);
				perror("stat()");
				fflush(stdout);fflush(stderr);
				sleep(5);
			} else
			if (st.st_size < ftell(f)) {
				printf("\n%s externally reopens/trunkated\n",lpath);
				f= reopen(f,lpath);
				fflush(stdout);fflush(stderr);
				continue;
			}
			printf("?");	//eof? 
			fflush(stdout);
			sleep(1);
			continue;
		}

			printf("\\");fflush(stdout);
		n=replace(buf,data);	// ->
		nn=post(header,n,ip,port);
		if (!n*nn) {sleep(1);continue;}

		memcpy( packet, header, nn);
		memcpy( packet+nn, data, n);
			printf("/");fflush(stdout);

			printf(">");fflush(stdout);
again:
		if (send(sock,packet,n+nn,MSG_NOSIGNAL)==n+nn) { 
				printf("p");fflush(stdout);
		} else {
         	perror("send()"); fflush(stdout); fflush(stderr);
         	sock= reconnect(sock,ip,port);
				sleep (3);
				puts("re-send");fflush(stdout);
				goto again;
     	}
	}

   if (f) fclose(f);
   if (sock) close(sock);
		puts("bye!"); fflush(NULL);
   return 0;
}
