//chat_server_thread.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ctype.h>

void *do_chat(void *);
int pushClient(int);
int popClient(int);

pthread_t thread;
pthread_mutex_t mutex;

#define MAX_CLIENT 10
#define CHATDATA 1024
#define MAX_NAME 16
#define INVALID_SOCK -1

//int list_c[MAX_CLIENT];
struct user
{
	int clientsocket;
	char name[MAX_NAME];
};
struct user userList[MAX_CLIENT];

char escape[] = "exit";
char greeting[] = "Welcome to chtting room\n[To check all participants] participants\n[To check your nickname   ] whoami\n[To change your nickname  ] nickname:(NEW NAME)\n[To whisper to somebody   ] /(somebody)/ (message)\nPress Enter to participate!\n";
char CODE200[] = "Sorry No More Connection\n";
char CODE300[] = "It's not online\n";


main(int argc, char *argv[])
{
	int c_socket, s_socket;
	struct sockaddr_in s_addr, c_addr;
	int len;

	int i, j, n;

	int res;

	if(argc < 2)	{
		printf("usage: %s port_number\n", argv[0]);
		exit(-1);
	}

	if(pthread_mutex_init(&mutex, NULL) != 0)	{
		printf("Can not create mutex\n");
		return -1;
	}

	s_socket = socket(PF_INET, SOCK_STREAM, 0);

	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(atoi(argv[1]));

	if(bind(s_socket, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1)	{
		printf("Can not Bind\n");
		return -1;
	}

	if(listen(s_socket, MAX_CLIENT) == -1)	{
		printf("listen Fail\n");
		return -1;
	}

	memset(&userList, 0, sizeof(userList));
	for(i = 0; i < MAX_CLIENT; i++)
		//list_c[i] = INVALID_SOCK;
		userList[i].clientsocket = INVALID_SOCK;

	while(1)	{
		len = sizeof(c_addr);
		c_socket = accept(s_socket, (struct sockaddr *) &c_addr, &len);

		res = pushClient(c_socket);
		if(res < 0)	{
			write(c_socket, CODE200, strlen(CODE200));
			close(c_socket);
		}else{
			write(c_socket, greeting, strlen(greeting));
			pthread_create(&thread, NULL, do_chat, (void *) c_socket);
		}
	}
}

void *
do_chat(void *arg)
{
	int c_socket = (int)arg;
	char chatData[CHATDATA];
	char sndBuffer[CHATDATA];
	int i,n;
    int SocketIndex;
	int nicklen;
	int spaceBar = 1;

    char *message, *ptr;
	int length;

	//Find its index and register its name
	if((n = read(c_socket, chatData, sizeof(chatData))) > 0)	{
		ptr = strtok(chatData, "[]");
		nicklen = strlen(ptr) + 2 ;	// 2 for [] (square brackets)
		for(i = 0; i < MAX_CLIENT; i++) {
			if(userList[i].clientsocket == c_socket)  {
				SocketIndex = i;
				strcpy(userList[i].name, ptr);
				break;
			}
		}
	}
    
	while(1)	{
		memset(chatData,0,sizeof(chatData));
		memset(sndBuffer,0,sizeof(sndBuffer));
		if((n = read(c_socket,chatData,sizeof(chatData))) > 0)	{
			//At the end of the data, there's a newline (\n, ASCII code 10)
			//A space (ASCII code 32) after [NICKNAME]
			message = chatData + nicklen + spaceBar;	//message points the pure message itself

			// Nickname modify
			if(!strncmp(message, "nickname:", 9))	{
				ptr = message + 9;	//pointer for new nickname

				if(strlen(ptr) >= MAX_NAME)	{
					sprintf(sndBuffer, "LENGTH OF NICKNAME MUST BE SHORTER THAN %d\n", MAX_NAME);
				}
				else	{	//Copy from chatData to userList
					length = 0;

					while(*ptr)	{
						if(*ptr == '\r')	break;
						if(*ptr == '\n')	break;
						if(length + 1 == MAX_NAME) break;
						if(*ptr == ' ')	{	ptr++;	continue;	}

						userList[SocketIndex].name[length] = *ptr;
						ptr++; length++;
					}
					userList[SocketIndex].name[length] = '\0';
					//nicklen = strlen(userList[SocketIndex].name) + 2 ;	// does not change from the client side
					sprintf(sndBuffer, "YOUR NICKNAME IS NOW %s\n", userList[SocketIndex].name);
				}
				write(c_socket, sndBuffer, sizeof(sndBuffer));
				continue;

			}// Check One's name
			else if(!strncmp(message, "whoami", 6))	{
				sprintf(sndBuffer, "YOUR NICKNAME : %s\n", userList[SocketIndex].name);
				write(c_socket, sndBuffer, sizeof(sndBuffer));
				continue;

			}// Check All Participants
			else if(!strncmp(message, "participants", 12))	{
				sprintf(sndBuffer, "---------PARTICIPANTS ARE BELOW-----------\n");
				write(c_socket, sndBuffer, sizeof(sndBuffer));
				for(i = 0, n = 0; i < MAX_CLIENT; i++)	{
					if(userList[i].clientsocket != INVALID_SOCK)	{
						memset(sndBuffer, 0, sizeof(sndBuffer));
						sprintf(sndBuffer, "%2d : %s\n", ++n, userList[i].name);
						write(c_socket, sndBuffer, sizeof(sndBuffer));
					}
				}
				continue;
			}

			
			//whisper function
			else if( message[0] == '/' &&		// first charater == slash 
				n >= nicklen + 4 &&				// equal or greater than 4 (space, slash, character, slash)
				isalnum(message[1]) &&			// character or number right after slash
				strchr(&message[2], '/') )		// after that a slash exists
			{
				ptr = strtok(message, "/");	//destination nickname
				
				//find socket from userList that matches nickname
				for(i = 0; i < MAX_CLIENT; i++)	{
					if(userList[i].clientsocket!=INVALID_SOCK && !strcmp(ptr, userList[i].name))	{
						//write on socket (username)
						ptr = strtok(NULL, "");	//message
						sprintf(sndBuffer, "Whisper From %s :%s", userList[SocketIndex].name, ptr);
						write(userList[i].clientsocket, sndBuffer, sizeof(sndBuffer));
						break;
					}
				}
				//if nickname not found
				if(i == MAX_CLIENT)	{
					write(c_socket, CODE300, sizeof(CODE300));
					continue;
				}
			}
			else	{
				sprintf(sndBuffer, "[%s] %s", userList[SocketIndex].name, message);
				//fputs(sndBuffer, stderr);
                for(i = 0; i < MAX_CLIENT; i++) {
                    if(userList[i].clientsocket != INVALID_SOCK)
                        write(userList[i].clientsocket, sndBuffer, sizeof(sndBuffer));
                }
            }

            if(strstr(chatData,escape) != NULL) {
                popClient(c_socket);
                break;
            }
            /*
            if(chatData[6]=='/')    {
                //get username from chatData
                ptr = strtok(NULL, delim);
                printf("Will be sent : %s\n", ptr); //보낼사람
                
                write(chatData[7]-'0', chatData, n);
                //write(chatData[7]-'0', ~
                printf("from [%d] to [%d]i: %s\n", c_socket, chatData[7]-'0', chatData);
            }
			*/
		}
	}
}

int 
pushClient(int c_socket){
	int i;

	for(i = 0; i<MAX_CLIENT; i++){
		pthread_mutex_lock(&mutex);
        
		//if(list_c[i]==INVALID_SOCK){
        if(userList[i].clientsocket==INVALID_SOCK)  {
            //list_c[i] = c_socket;
			userList[i].clientsocket = c_socket;
            pthread_mutex_unlock(&mutex);
			return i;
		}
		pthread_mutex_unlock(&mutex);
	}


	if(i==MAX_CLIENT)
		return -1;
}

int
popClient(int s)
{
	int i;

	close(s);

	for(i=0; i<MAX_CLIENT; i++){
		pthread_mutex_lock(&mutex);
		//if(s==list_c[i]){
        if(s==userList[i].clientsocket) {
			//list_c[i] = INVALID_SOCK;
			userList[i].clientsocket = INVALID_SOCK;
            pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
	}

	return 0;
}
		
