#include<stdio.h>
#include<stdlib.h>
#include<string.h>
int main(void)
{
	int m=0, n=0;
	scanf("id=%d&pwd=%d",&m,&n);
	printf("<head><meta charset='utf-8'/>");
	printf("<TITLE>subtraction operation</TITLE><br></head>");
	printf("<H1>欢迎使用用户信息管理系统!!!</H1><br> ");
	printf("<H2>您好，当前身份为普通用户</H2><br> ");
	printf("<H2>您的帐号: %d <br>您的密码: %d <H2>",m,n);
	return 0;
}