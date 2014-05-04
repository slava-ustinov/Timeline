#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#define SERVER_PORT 		12345		/* arbitrary, but client and server must agree */
#define BUF_SIZE 		8192			/* block transfer size */
#define QUEUE_SIZE 		10
#define MAX_USERS 		100
#define MAX_FILES_PER_USER	100
#define MAX_USERNAME_LEN	20
#define MAX_PASSWORD_LEN	20
#define MAX_FILENAME_LEN	60
#define MAX_MESSAGE_SIZE	8192
#define DB_FILENAME 		"p2pServerDb.txt"

enum ServerErrors_E
{
    ERR_NONE = 0,
    ERR_USERNAME_INVALID,
    ERR_PASSWORD_INVALID,
    ERR_FORMAT_INVALID,
    ERR_ALREADY_LOGGED_IN

};



/* Type Definitions */

typedef char	fileName_T[MAX_FILENAME_LEN];
typedef char	ipAddress_T[16];
typedef char	port_T[6];
typedef char	usernameOrPass[MAX_USERNAME_LEN];



typedef struct
{
	int 		isOnline;
	ipAddress_T 	ipAddress;
	port_T		postPort;
	//port_T		chatPort;
	//port_T		webPort;

} ServerDatabaseEntry_T;


/* Static Variables */

static usernameOrPass RegisteredUsers[MAX_USERS]=
{
	{"306111111"},
	{"306222222"},
	{"306333333"},
	{"306444444"},
	{"21527015"},
	{"21912472"},
	{"32516197"},
	{"37980364"},
	{"60120276"},
	{"66474941"},
	{"200346369"},
	{"200718799"},
	{"203778063"},
	{"204778799"},
	{"300054897"},
	{"300104965"},
	{"300128550"},
	{"300341807"},
	{"300764446"},
	{"300940103"},
	{"301112314"},
	{"309167567"},
	{"309472900"},
	{"317411775"},
	{"21542840"},
	{"21595061"},
	{"32506420"},
	{"32527053"},
	{"37096153"},
	{"39711924"},
	{"66016288"},
	{"66462011"},
	{"300074440"},
	{"300480738"},
	{"300531647"},
	{"300683133"},
	{"300841103"},
	{"300850062"},
	{"301048351"},
	{"301267605"},
	{"301354718"},
	{"304545247"},
	{"306569872"},
	{"307563098"},
	{"311633465"},
	{"32491821"},
	{"36951499"},
	{"37967221"},
	{"39335708"},
	{"39899299"},
	{"300012804"},
	{"300021136"},
	{"300323524"},
	{"300531209"},
	{"300908795"},
	{"301224283"},
	{"305893091"},
	{"305978124"},
	{"308877612"},
	{"309930006"}
};

static ServerDatabaseEntry_T	ServerDatabase[MAX_USERS];
static myClientIndex; /* each child process will use it */
static char buf[BUF_SIZE];
static char message[MAX_MESSAGE_SIZE];
static int sa;



/* Static Function Declarations */

static void F_InitDatabase();
static int F_ValidateUser ( char *username);
static int F_SearchUser ( char *username, int *index );
//static int F_HandleLoginRequest(char *testUsername, char *testPicPort, char *testChatPort, char *testWebPort);
static int F_HandleLoginRequest(char *testUsername, char *testPostPort);
static int F_ChildProcess(int sa);
static int F_SendMessage(int messageLen);
static void F_SaveDatabase();
static void F_LoadDatabase();
static void F_UpdateOnlineStatus(int userIndex, int status);
static void F_HandleWhoCommand();
static void F_HandleFileSearch(char *token);


static int F_SendMessage(int messageLen)
{
	int ret = ERR_NONE;
	
	write(sa, message, strlen(message));
	
	memset(message,'\0',MAX_MESSAGE_SIZE);
	
	return ret;
}/* F_SendMessage */



/* Static Function Definitions */

static void F_InitDatabase()
{
	int i;
	FILE *fd;

	for ( i = 0; i < MAX_USERS; i++ )
	{
		ServerDatabase[i].isOnline = 0;
	}
	
	system("rm -f p2pServerDb.txt");
	

	fd = fopen(DB_FILENAME, "w");
	if ( fd == NULL ) fatal ( "open db file failed" );
	
	fwrite ( ServerDatabase, 1, sizeof(ServerDatabase),fd );
	fflush(fd);
	fclose(fd);


}/* F_InitDatabase */


static int F_ValidateUser ( char *username)
{
	int ret = ERR_NONE;
	int userIndex = 0;

	ret= F_SearchUser ( username, &userIndex );
	
	if ( ret == ERR_NONE )
	{
		myClientIndex = userIndex;
	}
	
	return ret;


}/* F_ValidateUser */


static int F_SearchUser ( char *username, int *index )
{
	int ret = ERR_NONE;
	int i;

	for ( i = 0; i < MAX_USERS; i++ )
	{
		if ( 0 == strcmp ( RegisteredUsers[i], username ) )
		{
			*index = i;
			break;
		}
	}
	
	if ( i >= MAX_USERS )
	{
		ret = ERR_USERNAME_INVALID;
	}
	else if(ServerDatabase[i].isOnline == 1)
	{
		ret = ERR_ALREADY_LOGGED_IN;
	}

	return ret;
}/* F_SearchUser */



//static int F_HandleLoginRequest(char *testUsername, char *testPicPort, char *testChatPort, char *testWebPort)
static int F_HandleLoginRequest(char *testUsername, char *testPostPort)
{
	int ret = ERR_NONE;
	char *pChar = NULL;
	int i = 0;
	int usernameLen=0;
	int picturePortLen=0;
	int chatPortLen=0;
	
	/* find username */
	pChar = &buf[4];
	while(pChar[i] != '#' && pChar[i] != '\n' && pChar[i] != '\0')
	{
		i++;
	}
	if(pChar[i] == '#')
	{
		memcpy(testUsername, pChar, i);
		testUsername[i] = '\0';
		printf("username found: %s\n", testUsername);
	}
	else
	{
		printf("BAD Packet - can not find username\n");
		return ERR_FORMAT_INVALID;
	}
	
	/* find post port */
	usernameLen=i;
	pChar = &buf[4 + usernameLen + 1];
	i=0;
	while(pChar[i] != '#' && pChar[i] != '\n' && pChar[i] != '\0')
	{
		i++;
	}
	if(pChar[i] == '\n')
	{
		memcpy(testPostPort, pChar, i);
		testPostPort[i] = '\0';
		printf("post port found: %s\n", testPostPort);
	}
	else
	{
		printf("BAD Packet - can not find post port\n");
		return ERR_FORMAT_INVALID;
	}
	
	#if 0
	/* find chat port */
	picturePortLen=i;
	pChar = &buf[4 + usernameLen + picturePortLen + 2];
	i=0;
	while(pChar[i] != '#' && pChar[i] != '\n' && pChar[i] != '\0')
	{
		i++;
	}
	//if(pChar[i] == '#')
	if(pChar[i] == '\n')
	{
		memcpy(testChatPort, pChar, i);
		testChatPort[i] = '\0';
		printf("text port found: %s\n", testChatPort);
	}
	else
	{
		printf("BAD Packet - can not find chat port\n");
		return ERR_FORMAT_INVALID;
	}
	
	
	/* find web port */
	chatPortLen=i;
	pChar = &buf[4 + usernameLen + picturePortLen + chatPortLen+3];
	i=0;
	while(pChar[i] != '\n' && pChar[i] != '\0')
	{
		i++;
	}
	if(pChar[i] == '\n')
	{
		memcpy(testWebPort, pChar, i);
		testWebPort[i] = '\0';
		printf("web port found: %s\n", testWebPort);
	}
	else
	{
		printf("BAD Packet - can not find web port\n");
		return ERR_FORMAT_INVALID;
	}
	#endif
	ret = F_ValidateUser (testUsername);
	
	return ret;
	
}/* F_HandleLoginRequest */



#if 0
static int F_HandleShareRequest()
{
	int ret = ERR_NONE;
	char *pChar = NULL;
	int i = 0;
	int isMoreFiles = 1;
	
	ServerDatabase[myClientIndex].numOfSharedFiles = 0;
	
	
	/* find port */
	pChar = &buf[4];
	
	while(pChar[i] != '#' && pChar[i] != '\n' && pChar[i] != '\0' && i < 7)
	{
		i++;
	}
	if(pChar[i] == '#')
	{
		memcpy(ServerDatabase[myClientIndex].port, pChar, i);
		ServerDatabase[myClientIndex].port[i] = '\0';
		printf("user port found: %s\n", ServerDatabase[myClientIndex].port);
	}
	else if(pChar[i] == '\n')
	{
		isMoreFiles = 0; /* empty file list */
	}
	else
	{
		sprintf(message, "ERORShare message is malformed: can't find port number\n");
		F_SendMessage(70);
		printf("BAD Packet - can not find user port\n");
		return ERR_FORMAT_INVALID;
	}
	
	/* find files */
	//pChar = &buf[4 + i + 1];
	//i=0;
	
	while(isMoreFiles)
	{
		pChar += (i + 1);
		i=0;
		while(pChar[i] != '#' && pChar[i] != '\n' && pChar[i] != '\0' && i < 500)
		{
			i++;
		}
		if(pChar[i] == '#')
		{
			memcpy(ServerDatabase[myClientIndex].fileList[ServerDatabase[myClientIndex].numOfSharedFiles], pChar, i);
			ServerDatabase[myClientIndex].fileList[ServerDatabase[myClientIndex].numOfSharedFiles][i] = '\0';
			
			if(ServerDatabase[myClientIndex].numOfSharedFiles + 1 >= MAX_FILES_PER_USER)
			{
				isMoreFiles = 1;	
			}
			ServerDatabase[myClientIndex].numOfSharedFiles ++;
			
		}
		else if (pChar[i] == '\n')
		{
			memcpy(ServerDatabase[myClientIndex].fileList[ServerDatabase[myClientIndex].numOfSharedFiles], pChar, i);
			ServerDatabase[myClientIndex].fileList[ServerDatabase[myClientIndex].numOfSharedFiles][i] = '\0';
			
			if(ServerDatabase[myClientIndex].numOfSharedFiles + 1 >= MAX_FILES_PER_USER)
			{
				isMoreFiles = 0;	
			}
			ServerDatabase[myClientIndex].numOfSharedFiles ++;
	
			isMoreFiles = 0;
		}
		else
		{
			sprintf(message, "ERORShare message is malformed: can't find file number %d\n", ServerDatabase[myClientIndex].numOfSharedFiles);
			F_SendMessage(50);
			printf("BAD Packet - can not find user file\n");
			return ERR_FORMAT_INVALID;
		}
		
	}//while(isMoreFiles)
	
	printf("User %s has the following files:\n", RegisteredUsers[myClientIndex][0]);
	for(i = 0; i < ServerDatabase[myClientIndex].numOfSharedFiles; i++)
	{
		printf("%s\n", ServerDatabase[myClientIndex].fileList[i]);
	}
	
	
	return ret;
	
	
}/* F_HandleShareRequest */
#endif

static int F_ChildProcess(int sa)
{
	int ret = ERR_NONE;
	char testUsername[MAX_USERNAME_LEN] = "";
	//char testChatPort[MAX_USERNAME_LEN] = "";
	char testPostPort[MAX_USERNAME_LEN] = "";
	//char testWebPort[MAX_USERNAME_LEN] = "";
	
	char *tempStr1 = "-unknown-";
	//struct sockaddr tempAddress;
	int tempLen = 15;
	char *client_ip;
	char command[5] = "";
	char tempPort[6] = "";
	char tempIP[16] = "";
	int tempIndex;
	int isMyClientLogged = 0;
	int i;

	struct sockaddr_in tempAddress1;

	tempLen = sizeof(tempAddress1);

	memcpy(testUsername, tempStr1, 10);
	

	memset(&tempAddress1,0,sizeof(tempAddress1));

	getpeername ( sa, ( struct sockaddr * ) &tempAddress1, &tempLen );
	
	//printf("Integer IP: 0x%X\n", ntohl(*(int *)&tempAddress1.sin_addr));
	
	client_ip = inet_ntoa ( tempAddress1.sin_addr );
	
	memcpy(tempIP, client_ip/*"127.0.0.1"*/, 16);
	
	sprintf(tempPort, "%d", ntohs ( tempAddress1.sin_port ));
	printf ("timeline peer (IP %s, port %s) is connected\n", tempIP, tempPort);
			
	while(1)
	{
#if 0		
		/* read data from socket */
		if(0 >= read ( sa, buf, BUF_SIZE ))
		{
			
			//printf("User %s has quit unexpectedly\n", testUsername);
			printf("User %s has quit\n", testUsername);
			printf("connection with %s (IP %s, port %s) is closed\n",testUsername, tempIP, tempPort);
			F_UpdateOnlineStatus(myClientIndex, 0);
			exit(0);
		}
		
#endif	
		memset(buf,'\0',BUF_SIZE);
	
		/* read data from socket - byte by byte until '\0'*/

		i = 0;
		if(0 >= read(sa, &buf[i], 1))
		{
			printf("User %s has quit\n", testUsername);
			printf("connection with %s (IP %s, port %s) is closed\n",testUsername, tempIP, tempPort);
			F_UpdateOnlineStatus(myClientIndex, 0);
			exit(0);
		}
	
		while(buf[i]!='\n' && i < BUF_SIZE)
		{
			i++;
			if(0 >= read(sa, &buf[i], 1))
			{
				printf("User %s has quit\n", testUsername);
				printf("connection with %s (IP %s, port %s) is closed\n",testUsername, tempIP, tempPort);
				F_UpdateOnlineStatus(myClientIndex, 0);
				exit(0);
			}
		}
		//printf("buffer received %s", buf);
		
	
		
		
	
		/* LOCK SEMAPHORE!!!! */
		
		F_LoadDatabase();
			
		memcpy(command, buf, 4);
		command[4] = '\0';
				
		if(0 == strcmp(command, "LGIN") && !isMyClientLogged)
		{
			
			//printf("received command is: %s. Starting user validation...\n", command);
			//ret = F_HandleLoginRequest(testUsername, testPicPort, testChatPort, testWebPort);
			ret = F_HandleLoginRequest(testUsername, testPostPort);
			
			if(ERR_NONE == ret) /* login succeeded */
			{
				F_LoadDatabase();
				memcpy(ServerDatabase[myClientIndex].ipAddress, tempIP, 16);
				memcpy(ServerDatabase[myClientIndex].postPort, testPostPort, 6);
				//memcpy(ServerDatabase[myClientIndex].chatPort, testChatPort, 6);
				//memcpy(ServerDatabase[myClientIndex].webPort, testWebPort, 6);
				F_SaveDatabase();
				
				sprintf(message, "LRESWelcome %s!\n", testUsername);
				F_SendMessage(50);
				printf("User %s has logged in successfully\n", testUsername);
				F_UpdateOnlineStatus(myClientIndex, 1);
				isMyClientLogged = 1;
				
			}
			else /* login failed */
			{
				if(ERR_USERNAME_INVALID == ret)
				{
					sprintf(message, "ERORLogin failed: username incorrect\n");
					F_SendMessage(50);
					printf("Login failed: username incorrect\n");	
				}
				else if(ERR_PASSWORD_INVALID == ret)
				{
					sprintf(message, "ERORLogin failed: password incorrect\n");
					F_SendMessage(50);
					printf("Login failed: password incorrect\n");	
				}
				else /* ERR_ALREADY_LOGGED_IN */
				{
					printf("received command is: %s.\n User %s is already logged in. Sending error...\n", command, testUsername);
					myClientIndex=-1;
					sprintf(message, "ERORUser %s is already logged in\n", testUsername);
					F_SendMessage(50);
					
				}
				
			}
			
			
		}
		
		else if(0 == strcmp(command, "QUIT"))
		{
			printf("User %s has quit gracefully\n", testUsername);
			printf("connection with %s (IP %s, port %d) is closed\n",testUsername, client_ip, ntohs(tempAddress1.sin_port));
			close ( sa );
			F_UpdateOnlineStatus(myClientIndex, 0);
			exit(0);
		}
		
		else if(0 == strcmp(command, "WHO?") && isMyClientLogged)
		{
			F_HandleWhoCommand();
		}
		
		else
		{
			sprintf(message, "EROR Unknown command: %s (or not in time)\n",command);
			F_SendMessage(50);
		}
		
		
		
		/* UNLOCK SEMAPHORE!!!! */
		F_SaveDatabase();
		
				
			
	}//while(1)
	
	return ret;
}/* F_ChildProcess */






int main ( int argc, char *argv[] )
{
	int s, b, l, fd, bytes, on = 1;
				/* buffer for outgoing file */
	struct sockaddr_in channel;		/* hold's IP address */

	int pid;
	int ret = ERR_NONE;
	
	F_InitDatabase();
	

	/* Build address structure to bind to socket. */
	memset ( &channel, 0, sizeof ( channel ) );	/* zero channel */
	channel.sin_family = AF_INET;
	channel.sin_addr.s_addr = htonl ( INADDR_ANY );
	channel.sin_port = htons ( SERVER_PORT );

	/* Passive open. Wait for connection. */
	s = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ); /* create socket */
	if ( s < 0 ) fatal ( "socket failed" );
	setsockopt ( s, SOL_SOCKET, SO_REUSEADDR, ( char * ) &on, sizeof ( on ) );

	b = bind ( s, ( struct sockaddr * ) &channel, sizeof ( channel ) );
	if ( b < 0 ) fatal ( "bind failed" );

	l = listen ( s, QUEUE_SIZE );		/* specify queue size */
	if ( l < 0 ) fatal ( "listen failed" );

	/* Socket is now set up and bound. Wait for connection and process it. */
	while ( 1 )
	{
		sa = accept ( s, 0, 0 );		/* block for connection request */
		if ( sa < 0 ) fatal ( "accept failed" );
		
		pid = fork();
		if( pid < 0)
		{
			fatal("Unable to create child process, exiting.");
		}
		if(pid > 0)
		{
			/* parent process */
		}
		else
		{
			/* child process */
			F_ChildProcess(sa);
		}
		
	}//while(1)
}/* main() */


static void F_SaveDatabase()
{
	int i;
	FILE *fd;
	
	fd = fopen (DB_FILENAME, "w");
	fwrite ( ServerDatabase, 1, sizeof(ServerDatabase),fd );
	fflush(fd);
	fclose(fd);

}/* F_SaveDatabase */


static void F_LoadDatabase()
{
	int i;
	FILE *fd;
	
	fd = fopen (DB_FILENAME, "r");
	fread ( ServerDatabase, 1, sizeof(ServerDatabase),fd );
	fclose(fd);

}/* F_LoadDatabase */


static void F_UpdateOnlineStatus(int userIndex, int status)
{
	if(userIndex==-1) return;
	F_LoadDatabase();
	ServerDatabase[myClientIndex].isOnline = status;
	if(0 == status)
	{
		ServerDatabase[userIndex].postPort[0] = '\0';
		//ServerDatabase[userIndex].chatPort[0] = '\0';
		//ServerDatabase[userIndex].webPort[0] = '\0';
	}
	F_SaveDatabase();
}/* F_UpdateOnlineStatus */



static void F_HandleWhoCommand()
{
	int i = 0;
	
	
	//printf("entering handle who\n");
	
	/* find port */
	
		/* send WHOR - response */
	
		//sprintf(message,"WHOR");
		for(i=0; i < MAX_USERS; i++)
		{
			if(ServerDatabase[i].isOnline)
			{
				//sprintf(message,"WHOR%s#%s#%s#%s#%s\n", RegisteredUsers[i],
					//ServerDatabase[i].ipAddress, ServerDatabase[i].picPort,ServerDatabase[i].chatPort,ServerDatabase[i].webPort);
				sprintf(message,"WHOR%s#%s#%s\n", RegisteredUsers[i],
					ServerDatabase[i].ipAddress, ServerDatabase[i].postPort);
				
				F_SendMessage(5000);
			}
		}
		sprintf(message,"WHOR\n");
		F_SendMessage(5000);
		
		

}/* F_HandleWhoCommand */

#if 0
static void F_HandleFileSearch(char *token)
{
	int i, j;
	char tempToken[21] = "";
	
	for(i = 0; i < 21; i++)
	{
		if(token[i]=='\0' || token[i]=='\n')
		{
			break;
		}
		tempToken[i]=token[i];
	}
	tempToken[i]='\0';
	printf("Token received: %s\n", tempToken);
	
	
	//sprintf(message,"FRES");
	
	for(i = 0; i < MAX_USERS; i++)
	{
		if(ServerDatabase[i].isOnline && ServerDatabase[i].numOfSharedFiles)
		{
			for(j = 0; j < ServerDatabase[i].numOfSharedFiles; j++)
			{
				if(NULL != strstr(ServerDatabase[i].fileList[j], tempToken))
				{

					sprintf(message,"FRES%s#%s#%s#%s\n", 
							ServerDatabase[i].fileList[j],
							RegisteredUsers[i][0],
							ServerDatabase[i].ipAddress,
							ServerDatabase[i].port);
					F_SendMessage(100);

#if 0
					sprintf(message,"%s%s#%s#%s#%s\n",message, 
							ServerDatabase[i].fileList[j],
      							RegisteredUsers[i][0],
							ServerDatabase[i].ipAddress,
       							ServerDatabase[i].port);
#endif				
				}
			
			}
		}
		
	}
	sprintf(message,"FRES\n");
	F_SendMessage(50);
	
	
}/* F_HandleFileSearch */
#endif


fatal ( char *string )
{
	printf ( "fatal error: %s\nserver exiting...\n", string );
	exit (1);
}
