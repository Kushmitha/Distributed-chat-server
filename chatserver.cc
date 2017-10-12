#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<ctype.h>
#include <queue>
#include <vector>
#include<netinet/in.h>
#include<strings.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<signal.h>
#include<map>
#include<iostream>

using std::priority_queue;
using std::vector;
using std::multimap;
using std::cout;
using std::endl;
using std::pair;

char *ordering = NULL;
char *servers = NULL;
int vflag = 0;
int server_pos = 0;
int num_servers = 0;
int num_local_clients = 0;
int total_clients = 0;
int servers_updated = 0;
const int MAX_SERVERS = 10;
const int MAX_GROUPS = 10;
const int MAX_MSGS = 10000;
int num_msgs_sent = 0;

struct queue_value {
	int grp_no;
	char *msg;
};

struct broadcast {
	//int msg_no;
	int num_pi;
	int pi;
} B_msg[MAX_MSGS];

//Priority Queue
class Pair {
public:
	int priority;
	queue_value value;

	Pair(int priority, queue_value value) :
			priority(priority), value(value) {
	}
};

class PairCompare {
public:
	bool operator()(const Pair &t1, const Pair &t2) const {
		return t1.priority > t2.priority;
	}
};

struct server_struct {
	int is_active;
	char* forward_address;
	char* bind_address;
} server_list[100];

struct client_struct {
	sockaddr_in client;
	int group_no;
	char nickname[25];
	int seq_no;
} client_list[100];

struct c_table {
	char* client_addr;
} client_table[250];

//check from server
bool fromServer(char* data) {
	if (strncmp(data, "MeSsAiGeFrUmSeRvAaR", 19) == 0)
		return true;
	else
		return false;
}

//check from server for total ordering
int fromServer_total(char* data) {
	if (strncmp(data, "MeSsAiGeBrOaDcAsT", 17) == 0)
		return 1;
	else if (strncmp(data, "MeSsAiGePrOpOsE", 15) == 0)
		return 2;
	else if (strncmp(data, "MeSsAiGeAcCePt", 14) == 0)
		return 3;
	else
		return 0;
}

//returns max of two int
int max(int a, int b) {
	if (a >= b)
		return a;
	else
		return b;
}

//check if message is from client
void isClientCheck(sockaddr_in src) {
	int i = 0;
	for (i = 0; i < num_local_clients; i++)
		if (src.sin_addr.s_addr == client_list[i].client.sin_addr.s_addr
				&& src.sin_port == client_list[i].client.sin_port)
			break;
	if (i == num_local_clients) {
		client_list[num_local_clients].client = src;
		bzero(client_list[i].nickname, 25);
		//client_list[i].group_no=-1;
		num_local_clients++;
		return;
	} else {
		//printf("\nClient exists");
		return;
	}
}

//read from server.txt and update the structure
void update_server_list() {
	int i = 0, j = 0, k = 0;
	FILE *f;
	f = fopen(servers, "r");
	char* buf = (char*) calloc(1000, 1);
	while (true) {
		bzero(buf, 1000);
		if (fgets(buf, 100, f) != NULL) {
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = '\0';
			server_list[num_servers].is_active = 1;
			server_list[num_servers].bind_address = (char*) calloc(20, 1);
			server_list[num_servers].forward_address = (char*) calloc(20, 1);
			if (strstr(buf, ",") != NULL) {
				j = 0;
				while (buf[j] != ',')
					j++;
				for (k = 0; j < strlen(buf); k++, j++)
					server_list[num_servers].bind_address[k] = buf[j];
				server_list[num_servers].bind_address[k] = '\0';

			} else {
				strcpy(server_list[num_servers].bind_address, buf);
				strcpy(server_list[num_servers].forward_address, buf);
				num_servers++;
				continue;
			}

			server_list[num_servers].bind_address++;
			strncpy(server_list[num_servers].forward_address, buf,
					strlen(buf) - strlen(server_list[num_servers].bind_address)
							- 1);
			num_servers++;
		} else
			break;
	}
	fclose(f);
	servers_updated = 1;
}

//parse commands from client(stdin)
int parse_msg(char* msg) {
	if (strncmp(msg, "/join ", 6) == 0)
		return 1;
	else if (strncmp(msg, "/part", 5) == 0)
		return 2;
	else if (strncmp(msg, "/nick ", 6) == 0)
		return 3;
	else
		return 4;
}

//unordered multicast
void unordered_multicast() {
	int i = 0, j = 0;
	char* buf = (char*) calloc(10000, 1);
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	char *port = strstr(server_list[server_pos - 1].bind_address, ":");
	char *t = NULL;
	char *msg_to_send = (char*) calloc(1100, 1);
	port++;
	servaddr.sin_port = htons(atoi(port));
	bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr));
	struct sockaddr_in src;
	socklen_t srclen = sizeof(src);
	char* server_ip = (char*) calloc(20, 1);
	char* server_port;
	char *nick = NULL;
	int forward = 0;
	char* grp_no;
	char* temp = (char*) calloc(10, 1);
	char* log = (char*) calloc(100, 1);
	char *temp_buf = (char*) calloc(1000, 1);
	while (true) {
		bzero(buf, 1000);
		int rlen = recvfrom(sock, buf, 1000, 0, (struct sockaddr*) &src,
				&srclen);
		buf[rlen] = 0;
		if (fromServer(buf)) {
			int i = 0;
			grp_no = NULL;
			if (vflag)
				fprintf(stderr,
						"Message received by Server and sent by server: %s",
						buf + 19);
			for (i = 0; i < num_local_clients; i++) {
				grp_no = strstr(buf, "$$");
				//grp_no[strlen(grp_no)-1]='\0';
				grp_no++;
				grp_no++;
				bzero(temp_buf, 1000);
				strncpy(temp_buf, buf, strlen(buf) - strlen(grp_no) - 2);
				//strcat(temp_buf,"\n");
				if (client_list[i].group_no == atoi(grp_no)) {
					sendto(sock, temp_buf + 19, strlen(temp_buf) - 19, 0,
							(struct sockaddr*) &client_list[i].client,
							sizeof(client_list[i].client));
					//free(buffer);
				}
			}

		} else {
			isClientCheck(src);
			if (vflag)
				fprintf(stderr,
						"\nMessage received by Server and sent by client: %s",
						buf);
			//sendto(sock,"+OK You are now in chat room #\n",100, 0, (struct sockaddr*) &src,sizeof(src));
			bzero(log, 100);
			grp_no = NULL;
			forward = 0;
			switch (parse_msg(buf)) {
			case 1:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							grp_no = strstr(buf, " ");
							grp_no++;
							if (atoi(grp_no) <= 10 && atoi(grp_no) > 0) {
								client_list[i].group_no = atoi(grp_no);
								sprintf(log,
										"+OK You are now in chat room #%d\n",
										client_list[i].group_no);
							} else
								strcpy(log,
										"-ERR Enter a room number between 1 and 10!\n");
							break;
						} else {
							sprintf(log, "-ERR You are already in room #%d\n",
									client_list[i].group_no);
						}
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 2:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							strcpy(log,
									"-ERR You are not a part of a chat room\n");
						} else {
							sprintf(log, "+OK You have left chat room #%d!\n",
									client_list[i].group_no);
							client_list[i].group_no = 0;
						}

					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 3:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						//client_list[i].nick=(char*)calloc(20,1);
						nick = strstr(buf, " ");
						nick++;
						nick[strlen(nick) - 1] = '\0';
						strcpy(client_list[i].nickname, nick);
						sprintf(log, "+OK Nickname set to \'%s\'!\n",
								client_list[i].nickname);
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 4:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							strcpy(log, "-ERR Join a chat room!\n");
							sendto(sock, log, 100, 0, (struct sockaddr*) &src,
									sizeof(src));
						} else
							forward = 1;
					}

				break;
			}

			if (forward) {
				//Insert <from> before message
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						bzero(temp_buf, 1000);
						buf[strlen(buf)] = '\0';
						if (client_list[i].nickname[0] != '\0')
							sprintf(temp_buf, "<%s>%s$$%d\n",
									client_list[i].nickname, buf,
									client_list[i].group_no);
						else
							sprintf(temp_buf, "<%s:%d>%s$$%d\n",
									inet_ntoa(client_list[i].client.sin_addr),
									client_list[i].client.sin_port, buf,
									client_list[i].group_no);
						bzero(buf, 1000);
						strcpy(buf, temp_buf);
						//sendto(sock,buf, strlen(buf), 0,
						//	(struct sockaddr*) &src, sizeof(src));
						if (vflag)
							fprintf(stderr, "\nSend to servers: %s ", buf);
					}
				for (i = 0; i < num_servers; i++) {
					//if(i!=server_pos-1)
					//{
					bzero(server_ip, 20);
					server_port = NULL;
					bzero(msg_to_send, 1100);
					strcpy(temp, server_list[i].forward_address);
					server_port = strstr(temp, ":");
					server_port++;
					strncpy(server_ip, server_list[i].forward_address,
							strlen(server_list[i].forward_address)
									- strlen(server_port) - 1);
					inet_pton(AF_INET, server_ip, &src.sin_addr);
					src.sin_port = htons(atoi(server_port));
					sprintf(msg_to_send, "%s%s", "MeSsAiGeFrUmSeRvAaR", buf);
					sendto(sock, msg_to_send, strlen(msg_to_send), 0,
							(struct sockaddr*) &src, sizeof(src));
					//}
				}
			}
		}
	}

}

//fifo multicast
void fifo_multicast() {
	int i = 0, j = 0;
	int S_N[250] = { 0 };
	priority_queue<Pair, vector<Pair>, PairCompare> msg_queue[250];
	int client_index = 0;
	char* buf = (char*) calloc(10000, 1);
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	struct queue_value p;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	char *port = strstr(server_list[server_pos - 1].bind_address, ":");
	char *t = NULL;
	char *msg_to_send = (char*) calloc(1100, 1);
	port++;
	servaddr.sin_port = htons(atoi(port));
	bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr));
	struct sockaddr_in src;
	socklen_t srclen = sizeof(src);
	char* server_ip = (char*) calloc(20, 1);
	char* server_port;
	char *nick = NULL;
	int forward = 0;
	char* grp_no = NULL;
	char* seq_no = NULL;
	char *message = NULL;
	char *client_address = NULL;
	char* client_address_list[250];
	char* temp = (char*) calloc(10, 1);
	char* log = (char*) calloc(100, 1);
	char *temp_buf = (char*) calloc(1000, 1);
	while (true) {
		bzero(buf, 1000);
		int rlen = recvfrom(sock, buf, 1000, 0, (struct sockaddr*) &src,
				&srclen);
		buf[rlen] = 0;
		//fprintf(stderr, "\nBuffer1: %s", buf);
		if (fromServer(buf)) {
			//fprintf(stderr, "\n\nhiiii");
			bzero(temp_buf, 1000);
			t = NULL;
			strcpy(temp_buf, buf + 19);
			if (vflag)
				fprintf(stderr,
						"\nMessage received by Server and sent by server: %s",
						temp_buf);
			t = strtok(temp_buf, "$$");
			message = t;
			t = strtok(NULL, "$$");
			grp_no = t;
			t = strtok(NULL, "$$");
			seq_no = t;
			t = strtok(NULL, "$$");
			client_address = t;
			client_index = 0;
			//fprintf(stderr,"\nTokens:%s %s %s %s",message,grp_no,seq_no,client_address);
			for (i = 0; i < total_clients; i++) {
				if (strcmp(client_table[i].client_addr, client_address) == 0) {
					client_index = i;
					break;
				}
			}
			if (i == total_clients) {
				client_table[total_clients].client_addr = (char*) calloc(20, 1);
				strcpy(client_table[total_clients].client_addr, client_address);
				client_index = total_clients;
				total_clients++;
			}
			bzero(msg_to_send, 1100);
			sprintf(msg_to_send, "%s", message);
			p.grp_no = atoi(grp_no);
			p.msg = (char*) calloc(1100, 1);
			strcpy(p.msg, msg_to_send);
			if (vflag)
				fprintf(stderr, "\nPushing message: %s", msg_to_send);
			msg_queue[client_index].push(Pair(atoi(seq_no), p));
			while (!msg_queue[client_index].empty()) {
				Pair tmp = msg_queue[client_index].top();
				if (tmp.priority == S_N[client_index] + 1) {
					for (i = 0; i < num_local_clients; i++)
						if (tmp.value.grp_no == client_list[i].group_no)
							sendto(sock, tmp.value.msg, strlen(tmp.value.msg),
									0,
									(struct sockaddr*) &client_list[i].client,
									sizeof(client_list[i].client));
					if (vflag)
						fprintf(stderr, "Popped: %s", tmp.value.msg);
					//fprintf(stderr, "\nPopped: %s", tmp.value.msg);
					S_N[client_index]++;
					msg_queue[client_index].pop();
				} else {
					break;
				}
			}

		} else {
			isClientCheck(src);
			if (vflag)
				fprintf(stderr,
						"\nMessage received by Server and sent by client: %s",
						buf);
			//sendto(sock,"+OK You are now in chat room #\n",100, 0, (struct sockaddr*) &src,sizeof(src));
			bzero(log, 100);
			grp_no = NULL;
			forward = 0;
			switch (parse_msg(buf)) {
			case 1:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						client_index = i;
						if (client_list[i].group_no == 0) {
							grp_no = strstr(buf, " ");
							grp_no++;
							if (atoi(grp_no) <= 10 && atoi(grp_no) > 0) {
								client_list[i].group_no = atoi(grp_no);
								sprintf(log,
										"+OK You are now in chat room #%d\n",
										client_list[i].group_no);
							} else
								strcpy(log,
										"-ERR Enter a room number between 1 and 10!\n");
							break;
						} else {
							sprintf(log, "-ERR You are already in room #%d\n",
									client_list[i].group_no);
						}
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 2:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						client_index = i;
						if (client_list[i].group_no == -1) {
							strcpy(log,
									"-ERR You are not a part of a chat room\n");
						} else {
							sprintf(log, "+OK You have left chat room #%d!\n",
									client_list[i].group_no);
							client_list[i].group_no = 0;
						}

					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 3:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						client_index = i;
						//client_list[i].nick=(char*)calloc(20,1);
						nick = strstr(buf, " ");
						nick++;
						nick[strlen(nick) - 1] = '\0';
						strcpy(client_list[i].nickname, nick);
						sprintf(log, "+OK Nickname set to \'%s\'!\n",
								client_list[i].nickname);
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 4:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port
							&& client_list[i].group_no != 0) {
						client_index = i;
						forward = 1;
						client_list[i].seq_no++;
					}
				if (forward == 0) {
					strcpy(log, "-ERR Join a chatroom!\n");
					sendto(sock, log, 100, 0, (struct sockaddr*) &src,
							sizeof(src));
				}
				break;
			}

			if (forward) {
				//Insert <from> before message
				bzero(temp_buf, 1000);
				buf[strlen(buf)] = '\0';
				if (client_list[client_index].nickname[0] != '\0')
					sprintf(temp_buf, "<%s>%s$$%d\n",
							client_list[client_index].nickname, buf,
							client_list[client_index].group_no);
				else
					sprintf(temp_buf, "<%s:%d>%s$$%d\n",
							inet_ntoa(
									client_list[client_index].client.sin_addr),
							client_list[client_index].client.sin_port, buf,
							client_list[client_index].group_no);
				bzero(buf, 1000);
				strcpy(buf, temp_buf);
				//append metadata
				bzero(msg_to_send, 1100);
				buf[strlen(buf) - 1] = '\0';
				sprintf(msg_to_send, "%s%s$$%d$$%s:%d\n", "MeSsAiGeFrUmSeRvAaR",
						buf, client_list[client_index].seq_no,
						inet_ntoa(client_list[client_index].client.sin_addr),
						client_list[client_index].client.sin_port);
				//fprintf(stderr,"\nMsg: %s",msg_to_send);
				//send to other servers
				//fprintf(stderr, "\nSend to servers:%s", msg_to_send);
				for (i = 0; i < num_servers; i++) {

					bzero(server_ip, 20);
					bzero(temp, 10);
					server_port = NULL;
					strcpy(temp, server_list[i].forward_address);
					server_port = strstr(temp, ":");
					server_port++;
					strncpy(server_ip, server_list[i].forward_address,
							strlen(server_list[i].forward_address)
									- strlen(server_port) - 1);
					inet_pton(AF_INET, server_ip, &src.sin_addr);
					src.sin_port = htons(atoi(server_port));
					sendto(sock, msg_to_send, strlen(msg_to_send), 0,
							(struct sockaddr*) &src, sizeof(src));
				}
			}
		}
	}

}

struct queue_value_total {
	char *msg;
	int del_status;
};

//total multicast
void total_multicast() {
	multimap<int, queue_value_total> msg_map;
	std::pair<std::multimap<int, queue_value_total>::iterator,
			std::multimap<int, queue_value_total>::iterator> ret;
	multimap<int, queue_value_total>::iterator it, it1, it2;
	struct queue_value_total temp_total;
	int sock = socket(PF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in servaddr;
	char* buffer = (char*) calloc(1000, 1);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	char *port = strstr(server_list[server_pos - 1].bind_address, ":");
	char *temp_buf = (char*) calloc(1000, 1);
	char *t = NULL, *nick = NULL, *s = NULL;
	char min_node[1000];
	char* server_port;
	char* server_ip = (char*) calloc(20, 1);
	char *grp_no;
	int l, m;
	int forward = 0, client_index = 0;
	char* log = (char*) calloc(100, 1);
	char *msg_to_send = (char*) calloc(1100, 1);
	char* temp = (char*) calloc(10, 1);
	port++;
	servaddr.sin_port = htons(atoi(port));
	bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr));
	int i = 0, j = 0, k = 0;
	int P_N[MAX_GROUPS] = { 0 };			//highest proposed number for groups
	int A_N[MAX_GROUPS] = { 0 };			//highest agreed number for groups
	char* buf = (char*) calloc(10000, 1);
	struct sockaddr_in src;
	socklen_t srclen = sizeof(src);
	while (true) {
		bzero(buf, 1000);
		int rlen = recvfrom(sock, buf, 1000, 0, (struct sockaddr*) &src,
				&srclen);
		buf[rlen] = 0;
		//fprintf(stderr, "\nBuffer: %s", buf);
		i = 0;
		i = fromServer_total(buf);
		if (i) {
			if (vflag)
				fprintf(stderr, "\nServer-server: %s", buf);
			if (i == 1) //message starting with broadcast
					{
				bzero(temp_buf, 1000);
				strcpy(temp_buf, buf);
				t = strtok(temp_buf, "$$");
				t = strtok(NULL, "$$");
				grp_no = t;
				temp_total.msg = (char*) calloc(1000, 1);
				strcpy(temp_total.msg, buf);
				temp_total.del_status = 0;
				//fprintf(stderr, "\n Group: %d", atoi(grp_no));
				P_N[atoi(grp_no) - 1] = max(P_N[atoi(grp_no) - 1],
						A_N[atoi(grp_no) - 1]) + 1;
				it = msg_map.find(P_N[atoi(grp_no) - 1]);
				if (it == msg_map.end())
					i = P_N[atoi(grp_no) - 1];
				else
					i = A_N[atoi(grp_no) - 1] + 1;
				msg_map.insert(pair<int, queue_value_total>(i, temp_total));
				bzero(temp_buf, 1000);
				buf[strlen(buf) - 1] = '\0';
				sprintf(temp_buf, "MeSsAiGePrOpOsE%s$$%d\n", buf, i);
				//fprintf(stderr, "\nCheck 1 : %s\n", temp_buf);
				sendto(sock, temp_buf, 1000, 0, (struct sockaddr*) &src,
						sizeof(src));
			} else if (i == 2) //message starting with proposed
					{
				if (vflag)
					fprintf(stderr, "\nProposed message: %s", buf);
				bzero(temp_buf, 1000);
				strcpy(temp_buf, buf);
				t = strtok(temp_buf, "$$");
				t = strtok(NULL, "$$");
				t = strtok(NULL, "$$");
				t = strtok(NULL, "$$");
				j = atoi(t); //msg no in the B_msg[]
				t = strtok(NULL, "$$");
				i = atoi(t); //proposed number from other servers
				//fprintf(stderr,"i %d j %d",i,j);
				if (i > B_msg[j].pi)
					B_msg[j].pi = i;
				B_msg[j].num_pi++;
				if (B_msg[j].num_pi == num_servers) {
					bzero(msg_to_send, 1000);
					buf[strlen(buf) - 1] = '\0';
					sprintf(msg_to_send, "MeSsAiGeAcCePt%s$/$%d", buf,
							B_msg[j].pi);
					//fprintf(stderr, "\nAll proposals received! : %s",msg_to_send);
					//send accept message to other servers
					for (i = 0; i < num_servers; i++) {
						bzero(server_ip, 20);
						bzero(temp, 10);
						server_port = NULL;
						//fprintf(stderr, "\nBroadcast Accept to other servers : %s",msg_to_send);
						strcpy(temp, server_list[i].forward_address);
						server_port = strstr(temp, ":");
						server_port++;
						strncpy(server_ip, server_list[i].forward_address,
								strlen(server_list[i].forward_address)
										- strlen(server_port) - 1);
						inet_pton(AF_INET, server_ip, &src.sin_addr);
						src.sin_port = htons(atoi(server_port));
						sendto(sock, msg_to_send, strlen(msg_to_send), 0,
								(struct sockaddr*) &src, sizeof(src));

					}
				}

			} else if (i == 3)					//message starting with accept
					{
				if (vflag)
					fprintf(stderr, "\nAccept message : %s", buf);
				bzero(temp_buf, 1000);
				strcpy(temp_buf, buf);
				t = strstr(temp_buf, "$/$");
				t++;
				t++;
				t++;
				i = atoi(t);					//accept number
				bzero(temp_buf, 1000);
				strcpy(temp_buf, buf);
				t = strtok(temp_buf, "$$");
				t = strtok(NULL, "$$");
				j = atoi(t); //group number the message belongs
				bzero(temp_buf, 1000);
				strcpy(temp_buf, buf);
				bzero(buffer, 1000);
				t = strtok(temp_buf, "$$");
				strcpy(buffer, t + 29);
				strcat(buffer, "$$");
				t = strtok(NULL, "$$");
				strcat(buffer, t);
				strcat(buffer, "$$");
				t = strtok(NULL, "$$");
				strcat(buffer, t);
				strcat(buffer, "$$");
				t = strtok(NULL, "$$");
				strcat(buffer, t);
				//t=strtok(NULL,"$$");
				strcat(buffer, "\n");
				//for (std::multimap<int,queue_value_total>::iterator it=msg_map.begin(); it!=msg_map.end(); ++it)
				//fprintf(stderr,"\n %d , %s",it->second.del_status,it->second.msg);

				for (it = msg_map.begin(); it != msg_map.end(); ++it)
					if (strcmp(buffer, it->second.msg) == 0) {
						temp_total.del_status = 1;
						//fprintf(stderr, "\n\njoioio");
						temp_total.msg = (char*) calloc(1000, 1);
						strcpy(temp_total.msg, it->second.msg);
						msg_map.erase(it);
						msg_map.insert(
								pair<int, queue_value_total>(i, temp_total));
						break;
					}
				//fprintf(stderr,"\n\njoioio1");
				A_N[j - 1] = max(A_N[j - 1], i);
				j = 0;
				while (!msg_map.empty()) {
					//fprintf(stderr, "inside while");
					it = msg_map.begin();
					l = msg_map.count(it->first); //count of elements with same key
					if (l == 1) {
						if (it->second.del_status == 1) {
							//fprintf(stderr, "\n\njoioio2");
							//strcat(it->second.msg,"\n");
							t = strtok(it->second.msg, "$$");
							//strcat(t,"\n");
							bzero(msg_to_send, 1000);
							strcpy(msg_to_send, t + 17);
							//strcat(msg_to_send,"\n");
							//msg_to_send[strlen(msg_to_send)-1]='\n';
							for (i = 0; i < num_local_clients; i++) {
								if (vflag)
									fprintf(stderr,
											"\n Sending to client%d : %s",
											htons(
													client_list[i].client.sin_port),
											msg_to_send);
								sendto(sock, msg_to_send, strlen(msg_to_send),
										0,
										(struct sockaddr*) &client_list[i].client,
										sizeof(client_list[i].client));
							}
							msg_map.erase(it);
							//break;
						} else
							break;
					} else {
						k = 0;
						ret = msg_map.equal_range(it->first);
						for (it2 = ret.first; it2 != ret.second; ++it2) {
							temp_total = it2->second;
							bzero(temp_buf, 1000);
							strcpy(temp_buf, temp_total.msg);
							t = strtok(temp_buf, "$$");
							//t++;t++;
							//s=strtok(t,"$$");
							//s=strtok(NULL,"$$");
							if (k == 0) {
								bzero(min_node, 1000);
								strcpy(min_node, t);
								k = 1;
							} else {
								if (strcmp(min_node, t) > 0) {
									bzero(min_node, 1000);
									strcpy(min_node, t);
								}
							}
						}

						ret = msg_map.equal_range(it->first);
						j = 0;
						for (it2 = ret.first; it2 != ret.second; ++it2) {
							temp_total = it2->second;
							bzero(temp_buf, 1000);
							strcpy(temp_buf, temp_total.msg);
							s = strtok(temp_buf, "$$");
							//t++;t++;
							//s=strtok(t,"$$");
							//s=strtok(NULL,"$$");
							if (strcmp(s, min_node) == 0) {
								if (it2->second.del_status == 1) {
									//bzero(msg_to_send,1000);
									t = strtok(it2->second.msg, "$$");
									//strcat(t,"\n");
									bzero(msg_to_send, 1000);
									strcpy(msg_to_send, t + 17);
									//strcat(msg_to_send,"\n");
									//msg_to_send[strlen(msg_to_send)-1]='\n';
									for (i = 0; i < num_local_clients; i++) {
										if (vflag)
											fprintf(stderr,
													"\n Sending to client%d : %s",
													htons(
															client_list[i].client.sin_port),
													msg_to_send);
										sendto(sock, msg_to_send,
												strlen(msg_to_send), 0,
												(struct sockaddr*) &client_list[i].client,
												sizeof(client_list[i].client));
									}

									msg_map.erase(it2);
									break;
								} else {
									j = 1;
									break;
								}
							}
						}
					}
					if (j == 1)
						break;
				}

				//fprintf(stderr, "\nMap contents: ");
				/*
				 for (it = msg_map.begin(); it != msg_map.end(); ++it)
				 fprintf(stderr, "\n Key : %d Msg : %s Del_status : %d",
				 it->first, it->second.msg, it->second.del_status);*/
			} else {
				fprintf(stderr, "\n\nERROR");
				exit(1);
			}

		} else {
			isClientCheck(src);
			//fprintf(stderr, "\n\nNUM_CLIENTS:%d", num_local_clients);
			if (vflag)
				fprintf(stderr,
						"\nMessage received by Server and sent by client: %s",
						buf);
			//sendto(sock,"+OK You are now in chat room #\n",100, 0, (struct sockaddr*) &src,sizeof(src));
			bzero(log, 100);
			grp_no = NULL;
			forward = 0;
			switch (parse_msg(buf)) {
			case 1:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							grp_no = strstr(buf, " ");
							grp_no++;
							if (atoi(grp_no) <= 10 && atoi(grp_no) > 0) {
								client_list[i].group_no = atoi(grp_no);
								sprintf(log,
										"+OK You are now in chat room #%d\n",
										client_list[i].group_no);
							} else
								strcpy(log,
										"-ERR Enter a room number between 1 and 10!\n");
							break;
						} else {
							sprintf(log, "-ERR You are already in room #%d\n",
									client_list[i].group_no);
						}
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 2:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							strcpy(log,
									"-ERR You are not a part of a chat room\n");
						} else {
							sprintf(log, "+OK You have left chat room #%d!\n",
									client_list[i].group_no);
							client_list[i].group_no = 0;
						}

					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 3:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						//client_list[i].nick=(char*)calloc(20,1);
						nick = strstr(buf, " ");
						nick++;
						nick[strlen(nick) - 1] = '\0';
						strcpy(client_list[i].nickname, nick);
						sprintf(log, "+OK Nickname set to \'%s\'!\n",
								client_list[i].nickname);
					}
				sendto(sock, log, 100, 0, (struct sockaddr*) &src, sizeof(src));
				break;
			case 4:
				for (i = 0; i < num_local_clients; i++)
					if (src.sin_addr.s_addr
							== client_list[i].client.sin_addr.s_addr
							&& src.sin_port == client_list[i].client.sin_port) {
						if (client_list[i].group_no == 0) {
							strcpy(log, "-ERR Join a chat room!\n");
							sendto(sock, log, 100, 0, (struct sockaddr*) &src,
									sizeof(src));
						} else {
							client_index = i;
							forward = 1;
							break;
						}

					}

				break;
			}
			if (forward) {
				bzero(msg_to_send, 1000);
				buf[strlen(buf)] = '\0';
				//buf[strlen(buf) - 2] = '\0';
				//encapsulate the message with metadata
				if (client_list[client_index].nickname[0] != '\0')
					sprintf(msg_to_send,
							"MeSsAiGeBrOaDcAsT<%s>%s$$%d$$%s:%d$$%d\n",
							client_list[client_index].nickname, buf,
							client_list[client_index].group_no,
							inet_ntoa(
									client_list[client_index].client.sin_addr),
							client_list[client_index].client.sin_port,
							num_msgs_sent);
				else
					sprintf(msg_to_send,
							"MeSsAiGeBrOaDcAsT<%s:%d>%s$$%d$$%s:%d$$%d\n",
							inet_ntoa(
									client_list[client_index].client.sin_addr),
							client_list[client_index].client.sin_port, buf,
							client_list[client_index].group_no,
							inet_ntoa(
									client_list[client_index].client.sin_addr),
							client_list[client_index].client.sin_port,
							num_msgs_sent);

				//B_msg[num_msgs_sent].msg_no = num_msgs_sent;
				//B_msg[num_msgs_sent].pi = max(P_N[client_list[client_index].group_no-1],A_N[client_list[client_index].group_no])+1;
				B_msg[num_msgs_sent].num_pi = 0;
				B_msg[num_msgs_sent].pi = 0;
				num_msgs_sent++;
				//B-multicast to other servers
				for (i = 0; i < num_servers; i++) {
					bzero(server_ip, 20);
					bzero(temp, 10);
					server_port = NULL;
					if (vflag)
						fprintf(stderr, "\nBroadcast to other servers : %s",
								msg_to_send);
					strcpy(temp, server_list[i].forward_address);
					server_port = strstr(temp, ":");
					server_port++;
					strncpy(server_ip, server_list[i].forward_address,
							strlen(server_list[i].forward_address)
									- strlen(server_port) - 1);
					inet_pton(AF_INET, server_ip, &src.sin_addr);
					src.sin_port = htons(atoi(server_port));
					sendto(sock, msg_to_send, strlen(msg_to_send), 0,
							(struct sockaddr*) &src, sizeof(src));
				}

			}

		}

	}

}

//initialize all servers and the ordering
void init_server() {
	if (servers_updated == 0)
		update_server_list();
	if (strcmp(ordering, "unordered") == 0)
		unordered_multicast();
	else if (strcmp(ordering, "fifo") == 0)
		fifo_multicast();
	else if (strcmp(ordering, "total") == 0)
		total_multicast();
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "*** Author: Kushmitha Unnikumar (kushm)\n");
		exit(1);
	}

	char c;
	char* server_position = NULL;

	while ((c = getopt(argc, argv, "vo:")) != -1) {
		switch (c) {
		case 'v':
			vflag = 1;
			break;
		case 'o':
			ordering = optarg;
			break;
		default:
			fprintf(stderr,
					"Syntax: %s [-v] [-o ordering] serverListFile ServerPosition\n",
					argv[0]);
			exit(1);
		}
	}
	if (optind != (argc - 2)) {
		fprintf(stderr,
				"Error: Name of the server list file and/or server position is missing!\n");
		return 1;
	}
	servers = argv[optind];
	server_position = argv[optind + 1];
	server_pos = atoi(server_position);
	if (ordering == NULL) {
		ordering = (char*) calloc(15, 1);
		strcpy(ordering, "unordered");
	}
	init_server();
	return 0;
}
