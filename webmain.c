#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<strings.h>
#include<string.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/wait.h>
#include<stdlib.h>

void *serve_client(void *vargp);
void process_trans(int fd);
void read_requesthdrs(rio_t *rp);
int is_static(char *uri);
void parse_static_uri(char *uri,char *filename);
void parse_dynamic_uri(char *uri,char *filename,char *cgiargs);
void feed_static(int fd,char *filename,int filesize);
void get_filetype(char *filename,char *filetype);
void feed_dynamic(int fd,char *filename,char *cgiargs);
void error_request(int fd,char *cause,char *errnum,char *shortmsg,char *description);

int main(int argc,char **argv){
	int listen_sock,*conn_sock,port;
	socklen_t clientlen=sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	pthread_t tid;
	if(argc!=2){
		fprintf(stderr,"usage:%s<port>\n",argv[0]);
		exit(1);
	}
	port=atoi(argv[1]);
	listen_sock=open_listen_sock(port);
	while(1){
		conn_sock=malloc(sizeof(int));
		*conn_sock=accept(listen_sock,(SA*)&clientaddr,&clientlen);
		pthread_create(&tid,NULL,serve_client,conn_sock);
	}
}
