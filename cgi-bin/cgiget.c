#include<stdio.h>
#include<stdlib.h>

int main(void)
{
	char *data;
	int m=0, n=0;
	scanf("m=%d&n=%d",&m,&n);
	printf("<head><meta charset='utf-8'/>");
	printf("<TITLE>Multiplication operation</TITLE><br></head>");
	printf("<H3>乘法运算结果：</H3><br> ");
	printf("<H2>%d x %d = %d</H2>",m,n,m*n);

	return 0;
}