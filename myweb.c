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
			if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR&sbuf.st_mode)){
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
			if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR&sbuf.st_mode)){
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

int is_static(char *uri){
	if(!struct(uri,"cgi-bin")) return 1;
	else return 0;
}

void error_request(int fd,char *cause,char *errnum,char *shortmsg,char *description){
	char buf[1024],body[1024];
	sprintf(body,"<html><title>error request</title>");
	sprintf(body,"%s<body bgcolor=""fffff"">\r\n",body);
	sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
	sprintf(body,"%s<p>%s: %s\r\n",body,description,cause);
	sprintf(body,"%s<hr><em>myweb Web Server</em>\r\n",body);
	
	sprintf(buf,"HTTP/1.0%s%s\r\n",errnum,shortmsg);
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-type:text/html\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-length:%d\r\n\r\n",(int)srelen(body));
	rio_writen(fd,buf,strlen(buf));
	rio_writen(fd,body,strlen(body));
}

void read_requesthdrs(rio_t *rp){
	char buf[1024];

	rio_readlineb(rp,buf,1024);
	while(strcmp(buf,"\r\n")){
		printf("%s",buf);
		rio_readlineb(rp,buf,1024);
	}
	return;
}

void parse_static_uri(char *uri,char *filename){
	char *ptr;
	strcpy(filename,".");
	strcat(filename,uri);
	if(uri[strlen(uri-1)]=='/') strcat(filename,"home.html");
}

void parse_dynamic_uri(char *uri,char *filename,char *cgiargs){
	char *ptr;
	ptr=index(uri,'?');
	if(ptr){
		strcpy(cgiargs,ptr+1);
		*ptr='\0';
	}
	else strcpy(cgiargs,"");
	strcpy(filename,".");
	strcat(filename,uri);
}

void feed_static(int fd,char *filename,int filesize){
	int srcfd;
	char *srcp,filetype[1024],buf[1024];
	get_filetype(filename,filetype);
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	sprintf(buf,"%sServer:myweb Web Server\r\n",buf);
	sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
	sprintf(buf,"%sContent-type:%s\r\n\r\n",buf,filetype);
	rio_writen(fd,buf,strlen(buf));

	srcfd=open(filename,O_RDONLY,0);
	srcp=mmap(0,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);
	close(srcfd);
	rio_writen(fd,srcp,filesize);
	munmap(srcp,filesize);
}

void get_filetype(char *filename,char *filetype){
	if(strstr(filename,".html")) strcpy(filename,"text/html");
	else if(strstr(filename,".jpg")) strcpy(filetype,"image/jpeg");
	else if(strstr(filename,".mpeg")) strcpy(filename,"video/mpeg");
	else strcpy(filetype,"text/html");
}

