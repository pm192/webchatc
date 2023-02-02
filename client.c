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
#include <netdb.h>
#include <arpa/inet.h>

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

void stopClient(connection_info * connection);
void showOptions(int x);
void connectToServer (connection_info *connection,char *address,char *port);
void handleUserInput(connection_info *connection);
void handleServerMessage(connection_info *connection);
void registerUser(connection_info *connection,char input[256]);

client thisClient;
int main(int argc , char *argv[]){
	connection_info connection;
    fd_set file_descriptors;
	int sockfd , port , n;
	message msg;
	char buf[256];
	if(argc<3){
		fprintf(stderr,"Usage: %s hostname port\n",argv[0]);
		exit(1);
	}
	connectToServer (&connection,argv[1],argv[2]);
	puts("You are now connected to the server.");
	showOptions(2);
	while(true){
		FD_ZERO (&file_descriptors);
        FD_SET (STDIN_FILENO, &file_descriptors);
        FD_SET (connection.socket, &file_descriptors);
        fflush (stdin);

        if (select (connection.socket + 1, &file_descriptors, NULL, NULL, NULL) < 0){
            perror ("Select failed.");
            exit (1);
        }
        if (FD_ISSET (STDIN_FILENO, &file_descriptors)){
            handleUserInput (&connection);
        }
        if (FD_ISSET (connection.socket, &file_descriptors)) {
            handleServerMessage(&connection);
        }

	}
	close(sockfd);

	return 0;
}
void showOptions(int x){
	if(x==2){
		puts("Type /help to view more options.");
		return;
	}
	if(x==1){
		puts("Type /register <username> <password> to register.");
		return;
	}
	puts("/creategroup <groupname> to create a group.");
	puts("/deletegroup <groupname> to delete a group.");
	puts("/join <groupname> to join a group.");
	puts("/msg <message> to send a message to every group you are a member of.");
	puts("/smsg <groupname> <message> to send a message to a specific group (<groupname>).");
	puts("/add <username> to add a user to your friend list.");
	puts("/remove <username> to remove a user from your friend list.");
	puts("/pm <username> <message> to send a private message to a user on your friend list.");
	puts("/showFriends | /showfriends | /show friends to view your current friends.");
	puts("/help to view more options.");
}
void connectToServer(connection_info *connection,char *address,char *port){
	char buf[256];
	while (true){
		char *username;
		char *password;
		showOptions(1);
		fflush (stdout);
        memset (buf, 0, 256);
        fgets (buf, 256, stdin); //get input
        trim_newline (buf); //remove \n
        if(strstr(buf, "/register")){
        	username = strtok(buf + 9 , " ");
        	password = strtok(NULL , "");
        	strcpy(thisClient.username , username);
        	strcpy(thisClient.password , password);
        }
		else{
			continue;
		}
        if ((connection->socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){ //create socket
            perror ("Could not create socket");
        }
        connection->address.sin_addr.s_addr = inet_addr (address);
        connection->address.sin_family = AF_INET;
        connection->address.sin_port = htons (atoi (port));
        if (connect (connection->socket, (struct sockaddr *) &connection->address, sizeof (connection->address)) < 0){ //connect to server
            perror ("Connect failed.");
            exit (1);
        }
        thisClient.socket = connection->socket ;
        message msgr;
    	msgr.type = REGISTER;
    	strcpy (msgr.user.username, thisClient.username);
    	if (send (connection->socket, (void *) &msgr, sizeof (message), 0) < 0){
          	perror ("Send failed");
          	exit (1);
      	}
        message msg;
        ssize_t recv_val = recv (connection->socket, &msg, sizeof (message), 0);
        if (recv_val < 0){
            perror ("recv failed");
            exit (1);
        }
        else if (recv_val == 0){
            close (connection->socket);
            printf ("The username \"%s\" is taken, please try another name.\n", thisClient.username);
            continue;
        }
        break;
    }
}
void stopClient(connection_info * connection){
    close (connection->socket);
    exit (0);
}
void handleUserInput(connection_info *connection){
	char input[256];
	bzero(input , 256);
    fgets(input , 256 , stdin);
    trim_newline(input);
	if (strcmp (input, "/q\n") == 0 || strcmp (input, "/quit\n") == 0){
        stopClient(connection);
        exit(1);
    }
    if(strcmp(input,"/help")==0){
    	showOptions(0);
    }
    else if(strstr(input, "/creategroup")){
    	message msg;
    	msg.type = CREATE_GROUP;
    	strcpy(msg.user.username,thisClient.username);
    	if(input[13]=='\0'){
    		puts("Field <groupname> undefined.Please use the format: /creategroup <groupname>");
    		return;
    	}
    	char *groupname = strtok(input + 12 ," ");
    	strcpy(msg.data,groupname);
    	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to create group");
          	exit (1);
    	}
    	return;
    }
    else if(strstr(input, "/deletegroup")){
    	if(input[13]=='\0'){
    		puts("Field <groupname> undefined.Please use the format: /deletegroup <groupname>");
    		return;
    	}
    	message msg;
    	msg.type = DELETE_GROUP;
    	char *groupname = strtok(input + 12 ," " );
    	strcpy(msg.data,groupname);
    	strcpy(msg.user.username, thisClient.username);
    	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to create group");
          	exit (1);
    	}
    	return;
    }
    else if(strstr(input, "/join")){
    	if(input[6]=='\0'){
    		puts("Field <groupname> undefined.Please use the format: /join <groupname>");
    		return;
    	}
    	message msg;
    	msg.type = JOIN_GROUP;
    	char *groupname = strtok(input + 6 ," " );
    	strcpy(msg.data,groupname);
    	strcpy(msg.user.username, thisClient.username);
    	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to join group");
          	exit (1);
    	}
    }
    else if(strstr(input , "/msg")){
    	message msg;
    	msg.type = SEND_GROUP_MESSAGE_ALL;
    	if(input[4]=='\0'){
    		puts("Field <message> undefined.Please use the format: /msg <message>");
    		return;
    	}
    	char *ms = strtok(input+5 , "");
    	strcpy(msg.data , ms);
    	strcpy(msg.user.username , thisClient.username);
    	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to send group message");
          	exit (1);
    	}
    }
    else if(strstr(input , "/smsg")){
    	char *dest = strtok(input + 5 , " ");
    	int x= 6+strlen(dest);
    	char *mes  = strtok(NULL , "");
    	if(strlen(dest)==0){
    		printf("<groupname> undefined.Please use the format: /smsg <groupname> <message>\n");
    		return;
    	}
    	else if(strlen(mes)==0){
    		printf("<message> undefined.Please use the format: /smsg <groupname> <message>\n");
    		return;
    	}
    	message msg;
    	msg.type = SEND_GROUP_MESSAGE_SPEC;
    	strcpy(msg.groupDest , dest);
    	strcpy(msg.data , mes);
    	strcpy(msg.user.username , thisClient.username);
    	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to send group message");
          	exit (1);
    	}
    }
    else if(strstr(input, "/add")){
    	char *u = strtok(input + 5 , "");
    	if(strlen(u)==0){
    		printf("<username> undefined.Please use the format: /add <username>\n");
    		return;
    	}
    	message msg;
    	msg.type = ADD_CONTACT;
     	strcpy(msg.data , u );
     	strcpy(msg.user.username , thisClient.username);
     	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to add contact");
          	exit (1);
    	}
    }
    else if(strstr(input, "/remove")){
    	char *u = strtok(input + 8 , "");
    	if(strlen(u)==0){
    		printf("<username> undefined.Please use the format: /add <username>\n");
    		return;
    	}
    	message msg;
    	msg.type = DELETE_CONTACT ;
     	strcpy(msg.data , u );
     	strcpy(msg.user.username , thisClient.username);
     	if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror ("ERROR: Failed to delete contact");
          	exit (1);
    	}
    }
    else if(strstr(input, "/showFriends")||strstr(input, "/showfriends")||strstr(input,"/show friends")){
    	message msg;
    	msg.type = SHOW_FRIENDS;
		strcpy(msg.user.username, thisClient.username);
		if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror("ERROR: Failed to show friends");
          	exit (1);
    	}
    }
	else if(strstr(input , "/pm ")){
		message msg;
		msg.type = PRIVATE_MESSAGE;
		char *target = strtok(input + 4 , " ");
		char *mes = strtok(NULL , "");
		strcpy(msg.groupDest , target);
		strcpy(msg.data , mes);
		if(send(connection->socket,(void *)&msg,sizeof(message),0)<0){
          	perror("ERROR: Failed to show friends");
          	exit (1);
    	}
	}
}
void handleServerMessage(connection_info *connection){
	message msg;
    ssize_t recv_val = recv (connection->socket, &msg, sizeof (message), 0);
    if (recv_val < 0){
        perror ("recv failed");
        exit (1);
    }
    else if(recv_val == 0){
        close (connection->socket);
        puts ("Server disconnected.");
        exit (0);
    }
   	if(msg.type == REGISTER_SUCCESS)
   		printf("Register successfull\n");
   	else if(msg.type == REGISTER_ERROR)
   		printf("The username \"%s\" is not available.\n",msg.user.username);
   	else if(msg.type == CREATE_GROUP_SUCCESS)
   		printf("The group \"%s\" was created successfully.\n",msg.data);
   	else if(msg.type == CREATE_GROUP_ERROR)
   		printf("The group \"%s\" was not created.\n",msg.data);
   	else if(msg.type == DELETE_GROUP_SUCCESS)
   		printf("The group \"%s\" was deleted successfully.\n",msg.data);
   	else if(msg.type == DELETE_GROUP_ERROR)
   		printf("The group \"%s\" either does not exist , or you are not the admin.\n",msg.data);
   	else if(msg.type == JOIN_GROUP_SUCCESS)
   		printf("You have joined the group \"%s\".\n",msg.data);
   	else if(msg.type == JOIN_GROUP_ERROR)
   		printf("Error joining the group \"%s\".\n",msg.data);
   	else if(msg.type == SEND_GROUP_MESSAGE)
   		printf("[%s] - %s > %s\n",msg.groupDest,msg.user.username,msg.data);
   	else if(msg.type == ADD_CONTACT_SUCCESS)
   		printf("You are now friends with \"%s\".\n",msg.data);
   	else if(msg.type == ADD_CONTACT_ERROR)
   		printf("Failed to add \"%s\" to your friends.\n",msg.data);
	else if(msg.type == DELETE_CONTACT_SUCCESS)
		printf("You are no longer friends with %s\n",msg.data);
	else if(msg.type == DELETE_CONTACT_ERROR)
		printf("Failed to remove %s from your friends\n",msg.data);
	else if(msg.type == SHOW_FRIENDS)
		printf("Your friends are: \n%s",msg.data);
	else if(msg.type == PRIVATE_MESSAGE)
		printf("PM From %s : %s\n",msg.user.username , msg.data);
	else if(msg.type == PM_ERROR)
		printf("Failed to send PM to %s\n",msg.groupDest);
}
