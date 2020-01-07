#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
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
#include<sys/socket.h>
#include<fcntl.h>
#include<sys/mman.h>

#define RIO_BUFSIZE 4096
#define MAXLINE 4096
#define MAXBUF 4096
#define SERVER_STRING "Server:jdbhttpd/0.1.0\r\n"
typedef struct{
	int rio_fd;
	int rio_cnt;
	char *rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
}rio_t;
typedef struct sockaddr SA;
void open_listen_sock(int fd);
void *serve_client(void *vargp);
void process_trans(int fd);
void read_requesthdrs(rio_t *rp);
int is_static(char *uri);
void parse_static_uri(char *uri,char *filename);
void parse_dynamic_uri(char *uri,char *filename,char *cgiargs);
void feed_static(int fd,char *filename,int filesize);
void get_filetype(char *filename,char *filetype);
void feed_dynamic_get(int fd,char *filename,char *cgiargs);
void feed_dynamic_post(int fd,char *filename,char *cgiargs);
void error_request(int fd,char *cause,char *errnum,char *shortmsg,char *description);
void bad_request(int fd);
void unimplemented(int fd);
void cannot_execute(int fd);
int get_line(int fd,char *buf,int size);
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
void open_listen_sock(int fd){
	int listen_sock,optval=1;
	struct sockaddr_in serveraddr;
	
	if((listen_sock=socket(AF_INET,SOCK_STREAM,0))<0) return -1;
	if(setockopt(listen_sock,SQL_SOCKET,SO_REUSEADDR,(const void*)&optval,sizeof(int))<0) return -1;
	bzero((char*)&serveraddr,sizeof(serveraddr));
	serveraddr.sin_family=AF_INET;
	serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
	serveraddr.sin_port=htons((unsighed short)port);
	if(bind(listen_sock,(SA*)&serveraddr,sizeof(serveraddr))<0) return -1;
	if(listen(listen_sock,LISTENQ)<0) return -1;
	return listen_sock;
}
void *serve_client(void *vargp){
	int conn_socks=*((int *)vargp);
	pthread_detach(pthread_self());
	free(vargp);
	process_trans(conn_socks);
	close(conn_socks);
	return NULL;
}

void process_trans(int fd){
	int static_flag;
	struct stat sbuf;
	char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
	char filename[MAXLINE],cgiargs[MAXLINE];
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
	if(!strstr(uri,"cgi-bin")) return 1;
	else return 0;
}

void error_request(int fd,char *cause,char *errnum,char *shortmsg,char *description){
	char buf[MAXLINE],body[MAXBUF];
	sprintf(body,"<html><title>error request</title>");
	sprintf(body,"%s<body bgcolor=""ffffff"">\r\n",body);
	sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
	sprintf(body,"%s<p>%s: %s\r\n",body,description,cause);
	sprintf(body,"%s<hr><em>myweb Web Server</em>\r\n",body);
	
	sprintf(buf,"HTTP/1.0%s%s\r\n",errnum,shortmsg);
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-type:text/html\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-length:%d\r\n\r\n",(int)strlen(body));
	rio_writen(fd,buf,strlen(buf));
	rio_writen(fd,body,strlen(body));
}

void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	rio_readlineb(rp,buf,MAXLINE);
	while(strcmp(buf,"\r\n")){
		printf("%s",buf);
		rio_readlineb(rp,buf,MAXLINE);
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
	char *srcp,filetype[MAXLINE],buf[MAXBUF];
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

void feed_dynamic_get(int fd,char *filename,char *cgiargs){
	char buf[MAXLINE],*emptylist[]={NULL};
	int pfd[2];

	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Server:myweb Web Server\r\n");
	rio_writen(fd,buf,strlen(buf));
	
	pipe(pfd);
	if(fork()==0){
		close(pfd[1]);
		dup2(pfd[0],STDIN_FILENO);
		dup2(fd,STDOUT_FILENO);
		execve(filename,emptylist,environ);
	}
	close(pfd[0]);
	write(pfd[1],cgiargs,strlen(cgiargs)+1);
	wait(NULL);
	close(pfd[1]);
}

void feed_daynamic_post(int fd,char *filename,char *cgiars){
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars=1;
	int content_length=-1;

	buf[0]='A';
	buf[1]='\0';

	numchars=get_line(fd,buf,sizeof(buf));
	while((numchars>0)&&strcmp("\n",buf)){
		buf[15]='\0';
		if(strcasecmp(buf,"Content-Length:")==0)
			content_length=atoi(&(buf[16]));
		numchars=get_line(fd,buf,sizeof(buf));
	}
	if(content_length==-1){
		bad_request(fd);
		return;
	}

	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	send(fd,buf,strlen(buf),0);

	if(pipe(cgi_output)<0){
		cannot_execute(fd);
		return;
	}
	if(pipe(cgi_input)<0){
		cannot_execute(fd);
		return;
	}
	if((pid=fork())<0){
		cannot_execute(fd);
		return;
	}
	if(pid==0){
		char meth_env[255];
		char length_env[255];

		dup2(cgi_output[1],1);
		dup2(cgi_input[0],0);
		close(cgi_output[0]);
		close(cgi_input[1]);
		sprintf(meth_env,"REQUEST_METHOD=%s","POST");
		putenv(meth_env);

		sprintf(length_env,"CONTENT_LENGTH=%d",content_length);
		putenv(length_env);

		execl(filename,filename,NULL);
		exit(0);
	}else{
		close(cgi_output[1]);
		close(cgi_input[0]);
		for(i=0;i<content_length;i++){
			recv(fd,&c,1,0);
			write(cgi_input[1],&c,1);
		}
		while(read(cgi_output[0],&c,1)>0) send(fd,&c,1,0);
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid,&status,0);
	}
}

void unimplemented(int fd){
	char buf[1024];

	sprintf(buf,"HTTP/1.0 501 Method Not Implemented\r\n");
	send(fd,buf,strlen(buf),0);

	sprintf(buf,SERVER_STRING);
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"Content-Type:text/html\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"</TITLE></HEAD>\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"<BODY><P>HTTP request method not supported.\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(fd,buf,strlen(buf),0);
}

void bad_request(int fd){
	char buf[1024];

	sprintf(buf,"HTTP/1.0 400 BAD REQUEST\r\n");
	send(fd,buf,sizeof(buf),0);
	sprintf(buf,"Content-type:text/htm\r\n");
	send(fd,buf,sizeof(buf),0);
	sprintf(buf,"\r\n");
	send(fd,buf,sizeof(buf),0);
	sprintf(buf,"<P>Your browser sent a bad request,");
	send(fd,buf,sizeof(buf),0);
	sprintf(buf,"such as a POST without a Content-Length.\r\n");
	send(fd,buf,sizeof(buf),0);
}

void cannot_execute(int fd){
	char buf[1024];
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"Content-type:text/html\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(fd,buf,strlen(buf),0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(fd,buf,strlen(buf),0);
}

