#include <stdlib.h>
#include <stdio.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "*** Author: KUSHMITHA UNNIKUMAR (kushm)\n");
		exit(1);
	}
	char *port=(char*)calloc(10, 1);
	struct sockaddr_in ServAddr,src;
	fd_set rdset;
	int sock = 0, rc, flag = 0,ip_length;
	socklen_t len;
	char *ip_addr = (char*) calloc(10, 1);
	char *msg_from_user = (char*) calloc(1000, 1);
	char *msg_from_server = (char*) calloc(1000, 1);

	port = strstr(argv[1], ":");
	port++;
	ip_length = strlen(argv[1]) - strlen(port);
	strncpy(ip_addr, argv[1], ip_length - 1);
	//printf("\n%s %s",port,ip_addr);
	bzero(&ServAddr,sizeof(ServAddr));
	ServAddr.sin_family = AF_INET;
	ServAddr.sin_port = htons(atoi(port));
	inet_pton(AF_INET,ip_addr,&(ServAddr.sin_addr));
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		fprintf(stderr, "%s: Cannot open socket \n", argv[0]);
		exit(1);
	}

	int max,rlen;
	int g_no=-1;
	char *t=NULL;
	while (true) //select for stdin and socket
	{
		FD_ZERO((&rdset));
		FD_SET(sock,&rdset);
		FD_SET(STDIN_FILENO,&rdset);
		max=STDIN_FILENO>sock?STDIN_FILENO+1:sock+1;
		select(max,&rdset,NULL,NULL,0);

		if(FD_ISSET(sock,&rdset))//from socket
		{
			bzero(msg_from_server,1000);
			len=sizeof(src);
			rlen=recvfrom(sock,msg_from_server,1000,0,(struct sockaddr*)&src,&len);
			msg_from_server[rlen]=0;
			printf("%s",msg_from_server);
		}
		else if(FD_ISSET(STDIN_FILENO,&rdset))//from stdin
		{
			bzero(msg_from_user,1000);
			fgets(msg_from_user,1000,stdin);
			/*if(msg_from_user[strlen(msg_from_user)-1]=='\n')
				msg_from_user[strlen(msg_from_user)-1]=0;*/
			if(msg_from_user[0]=='/' && msg_from_user[1]=='q' && msg_from_user[2]=='u' && msg_from_user[3]=='i' && msg_from_user[4]=='t')
			{
				printf("\nClient quitting!");
				close(sock);
				exit(0);
			}
			else
			{
				sendto(sock, msg_from_user, strlen(msg_from_user), 0,(struct sockaddr *)&ServAddr, sizeof(ServAddr));
			}
		}
		FD_CLR(sock,&rdset);
		FD_CLR(STDIN_FILENO,&rdset);
	}
	close(sock);
	return 0;
}
