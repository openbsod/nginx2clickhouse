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
#define LPATH "/var/log/nginx/access.log"
#define IP "127.0.0.1"
#define PORT 8123
#define TABLE "nginx.nginx_zabbix"
#define INTERVAL 20
#define BUFSIZE 32768
#define M8	8388608
#define OK 1
#define NO 0

#define VER "-31.04"

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
	/* 
	if (setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(void*)&timeout,sizeof(timeout)) < 0) {
			perror("setsockopt-SO_SNDTIMEO()\n");
			exit(-1);
	}
	*/
	while(1) {
		printf("connecting to %s:%d ...",ip,port); fflush(stdout);
		if (connect(sock,(struct sockaddr*)&addr,sizeof(addr))==0) 
			break; 
		perror(" failed");
		printf("waiting for 10s... "); fflush(stdout);
		sleep(10);
	}
	puts("(re)connected\n"); fflush(stdout);
	return sock;
}

int post(char *header, int lenght, char *ip, int port)
{
	if (0 >= snprintf(header, BUFSIZE,
"POST /?query=INSERT%%20INTO%%20%s%%20FORMAT%%20JSONEachRow HTTP/1.1\n"
"TE: deflate,gzip;q=0.3\n"
"Connection: TE, close\n"
"Host: %s:%d\n"
"User-Agent: lwp-request/6.03 libwww-perl/6.04\n"
"Content-Length: %d\n"
"Content-Type: application/x-www-form-urlencoded\n\n"
	, TABLE,ip,port,lenght)) return NO; else return strnlen(header,BUFSIZE); 
}

int replace(char *src, char *dst)
{
	int j,i,q=0;
	if (!src || !dst) return NO;
	if (src[0]!='{') return NO;
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

int main(int argc, char **argv)
{
	int	sock=0,r,n,nn,ofs,e=0,port=PORT;
	char	ip[256]=IP,	lpath[1024]=LPATH;
	FILE*	f=0;
	struct stat st;
	time_t et,nt;
	char *row, *buf, *data, *header, *packet, *answer;
	if (argc==1) {
		printf("j2s v.%s\n",VER);
		puts("usage\n\tj2s remoteIP remotePort\n");
		exit(0);
	}
	buf=calloc(BUFSIZE,1);		// a nginx's log row
	row=calloc(BUFSIZE,1);		// reformated one
	data=calloc(M8,1);			// accumulated rows
	header=calloc(BUFSIZE,1);	// POST with data lenght
	packet=calloc(M8,1);			// final packet to send
	answer=calloc(BUFSIZE,1);	// buf for clickhouse shit
	if (argc>=3) {	
		strncpy(ip,argv[1],256); port=atoi(argv[2]);
		if (argc==4) strncpy(lpath,argv[3],sizeof(lpath));
	}
	f= reopen(f,lpath);
	sock= reconnect(sock,ip,port);
	for (;;) {
		nt=time(NULL);
		for (et=ofs=0; et<INTERVAL && ofs < M8/2; ) {
			if (fgets(buf,BUFSIZE-1,f)<=0) {				/* read row */
				if (stat(lpath,&st)==-1) {					/* problems */
					printf("\n%s ",lpath);
					perror("stat()");							/* no file */
					sleep(5); 
				} else
				if (st.st_size < ftell(f)) {				/* was truncated */
					printf("\n%s trunkated\n",lpath);
					f= reopen(f,lpath);						
				} else 
				if (feof(f)) printf("]");					/* EOF reached */
				else {
					perror("fgets()");
					printf("?");								/* other */
				}
				fflush(stdout);fflush(stderr);
				sleep(1);										/* pause anycase */
			} else {												/* read row OK */
				printf(".");									
				if (!(n=replace(buf,row))) continue;	/* buf -> row */	
				if (ofs+n >= M8/2) break;					
				memcpy(data+ofs,row,n);						/* pack row -> data */
				ofs+=n;
			}
			et=(time(NULL)-nt);								/* calculate elapsed time */
		}
		nn=post(header,ofs,ip,port);						/* nn - header lenght , ofst - data lenght */
		memcpy(packet,header,nn);							
		memcpy(packet+nn,data,ofs);						/* push all shit into a packet */
		while ((r= send(sock,packet,nn+ofs,MSG_NOSIGNAL))!=nn+ofs) {
				printf("\nsent%d/%d\n",nn+ofs,r);
				perror("send()"); 					
            fflush(stdout); 								/* send failure */
            fflush(stderr);
            sock= reconnect(sock,ip,port);
            sleep(3);
            puts("re-send");fflush(stdout);	
		}
		printf(">");fflush(stdout);						/* data sent success */
		if ((r= recv(sock,answer,							/* now receive clickhouse's answer */
		BUFSIZE,0))>0&&r<BUFSIZE) {
				answer[r]='\0';
				printf("\n--[answer %dB]--\n",r);
				puts(answer);
				puts("----");
				fflush(stdout);
		} else perror("recv()"); 
		sleep(5);
		sock =reconnect(sock,ip,port);					/* reconnect after each injection */
	}
	puts("bye!"); 												/* you can't see it */
   return 0;
}

