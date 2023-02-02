#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define MAX_GROUPS 10
#define MAX_CONTACTS 10

void trim_newline (char *text){
    int len = strlen (text) - 1;
    if (text[len] == '\n')
      	text[len] = '\0';
}

typedef enum{
   REGISTER,
   LOGIN,
   CREATE_GROUP,
   DELETE_GROUP,
   JOIN_GROUP,
   SEND_GROUP_MESSAGE,
   SEND_GROUP_MESSAGE_ALL,
   SEND_GROUP_MESSAGE_SPEC,
   ADD_CONTACT,
   DELETE_CONTACT,
   PRIVATE_MESSAGE,
   REGISTER_ERROR,
   LOGIN_ERROR,
   CREATE_GROUP_ERROR,
   DELETE_GROUP_ERROR,
   JOIN_GROUP_ERROR,
   ADD_CONTACT_ERROR,
   DELETE_CONTACT_ERROR,
   REGISTER_SUCCESS,
   LOGIN_SUCCESS,
   CREATE_GROUP_SUCCESS,
   DELETE_GROUP_SUCCESS,
   JOIN_GROUP_SUCCESS,
   ADD_CONTACT_SUCCESS,
   DELETE_CONTACT_SUCCESS,
   PM_ERROR,
   SHOW_FRIENDS,
   SERVER_FULL,
   GROUPS_FULL,
   CONTACTS_FULL,

}message_type;

typedef struct{
	int socket;
    struct sockaddr_in address;
	char username[20];
	char password[20];
	char contacts[MAX_CONTACTS][20];

}client;

typedef struct{
	char groupname[20];
	char activeClients[MAX_CLIENTS][20];
	char admin[20];
}group;

typedef struct connection_info{
    int socket;
    struct sockaddr_in address;
} connection_info;

typedef struct{
    message_type type;
    client user;
    char data[256];
    char groupDest[256];
}message;

int constructFdSet (fd_set * set, connection_info * server_info,client clients[]);
void handleClientMessage (int sender);
void stopServer (client connection[]);
void handleUserInput ();
void sendFullMessage (int socket);
void handleNewConnection (connection_info * server_info);
void initializeServer (connection_info *server_info,int port);
void sendRegSuccess (int sender);
void error(const char *msg);
void sendGroupResultMsg (int sender , char groupname[20],int c);
void printGroups ();
void sendGroupMessageAll (char *username , char *mes);
void sendGroupMessageSpec (char *username , char *target , char *mes);
void sendRegSuccess (int sender);
int groupExists (char *groupname);
int alreadyJoined (char *username , char *groupname);
void sendFriendResultMsg (char *username , char *tar,int x,int sender);
int ifUserInGroup (int j, char *username);
int addFriend (int sender , char *username);
int deleteFriend (int sender , char *username);
void sendPrivateMessage (int sender , char *target , char *mes);

client clients[MAX_CLIENTS];
group groups[MAX_GROUPS];

int main(int argc , char *argv[]){
	printf("***Starting Server***\n");
	if(argc<2){
		fprintf(stderr,"ERROR: Provide a port number\n");
		exit(1);
	}
	fd_set file_descriptors;
    connection_info server_info;
    int i;
    for (i=0;i<MAX_CLIENTS;i++){
        clients[i].socket = 0;
    }
    initializeServer(&server_info,atoi(argv[1]));
    while (true){
        int max_fd = constructFdSet (&file_descriptors, &server_info, clients);
        if(select(max_fd + 1, &file_descriptors, NULL, NULL, NULL)<0){
          	perror ("Select Failed");
          	stopServer(clients);
        }

        if (FD_ISSET(STDIN_FILENO,&file_descriptors)){
            handleUserInput();
        }

       	if (FD_ISSET(server_info.socket,&file_descriptors)){
        	handleNewConnection(&server_info);
        }
        for (i = 0; i < MAX_CLIENTS; i++) {
       	    if (clients[i].socket > 0 && FD_ISSET (clients[i].socket, &file_descriptors)){
                handleClientMessage(i);
            }
        }
    }
	return 0;
}

void initializeServer(connection_info *server_info,int port){
    if ((server_info->socket = socket (AF_INET, SOCK_STREAM, 0)) < 0){
        perror ("Failed to create socket");
        exit (1);
    }
    server_info->address.sin_family = AF_INET;
    server_info->address.sin_addr.s_addr = INADDR_ANY;
    server_info->address.sin_port = htons (port);
    if (bind (server_info->socket, (struct sockaddr *) &server_info->address, sizeof (server_info->address)) < 0){
          perror ("Binding failed");
          exit (1);
    }
    const int optVal = 1;
    const socklen_t optLen = sizeof (optVal);
    if (setsockopt (server_info->socket,SOL_SOCKET,SO_REUSEADDR,(void *)&optVal,optLen)<0){
          perror ("Set socket option failed");
          exit (1);
      }

    if (listen (server_info->socket, 3) < 0){
          perror ("Listen failed");
          exit (1);
      }
    printf ("Waiting for users...\n");
}

void error(const char *msg){
	perror(msg);
	exit(0);
}
void handleNewConnection (connection_info * server_info){
    int new_socket;
    int address_len;
    new_socket = accept(server_info->socket,(struct sockaddr *)&server_info->address, (socklen_t *) & address_len);
    if (new_socket < 0){
        perror ("Accept Failed");
        exit (1);
    }
    int i;
    for(i=0;i<MAX_CLIENTS;i++){
        if (clients[i].socket == 0){
            clients[i].socket = new_socket;
            break;
        }
        else if (i==MAX_CLIENTS-1){
            sendFullMessage(new_socket);
        }
    }
}
void handleUserInput(){
    char input[255];
    fgets (input, sizeof (input), stdin);
    trim_newline (input);
    if(strstr(input , "/q")||strstr(input , "/quit")){
    	stopServer(clients);
    }
    else if(strstr(input , "/groups")){
    	printGroups();
    }

}
void stopServer (client connection[]){
    int i;
    for (i = 0; i < MAX_CLIENTS; i++){
        close (connection[i].socket);
    }
    exit (0);
}
void sendRegSuccess (int sender){
    message msg;
    strncpy (msg.user.username, clients[sender].username, 21);
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++){
        if (clients[i].socket != 0){
          if (i == sender){
            msg.type = REGISTER_SUCCESS;
            if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0){
              	perror ("Send failed");
              	exit (1);
            }
          }
        }
    }
}
int groupExists(char *groupname){
	int i;
	for(i=0;i<MAX_GROUPS;i++){
		if(strcmp(groups[i].groupname , groupname )==0){
			return 1;
		}
	}
	return 0;
}
int alreadyJoined(char *username , char *groupname){
	int i,j;
	for(i=0;i<MAX_GROUPS;i++){
		for(j=0;j<MAX_CLIENTS;j++){
			if(strcmp(groups[i].activeClients[j] , username)==0 &&strcmp(groups[i].groupname , groupname)==0){
				return 1;
			}
		}

	}
	return 0;
}
int alreadyFriends(char *username , char *tar){
	int i,j;
	for(i=0;i<MAX_CLIENTS;i++){
		for(j=0;j<MAX_CONTACTS;j++){
			if(strcmp(clients[i].contacts[j] , tar)==0&&strcmp(clients[i].username , username)==0){
				return 1;
			}
		}
	}
	return 0;
}
int notFriends(char *username , char *tar){
	int i,j;
	for(i=0;i<MAX_CLIENTS;i++){
		for(j=0;j<MAX_CONTACTS;j++){
			if(strcmp(clients[i].contacts[j] , tar)==0&&strcmp(clients[i].username , username)==0){
				return 0;
			}
		}
	}
	return 1;
}

void handleClientMessage (int sender){
    int read_size;
    message msg;
    if((read_size = recv (clients[sender].socket, &msg, sizeof (message), 0)) == 0){
        printf ("User disconnected: %s.\n",clients[sender].username);
        close (clients[sender].socket);
        clients[sender].socket = 0;
	}
    else{
    	if(msg.type==REGISTER){
    		int i;
                for (i = 0; i < MAX_CLIENTS; i++)
                  {
                      if (clients[i].socket != 0 && strcmp (clients[i].username, msg.user.username) == 0)
                        {
                            close (clients[sender].socket);
                            clients[sender].socket = 0;
                            return;
                        }
                  }

                strcpy (clients[sender].username, msg.user.username);
                printf ("User connected: %s\n", clients[sender].username);
                sendRegSuccess (sender);
    	}
    	else if(msg.type==CREATE_GROUP){
    		int i=0;
    		if(groupExists(msg.data)==1){
    			sendGroupResultMsg(sender,msg.data,2);
    			return;
    		}
            for(i=0;i < MAX_GROUPS; i++){
            	if(strlen(groups[i].groupname)==0){
            		strcpy(groups[i].groupname,msg.data);
            		strcpy(groups[i].admin,msg.user.username);
            		strcpy(groups[i].activeClients[0] , clients[sender].username);
            		printf("User %s created a group called %s \n",groups[i].admin,groups[i].groupname);
            		sendGroupResultMsg(sender,msg.data,1);
            		return;
            	}

            }
            sendGroupResultMsg(sender,msg.data,2);
        }
        else if(msg.type==DELETE_GROUP){
        	int i=0;
        	for(i=0;i<MAX_GROUPS;i++){
        		if(strcmp(groups[i].groupname,msg.data)==0&&strcmp(groups[i].admin,msg.user.username)==0){
        			memset(&groups[i].groupname , 0 , sizeof(groups[i].groupname));
        			memset(&groups[i].admin , 0 , sizeof(groups[i].admin));
        			int k=0;
        			for(k=0;k<MAX_CLIENTS;k++){
        				memset(&groups[i].activeClients[k] , 0 , sizeof(groups[i].activeClients[k]));
        			}
        			sendGroupResultMsg(sender,msg.data,3);
        			return;
        		}
        	}
        	sendGroupResultMsg(sender,msg.data,4);
        }
        else if(msg.type==JOIN_GROUP){
        	int i=0;
        	if(alreadyJoined(msg.user.username , msg.data)==1){
				sendGroupResultMsg(sender,msg.data,6);
				return;
			}
        	for(i=0;i<MAX_GROUPS;i++){
        		if(strcmp(groups[i].groupname,msg.data)==0){
        			int j=0;
        			for(j=0;j<MAX_CLIENTS;j++){
        				if(strlen(groups[i].activeClients[j])==0){
        					strcpy(groups[i].activeClients[j],msg.user.username);
        					sendGroupResultMsg(sender,msg.data,5);
        					return;
        				}
        			}
        		}
        	}
        	sendGroupResultMsg(sender,msg.data,6);
        }
        else if(msg.type==SEND_GROUP_MESSAGE_ALL){
        	printf("[ALL] User \"%s\"  : %s \n",msg.user.username , msg.data);
        	sendGroupMessageAll(msg.user.username , msg.data);
        }
        else if(msg.type==SEND_GROUP_MESSAGE_SPEC){
        	printf("[%s] User \"%s\"  : %s \n",msg.groupDest,msg.user.username , msg.data);
        	sendGroupMessageSpec(msg.user.username , msg.groupDest , msg.data);
        }
        else if(msg.type==ADD_CONTACT){
        	if(alreadyFriends(msg.user.username , msg.data)==1){
        		sendFriendResultMsg(msg.user.username , msg.data , 2,sender);
        		return;
        	}
        	int i=0;
        	for(i=0;i<MAX_CONTACTS;i++){
        		if(strlen(clients[sender].contacts[i])==0){
        			strcpy(clients[sender].contacts[i] , msg.data);
        			printf("User \"%s\" is now friends with \"%s\" .\n",clients[sender].username,clients[sender].contacts[i]);
        			if(addFriend(sender , msg.data)==0){
						sendFriendResultMsg(msg.user.username , msg.data , 1,sender);
					}
					else{
						sendFriendResultMsg(msg.user.username , msg.data ,2,sender);
						memset(clients[sender].contacts[i] , 0 , sizeof(clients[sender].contacts[i]));
					}
        			return;
        		}
        	}
        	sendFriendResultMsg(msg.user.username , msg.data ,2,sender);
        }
        else if(msg.type==DELETE_CONTACT){
        	if(notFriends(msg.user.username , msg.data)==1){
        		sendFriendResultMsg(msg.user.username , msg.data , 4,sender);
        		return;
        	}
        	int i;
        	for(i=0;i<MAX_CONTACTS;i++){
        		if(strcmp(clients[sender].contacts[i],msg.data)==0){
        			memset(clients[sender].contacts[i] , 0 ,sizeof(clients[sender].contacts[i]) );
					if(deleteFriend(sender , msg.data)==0){
						memset(clients[sender].contacts[i] , 0 ,sizeof(clients[sender].contacts[i]) );
						sendFriendResultMsg(msg.user.username , msg.data , 3,sender);
						printf("User \"%s\" unfriended \"%s\" .\n",clients[sender].username,msg.data);
						return;
					}
					else{
						strcpy(clients[sender].contacts[i] , msg.data);
						sendFriendResultMsg(msg.user.username , msg.data , 4,sender);
					}
        			return;
        		}
        	}
        	sendFriendResultMsg(msg.user.username , msg.data , 4,sender);
        }
        else if(msg.type==SHOW_FRIENDS){
        	int i;
        	char friends[256];
			strcpy(friends , "");
			for(i=0;i<MAX_CONTACTS;i++){
				if(strlen(clients[sender].contacts[i])>0){
					strcat(friends , clients[sender].contacts[i]);
					strcat(friends , "\n");
				}
			}
			message msg;
			msg.type = SHOW_FRIENDS ;
			strcpy(msg.data , friends);
			if (send (clients[sender].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	exit (1);
            }
        }
		else if(msg.type == PRIVATE_MESSAGE){
			sendPrivateMessage(sender , msg.groupDest , msg.data);
		}
    }
}
void sendPrivateMessage(int sender , char *target , char *mes){
	int i,j,k;
	message msg;
	msg.type = PRIVATE_MESSAGE;
	for(i=0;i<MAX_CONTACTS;i++){
		if(strcmp(clients[sender].contacts[i] , target)==0){
			strcpy(msg.data , mes);
			strcpy(msg.user.username , clients[sender].username);
			//find the targets's socket
			for(j=0;j<MAX_CLIENTS;j++){
				if(strcmp(clients[sender].contacts[i] , clients[j].username)==0){
					k=j;
					break;
				}
			}
			strcpy(msg.groupDest , target);
			if (send (clients[k].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	exit (1);
            }
			printf("\"%s\" sent a PM to \"%s\" : %s .\n",clients[sender].username , target , msg.data);
			return;
		}
	}
	strcpy(msg.groupDest , target);
	msg.type = PM_ERROR;
	if (send (clients[sender].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	exit (1);
            }
}
int deleteFriend(int sender , char *username){
	int i,j,k;

	for(i=0;i<MAX_CLIENTS;i++){
		if(strcmp(clients[i].username , username)==0){
			for(j=0;j<MAX_CONTACTS;j++){
				if(strcmp(clients[i].contacts[j] , clients[sender].username )==0){
					memset(clients[i].contacts[j] , 0 , sizeof(clients[i].contacts[j]));
					sendFriendResultMsg(clients[i].username , clients[sender].username , 3 , i);
					return 0;
				}
			}
		}
	}
	return 1;
}
int addFriend(int sender , char *username){
	int i,j,k;

	for(i=0;i<MAX_CLIENTS;i++){
		if(strcmp(clients[i].username , username)==0&&strlen(clients[i].username)>0){
			k=i;
			for(j=0;j<MAX_CLIENTS;j++){
				if(strlen(clients[i].contacts[j])==0){
					strcpy(clients[i].contacts[j], clients[sender].username);
					sendFriendResultMsg(clients[i].username , clients[sender].username ,1,i);
					return 0;
				}
			}
		}
	}
	return 1;
}
int ifUserInGroup(int j, char *username){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(strcmp(groups[j].activeClients[i] , username)==0){
			return 0;
		}
	}
	return 1;
}
void sendGroupMessageAll(char *username , char *mes){
	int i,j,k,l;
	//for every group , if this user is in the group then send the message to all of its members
	for(i=0;i<MAX_GROUPS;i++){
		if(ifUserInGroup(i,username)==0){
			sendGroupMessageSpec(username , groups[i].groupname , mes);
		}
	}
}
void sendGroupMessageSpec(char *username , char *target , char *mes){
	int i,j,k,ok=0;
	//check if username is in the target group
	for(i=0;i<MAX_GROUPS;i++){
		if(strcmp(groups[i].groupname , target )==0 && strlen(groups[i].groupname)>0){ //group exists
			k=i;
			for(j=0;j<MAX_CLIENTS;j++){
				if(strcmp(groups[i].activeClients[j] , username )==0 &&strlen(groups[i].activeClients[j])>0){ //user is in group
					ok=1;
					i=MAX_GROUPS+1;
					break;
				}
			}
		}
	}
	if(ok==0){

		return;
	}
	//send the message to the rest members of the group
	int p;
	message msg;
	msg.type = SEND_GROUP_MESSAGE;
	strcpy(msg.data , mes);
	strcpy(msg.groupDest , target );
	strcpy(msg.user.username , username);
	for(i=0;i<MAX_CLIENTS;i++){
		for(j=0;j<MAX_CLIENTS;j++){
			if(strcmp(groups[k].activeClients[i] , clients[j].username)==0 && strlen(groups[k].activeClients[i])>0 && strcmp(groups[k].activeClients[i] , username )!=0){

			if (send (clients[j].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	 exit (1);
            }

			}
		}
	}

}
int constructFdSet (fd_set * set, connection_info * server_info, client clients[]){
    FD_ZERO (set);
    FD_SET (STDIN_FILENO, set);
    FD_SET (server_info->socket, set);
    int max_fd = server_info->socket;
    int i;
    for (i = 0; i < MAX_CLIENTS; i++){
        if (clients[i].socket > 0){
            FD_SET (clients[i].socket, set);
            if (clients[i].socket > max_fd){
                max_fd = clients[i].socket;
            }
        }
    }
    return max_fd;
}
void sendFullMessage (int socket){
    message too_full_message;
    too_full_message.type = SERVER_FULL;
    if (send (socket, &too_full_message, sizeof (too_full_message), 0) < 0){
        perror ("Send failed");
        exit (1);
    }
    close (socket);
}
void sendGroupResultMsg(int sender ,char groupname[20],int c){
	message msg;

	if(c==1){
		msg.type = CREATE_GROUP_SUCCESS;
	}
	else if(c==2){
		msg.type = CREATE_GROUP_ERROR;
	}
	else if(c==3){
		msg.type = DELETE_GROUP_SUCCESS;
	}
	else if(c==4){
		msg.type = DELETE_GROUP_ERROR;
	}
	else if(c==5){
		msg.type = JOIN_GROUP_SUCCESS;
	}
	else if(c==6){
		msg.type = JOIN_GROUP_ERROR;
	}

	strcpy(msg.data,groupname);
	strcpy (msg.user.username,clients[sender].username);
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++){
        if (clients[i].socket != 0){
          if (i == sender){
            if (send (clients[i].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	 exit (1);
            }
          }
        }
    }
}
void sendFriendResultMsg(char *username , char *tar,int x,int sender){
	message msg;

	if(x==1){
		msg.type = ADD_CONTACT_SUCCESS;
	}
	else if(x==2){
		msg.type = ADD_CONTACT_ERROR;
	}
	else if(x==3){
		msg.type = DELETE_CONTACT_SUCCESS;
	}
	else if(x==4){
		msg.type = DELETE_CONTACT_ERROR;
	}
	strcpy (msg.user.username,username);
	strcpy(msg.data , tar);
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++){
        if (clients[i].socket != 0){
          if (i == sender){
            if (send (clients[i].socket, &msg, sizeof (message), 0) < 0){
              	perror ("Send failed");
             	 exit (1);
            }
          }
        }
    }

}
void printGroups(){
	int i=0;
	for(i=0;i<MAX_GROUPS;i++){
		if(strlen(groups[i].groupname)==0){
			continue;
		}
		printf("Group %d : %s , admin: %s ,active clients: \n",i,groups[i].groupname,groups[i].admin);
		int k=0;
		for(k=0;k<MAX_CLIENTS;k++){
			if(strlen(groups[i].activeClients[k])>0){
				printf("%s\n",groups[i].activeClients[k]);
			}
		}
	}
}
