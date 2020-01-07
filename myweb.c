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
void feed_dynamic_get(int fd,char *filename,char *cgiargs);
void feed_dynamic_post(int fd,char *filename);
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

void *serve_client(void *vargp){
	int conn_socks=*((int *)vargp);
	pthread_detach(pthread_self());
	free(vargp);
	process_trans(conn_socks);
	close(con_socks);
	return NULL;
}

void process_trans(int fd){
	int static_flag,cgi=0;
	struct stat sbuf;
	char buf[1024],method[1024],uri[1024],version[1024];
	char filename[1024],cgiargs[1024];
	rio_t rio;

	rio_readinitb(&rio,fd);
	rio_readlineb(&rio,buf,1024);
	sscanf(buf,"%s %s %s",method,uri,version);
	if(strcasecmp(method,"GET")&&strcasecmp(method,"POST")){
		unimplemented(fd);
		return;
	}

	read_requesthdrs(&rio);

	static_flag=is_static(uri);
	if(static_flag)
		parse_static_uri(uri,filename);
	else
		parse_dynamic_uri(uri,filename,cgiargs);

	if(strcasecmp(method,"POST")==0){
		if(stat(filename,&sbuf)<0){
			error_request(fd,filename,"404","Not found","myweb could not find this file");
			return;
		}

		if(static_flag){
			if(!S_ISREG(sbuf.st_mode))||!(S_IRUSR&sbuf.st_mode)){
				error_request(fd,filename,"403","Forbidden","myweb is not permtted to read the file");
				return;
			}
			feed_static(fd,filename,sbuf.st_size);
		}
		else{
			if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR&sbuf.st_mode)){
				error_request(fd,filename,"403","Forbidden","myweb could not run the CGI program");
				return;
			}
			feed_dynamic_post(fd,filename,cgiargs);
		}
	}

	if(strcasecmp(method,"GET")==0){

		if(stat(filename,&sbuf)<0){
			error_request(fd,filename,"404","Not found","myweb could not find this file");
			return;
		}

		if(static_flag){
			if(!S_ISREG(sbuf.st_mode))||!(S_IRUSR&sbuf.st_mode)){
				error_request(fd,filename,"403","Forbidden","myweb is not permtted to read the file");
				return;
			}
			feed_static(fd,filename,sbuf.st_size);
		}
		else{
			if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR&sbuf.st_mode)){
				error_request(fd,filename,"403","Forbidden","myweb could not run the CGI program");
				return;
			}
			feed_dynamic_get(fd,filename,cgiargs);
		}
	}
}
