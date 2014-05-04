/* Timeline - Part 2
 * Presented by 30990006 and 301224283
 * IMPORTANT: To enable the usage of ports under 1024 by the WEB server, run this program as Superuser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>      /* standard unix functions, like getpid() */
#include <pthread.h>
#include <sys/types.h>   /* various type definitions, like pid_t*/
#include <string.h>      /* String library */
#include <semaphore.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <ctype.h>
#include <time.h>

#define TRACKER_PORT 12345		/* arbitrary, but client and server must agree */
#define BUF_SIZE 8092			/* TCP block transfer size */
#define SYN_REQUEST_QUEUE_SIZE 10       /* Defines the queue size for connection requests */
#define UDP_PAYLOAD_SIZE 1000           /* The payload size of a single framed UDP datagram */
#define MAX_UDP_BUFFER 65536            /* The meaximum supported size of a single incoming UDP datagram */
#define IMAGE 0
#define TXT 1

//Declarations
int SetInitialConnectionWithTheTracker (char*);
int LoginWithTheTracker (int);
void PrintTrackerLoginReply (int ,char* );
void CreateUsersOnlineUpdateThread (int*);
void* UsersOnlineUpdater (void*);
void Create_MyUserID_and_PostsOnMyWall_Files ();
void* WebServerWelcomeSocketThread (void*);
void* WebServerSocketThread (void*);
void CreateWebServerWelcomeSocketThread (char*);
void WebServer_html_GET_handler (int);
void WebServer_TXT_Or_Image_GET_handler (int,int,char*);
int FindFileNameIn_GET_Message (char*, char*);
void TXT_POST_Handler (int,char*);
int SearchUserPropertiesByID (char*, char*, char*);
void Send_HTTP_RedirectMessage (int);
void Image_POST_Handler (int,char*,int);
void Create_UDP_IncomingDataHandlerThread();
void* UDP_IncomingDataHandler();
void UDP_ReceiveASingleFullFile(int ,struct timeval,fd_set,char*);
int UDP_CheckFrameValidity (char**,char*);
int UDP_ReceiveTextMessagePayload(int,struct sockaddr_in,fd_set,struct timeval,char*,int,int,char*);
int UDP_ReceiveImageMessagePayload(int,struct sockaddr_in,fd_set,struct timeval,char*,int,int,char*);
void UDP_SendAck(int,struct sockaddr_in,int);
void BuildFileNameForIncomingJPG (char*,char*,char*,char*);
int UDP_SendASingleFile (char*,char*,char*);
int GetFileLength (char*);
void Build_UDP_2ByteHeader (int,int,int,char*);
int BuildASingleUDPFrame (int*,char*,int,int,FILE*,char*);
void SemaphoreInit ();

//Global variables
int _PostPort; //this server's UDP port for file reception
char _UserName[15]; //The logged in user
sem_t UsersOnlineFileLocker, PostsOnMyWallLocker, StrtokLocker,StrStrLocker, TXTPostFileLocker, PICPostFileLocker; //semaphores


int main(int argc, char** argv) 
{
    char tracker_IP[40], WebPort[10];
    int SocketToTracker;
    
    SemaphoreInit (); //init all semaphores
    
    srand(time(NULL));
    
    /*
    if (argc !=4) //illegal command line
        {
        printf("Illegal number of arguments! Exiting.\n");
        exit(0);
        }
    
    //breaking down the argv array:
    strcpy(tracker_IP,argv[1]); //get the 1st parameter
    strcpy(_UserName,argv[2]); //get the 2nd parameter
    strcpy(WebPort,argv[3]); //get the 3rd parameter
     */  
   
    strcpy(tracker_IP,"127.0.0.1");
    strcpy(_UserName,"301224283");
    strcpy(WebPort,"81");
    
    
    SocketToTracker = SetInitialConnectionWithTheTracker(tracker_IP); //connect to the tracker
    if (SocketToTracker==-1) //TCP failed to complete the 3-way-handshake
        {
        printf("Connection to tracker failed! Exiting\n");
        exit(1);
        }
    
    if(LoginWithTheTracker(SocketToTracker)!=1) //if the login failed
        exit(1); //exit
    
    Create_MyUserID_and_PostsOnMyWall_Files(); //create necessary files
    
    CreateUsersOnlineUpdateThread(&SocketToTracker); //create the thread that communicates with the tracker.
    
    CreateWebServerWelcomeSocketThread(WebPort); //create the welcome socket thread
    
    Create_UDP_IncomingDataHandlerThread(); //create the incoming UDP data handler thread
    
    while (1)
        {
        sleep(120); //done because sched_yield() simply won't work.
        }

    return 0; //meaningless. done to remove a warning
     
    
}//end of main()

/*
 * Initializes all relevant semaphores.
 */
void SemaphoreInit ()
{
    sem_init(&UsersOnlineFileLocker,0,1); //init semaphore
    sem_init(&PostsOnMyWallLocker,0,1); 
    sem_init(&StrtokLocker,0,1); 
    sem_init(&StrStrLocker,1,1); 
    sem_init(&TXTPostFileLocker,0,1); 
    sem_init(&PICPostFileLocker,0,1); 
} //end of SemaphoreInit()


/*
 * Creates a thread that handles all UDP datagram reception.
 */
void Create_UDP_IncomingDataHandlerThread()
{
    pthread_t temp;
    
    if (pthread_create(&temp,NULL,UDP_IncomingDataHandler,NULL))
        {
        printf("Error while creating a thread. Exiting.");
        exit(1);
        }
}//end of Create_UDP_IncomingDataHandlerThread()

/*
 * Handles all UDP incoming data. Run as a thread.
 */
void* UDP_IncomingDataHandler()
{
  struct sockaddr_in si_me;
  int s;
  fd_set read_set;
  struct timeval timeout;
  char ans[4]={'\0'}; //will hold the received file type.
  

  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) //create a socket
      {
      printf("UDP socket creation failed! exiting.\n");
      exit(1);
      }

  memset((char *) &si_me, 0, sizeof(si_me)); //clear the struct 
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(_PostPort); //set local UDP port to listen to
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (bind(s, &si_me, sizeof(si_me))==-1) //bind the socket
      {
      printf("UDP socket bind failed! exiting.\n");
      exit(1);
      }
  
  /* Set time limit. */
   timeout.tv_sec = 2;
   timeout.tv_usec = 0;
   
    /* Create a descriptor set containing our single socket.  */
   FD_ZERO(&read_set);
   FD_SET(s, &read_set);
  
  while (1) //loop forever
         {
         UDP_ReceiveASingleFullFile(s,timeout,read_set,ans);
         }//while (1)
  
  
}//end of UDP_IncomingDataHandler()

/*
 * Sends a single file via UDP socket
 * @param fileName - the file to be sent
 * @param otherIP - destination IP
 * @param otherPort - destination port
 */
int UDP_SendASingleFile (char* fileName, char* otherIP, char* otherPort)
{
    char singleFrame[UDP_PAYLOAD_SIZE+2]={'\0'}, type[4];
    int fileLength;
    int ans =1, numOfRetrans=0, isLast=0, currentSeqNum=0,isFirst=1;
    fd_set read_set;
    struct timeval timeout;
    struct sockaddr_in si_other;
    int sock, slen=sizeof(si_other);
    FILE *file;

  if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) //create a socket
      {
      printf("UDP socket creation failed! exiting.\n");
      exit(1);
      }
    
  memset((char *) &si_other, 0, sizeof(si_other)); //clear the destination address struct
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(atoi(otherPort)); //set dest. port
  
  if (inet_aton(otherIP, &si_other.sin_addr)==0)  //set dest. IP address
    {
    printf("inet_aton() failed!. Exiting.\n");
    exit(1);
    }

  /* Set time limit. */
   timeout.tv_sec = 0;
   timeout.tv_usec = 500000; //set timeout to 500ms
   
    /* Create a descriptor set containing our single socket.  */
   FD_ZERO(&read_set);
   FD_SET(sock, &read_set);
   
   fileLength = GetFileLength(fileName); //get the length of the file
   
   if (strcmp(fileName,"TextPost.txt")==0) //if it's a text message
        {
        strcpy(type,"TXT"); //note that this is a text file
        file = fopen(fileName,"rt"); //open as text file for reading
        }
   else 
        {
        strcpy(type,"PIC"); //note that this is a picture file
        file = fopen(fileName,"rb"); //open as binary file for reading
        }     
   
   do //create a frame, send it and wait for an ACK
     {
       if(!numOfRetrans) //build a new frame only if the last frame was ACKed correctly.
            {
             isLast = BuildASingleUDPFrame(&fileLength,type,currentSeqNum,isFirst,file,singleFrame); //build a frame
            }
       
       if (sendto(sock, singleFrame, sizeof(singleFrame), 0, &si_other, slen)==-1) //send
        {
        printf("UDP send failed!");
        ans =0;
        break;
        }
       
       if(!UDP_ReceiveACK(sock,currentSeqNum,read_set,timeout,si_other)) //ACK wasn't received successfully
            {
            numOfRetrans++; //count the retransmissions

            if(numOfRetrans<6)
                {
                continue; //resend the frame
                }
            else //5 retransmission were made
                {
                ans=0;
                break; //bail out
                }

            }//if(!UDP_ReceiveACK())
       
       /*ACK was received successfully*/
       currentSeqNum = (currentSeqNum+1)%2; //move to the next sequence number
       isFirst=0; //mark that the 1st frame was sent
       numOfRetrans=0; //reset
       
     }while (!isLast); //while we haven't finished with the file
    
   
    fclose(file); //close the file 
    return ans;
    
}//end of UDP_SendASingleFile()

/*
 * Receives a single ACK byte and checks it's validity.
 * @param sock - relevant socket
 * @param seqNum - the expected ACK bit
 * @param read_set - the fd_set for select()
 * @param timeout - the timeout struct
 * @param si_other - the expected other side
 * @return 1 if ack was received in time and correctly, 0 otherwise
 */
int UDP_ReceiveACK (int sock,int seqNum, fd_set read_set, struct timeval timeout, struct sockaddr_in si_other)
{
    int ans=1;
    int returnValue;
    unsigned char temp;
    struct sockaddr_in si_toCheck;
    int slen = sizeof(si_toCheck);
    char port1[7], port2[7];
    int receivedFrame;
    unsigned char seqMask = 0x80;
    
       /* Set time limit. */
     timeout.tv_sec = 0;
     timeout.tv_usec = 500000; //set timeout to 500ms
     
     returnValue = select(sock+1,&read_set,NULL,NULL,&timeout); //wait for the next frame. 
     
     if(!returnValue) //socket timed out
         ans=0;
     else //socket hasn't timed out
        {
 
         if (recvfrom(sock, &temp, 1, 0, &si_toCheck, &slen)==-1) //read the first byte from the socket. Save the sender's address.
            {
            printf("Reading from UDP socket failed! Exiting.\n");
            exit(1);
            }
         
            receivedFrame = (temp & seqMask)?1:0; //check the seq # bit.
            sprintf(port1,"%d",ntohs(si_other.sin_port)); //convert the expected port to a string
            sprintf(port2,"%d",ntohs(si_toCheck.sin_port)); //convert the port that the last message was received from to a string
         
         if (receivedFrame!=seqNum || strcmp(inet_ntoa(si_toCheck.sin_addr),inet_ntoa(si_other.sin_addr)) || strcmp(port1,port2) )
            //this isn't the seq # we're expecting or it came from an unexpected source. we need to throw it.
            {
             ans=0;
            }//if
        }//else
     
     return ans;  
}//end of UDP_ReceiveACK()

/*
 * Builds a single UDP frame.
 * @param fileLength - reference to the remaining file size to send
 * @param type - the file's type (TXT\PIC)
 * @param seqNum - the sequence number for the current frame
 * @param isFirstFrame - indicates whether it's the 1st frame or not
 * @param file - pointer to the file
 * @param ans - where the full frame will be saved
 * @return - 1 if this was the last frame, 0 otherwise 
 */
int BuildASingleUDPFrame (int *fileLength, char* type, int seqNum, int isFirstFrame, FILE* file, char* ans)
{
     char singleFrame[UDP_PAYLOAD_SIZE+2]={'\0'}, temp[UDP_PAYLOAD_SIZE]={'\0'};
    char temp2[20];
    int isLast=0,readBytes;
    
    if (isFirstFrame) //this is the 1st frame
        {
           sprintf(temp2,"%s#%s#",type,_UserName); //build the TYPE#USERNAME# string

           readBytes = fread(temp,1,UDP_PAYLOAD_SIZE-strlen(temp2),file); // read 1 item of size [ UDP_PAYLOAD_SIZE-strlen(temp)] from the file into temp
           *fileLength-=readBytes; //decrease the read bytes from the total file length
                   
           if (*fileLength==0) //finished reading the file
               isLast=1; //mark

           Build_UDP_2ByteHeader(seqNum,isLast,readBytes+strlen(temp2),singleFrame); //build the 2 byte header and store it in 'singleFrame'

           strcat (singleFrame,temp2); //concatenate the 2 byte header and the TYPE#USERNAME# string
           memcpy(singleFrame+strlen(singleFrame),temp,readBytes); //concatenate the full header with the data. this frame is now ready.
        }
    else //this isn't the first frame
         {
           readBytes = fread(temp,1,UDP_PAYLOAD_SIZE,file); // read 1 item of size [ UDP_PAYLOAD_SIZE] from the file into temp
           *fileLength-=readBytes; //decrease the read bytes from the total file length
                   
           if (*fileLength==0) //finished reading the file
               isLast=1; //mark

           Build_UDP_2ByteHeader(seqNum,isLast,readBytes,singleFrame); //build the 2 byte header and store it in 'singleFrame'

           memcpy(singleFrame+strlen(singleFrame),temp,readBytes); //concatenate the full header with the data. this frame is ready.
         }
    
    memcpy(ans,singleFrame,sizeof(singleFrame)); //save the frame
    return isLast;
}//end of BuildASingleUDPFrame()

/*
 * Builds the 2 byte header and stores it in 'ans'
 */
void Build_UDP_2ByteHeader (int seqNum, int isLast, int length, char* ans)
{
    unsigned short header=0, *ptr; //short is consisted of 2 bytes.
    char temp [3]={'\0'};
    
    if (seqNum) //set the seq num
       {
        header |= 0x8000;
       }
    if (isLast) //set the isLast bit
       {
        header |= 0x4000;
       }
    
    header |= length; //set the length 
    
    ptr = temp; 
    ptr[0] = htons(header); //make a string out of the 2 byte header. htons() is used to store it with msB first.
    
    strcpy(ans,temp); //save the result
}//end of Build_UDP_2ByteHeader()

/*
 * Receives a single full file (txt/img) via a UDP socket.
 * @param sock - the UDP socket
 * @param timeout - A timeout value for the reception.
 * @param ans - the result will saved here. "TXT" if a text was received successfully.
 * "PIC" if it's a picture file. "TTT" if the session timed out. "FFF" if the received frame is invalid.
 */
void UDP_ReceiveASingleFullFile(int sock,struct timeval timeout,fd_set read_set, char* ans)
{
   struct sockaddr_in si_other;
   int returnVal, slen=sizeof(si_other);
   int expectedFrame=0, receivedFrame; //the seq # of the next expected frame and the seq# of the currently received frame.
   int payloadLength, isLast;
   unsigned short ushort1, *ptr; //will be used to read the 1st 2 bytes from every frame
   unsigned short seqMask = 0x8000, isLastMask = 0x4000, lengthMask = 0x3FFF;
   char otherUser[12],type[4]={'\0'};
   char* buff; //will be used to store a single UDP datagram
   int buff_backup;
   
   buff = malloc (MAX_UDP_BUFFER*sizeof(char)); //allocate memory for the buffer
   memcpy(buff,MAX_UDP_BUFFER,0); //clear the buffer
   buff_backup = buff; //create a backup for buff
   
   do
    {
     returnVal = recvfrom(sock, buff, MAX_UDP_BUFFER-1, 0, &si_other, &slen); //read the first 2 bytes from the socket. Save the sender's address.

      if(returnVal==-1) //socket failed       
          {
          printf("Reading from UDP socket failed! Exiting.\n");
          exit(1);
          }
    }while(returnVal==1); //done to handle the case of a misbehaving ACK message that found its way into the socket.
   
    ptr = buff; //convert to a pointer of type unsigned-char
    ushort1 = ptr[0]; //extract the first 2 bytes from the received datagram.
    
   ushort1 = htons(ushort1); //convert the 2 bytes to be displayed in a way we can read the data easily.
   receivedFrame = (ushort1 & seqMask)?1:0; //check the seq # bit.
   isLast = (ushort1 & isLastMask)?1:0; //check the isLast bit.
   payloadLength = ushort1 & lengthMask; //get the payload length
   
   if (receivedFrame == expectedFrame) //this is the frame we're expecting
        {
        buff= buff+2; //throw away the 1st 2 header bytes
        memcpy(type,buff,3); //copy the file type 
        
        if (!strcmp(type,"TXT") || !strcmp(type,"PIC") ) //this is a valid frame type
            {
            buff+=3; //skip the type field
            }
        else //this file type is invalid and so is the entire frame.
            {
            strcmp(ans,"FFF"); //mark that an invalid and unexpected data has arrived
            return;
            }
        
        returnVal = UDP_CheckFrameValidity(&buff,otherUser); //check if the username is valid
        
        /*The socket now contains the payload only */
        if(!returnVal) //if the user ID field is invalid
            return;
        
        /*buff now points to the 1st data byte*/
        
         UDP_SendAck(sock,si_other,expectedFrame); //send an ACK
         payloadLength -= strlen(otherUser)+3+1+1; //calculate the length of the text data only
        
        if ( strcmp(type,"TXT")==0 ) //if this is a text message
            {
            returnVal = UDP_ReceiveTextMessagePayload(sock,si_other,read_set,timeout,buff,payloadLength,isLast,otherUser);
            if (returnVal==1)
                printf("Text message received successfully!\n");
            if(returnVal==0)
                printf("UDP reception timed out!\n");   
            }
        else //this is a picture file
            {
            returnVal = UDP_ReceiveImageMessagePayload(sock,si_other,read_set,timeout,buff,payloadLength,isLast,otherUser);
            if (returnVal==1)
                printf("Image file received successfully!\n");
            if(returnVal==0)
                printf("UDP reception timed out!\n");
            }
        
        }//if (receivedFrame == expectedFrame) 
   else //the seq # is wrong. We need to throw this frame away.
        {
         printf("Unexpected UDP frame (wrong sequence #) was thrown!\nExpected frame #%d, received #%d.\n",expectedFrame,receivedFrame);
        }
   
   free((char*)buff_backup); //free buff_backup, since buff was destroyed
   
    
}//end of UDP_ReceiveASingleFullFile()

/*
 * Assembles an entire image file by receiving all relevant frames.
 * @param sock - UDP socket
 * @param si_other - the details of the sending side
 * @param read_set - the set of sockets for 'select()'
 * @param timeout - timeout value
 * @param firstFrame - the payload of the 1st frame
 * @param firstPayloadLength - the payload length of the 1st frame
 * @param isLast - indicated whether this is the last frame or not
 * @return 1 if successful
 * @return 0 if timed-out
 * @return -1 if select failed
 */
int UDP_ReceiveImageMessagePayload(int sock,struct sockaddr_in si_other,fd_set read_set, struct timeval timeout,
        char* firstFrame,int firstPayloadLength, int isLast,char* otherUser)
{
    char temp[2]={'\0'}, fileName[30]={'\0'}, date[15]={'\0'}, time[10]={'\0'};
    struct sockaddr_in si_toCheck;
    int expectedFrame=1,i;
    char port1[7], port2[7];
    FILE* file;
    int returnValue, slen=sizeof(si_other);
    int payloadLength, receivedFrame;
    unsigned short ushort1, *ptr; //will be used to read the 1st 2 bytes from every frame
    unsigned short seqMask = 0x8000, isLastMask = 0x4000, lengthMask = 0x3FFF;
    char buff[MAX_UDP_BUFFER];
    
    /*Logic: We open a new file for the picture and write to it as we go. */
    BuildFileNameForIncomingJPG(otherUser,fileName,time,date); //get the appropriate file name for this picture
    
    file = fopen(fileName,"wb"); //open a new file for the picture
    
    for (i=0; i< firstPayloadLength; i++)
        {
        temp[0]=firstFrame[i]; //read a single byte from the initial payload
        fwrite(temp,sizeof(char),1,file); //write a single byte from temp to the file 
        }
    
    while(!isLast) //if this is NOT the only frame
       {
        /* Set time limit. */
        timeout.tv_sec = 2; //set timeout to 2 seconds 
        timeout.tv_usec = 0;     
        
        returnValue = select(sock+1,&read_set,NULL,NULL,&timeout); //wait for the next frame. 
        if (returnValue==0) //connection timed out
            return 0;
        if (returnValue==-1) //select() failed
            return -1;
        
        if (recvfrom(sock, buff, sizeof(buff), 0, &si_toCheck, &slen)==-1) //read the next datagram
            {
            printf("Reading from UDP socket failed! Exiting.\n");
            exit(1);
            }

        ptr = buff; //convert to a pointer of type unsigned-char
        ushort1 = ptr[0]; //extract the first 2 bytes from the received datagram.
        ushort1 = htons(ushort1); //convert the 2 bytes to be displayed so we can read the data easily.

        receivedFrame = (ushort1 & seqMask)?1:0; //check the seq # bit.
        isLast = (ushort1 & isLastMask)?1:0; //check the isLast bit.
        payloadLength = ushort1 & lengthMask; //get the payload length

        sprintf(port1,"%d",ntohs(si_other.sin_port)); //convert the desired port to a string
        sprintf(port2,"%d",ntohs(si_toCheck.sin_port)); //convert the port that the last message was received from to a string

        if (receivedFrame!=expectedFrame || strcmp(inet_ntoa(si_toCheck.sin_addr),inet_ntoa(si_other.sin_addr)) || strcmp(port1,port2) )
            //this isn't the seq # we're expecting or it came from an unexpected source. we need to throw it.
            {
            isLast=0; //we haven't finished yet.
            UDP_SendAck(sock,si_other,(expectedFrame+1)%2); //send an ACK for the previous frame 
            }//if it's not the expected frame

        else //this is the expected frame
             {
             
              for (i=2; i< payloadLength+2; i++) //read the payload
                {
                fwrite(buff+i,sizeof(char),1,file); //write a single byte from temp to the file 
                }//for

              UDP_SendAck(sock,si_other,expectedFrame); //send an ACK for the current frame 
              expectedFrame = (expectedFrame+1)%2; //update the expected seq #
             }//else
         
         }//while(!isLast)
    
    /*If we've reached this point, it means the entire image data was received*/
    fclose(file); //close the image file stream
    
    file = fopen("PostsOnMyWall.txt","a+"); //open the file for appending
    if (file==NULL)
        {
        printf("Failed to open PostsOnMyWall.txt! exiting\n");
        exit(1);
        }
    
    fprintf(file,"<tr><td>user %s upload picture:<br> <img src=""%s"" width=""100%""/> "
            "<br> uploaded at: %s %s </td></tr>\n",otherUser,fileName,time,date); //write the valid html line to the file
    fclose(file); //close the file stream
    
    
    return 1;
}//end of UDP_ReceiveImageMessagePayload()


/*
 * Builds a filename for an incoming image, creates current time and date strings.
 * filename format: other-user_hh_mm_ss.jpg
 * @param otheruser - A string with the sender's IP
 * @param Filename - where the picture's file name string will be saved
 * @param time - where the current time string will be saved
 * @param date - where the current date string will be saved
 */
void BuildFileNameForIncomingJPG (char* otherUser, char* Filename, char* current_time, char* date)
{
    /*Important insight: There should be no spaces in the file's name. */
    char temp_filename[30]={'\0'};
    char temp[30],temp_time[15]={'\0'};
    time_t epoch_time;
    struct tm *tm_p;
    
    epoch_time = time( NULL );
    tm_p = localtime( &epoch_time );
    
    strcat(temp_filename,otherUser); //copy the username
    strcat(temp_filename,"_");
    
    sprintf(temp,"%2d",tm_p->tm_hour); //convert the hour to a string
    if(temp[0]==' ') //remove a space char if necessary
        temp[0]='0';
    
    strcat(temp_time,temp); //add to the time string
    strcat(temp_time,":"); //add to the time string
    
    strcat(temp_filename,temp); //add to the filename string
    strcat(temp_filename,"_");
    
    sprintf(temp,"%2d",tm_p->tm_min); //convert the minutes to a string
    if(temp[0]==' ') //remove a space char if necessary
        temp[0]='0';

    strcat(temp_time,temp); //add to the date string
    strcat(temp_filename,temp); //add to the filename string
    strcat(temp_filename,"_");
    
    sprintf(temp,"%2d",tm_p->tm_sec); //convert the seconds to a string
    if(temp[0]==' ') //remove a space char if necessary
         temp[0]='0';
    
    strcat(temp_filename,temp); //add to the filename string
    strcat(temp_filename,".jpg"); //add to the filename string
    
    strcpy(current_time, temp_time); //save the time
    strcpy(Filename,temp_filename); //save the filename
    
    sprintf(date,"%d/%d/%d", tm_p->tm_mday, tm_p->tm_mon + 1, tm_p->tm_year + 1900); //save the date
    
}//end of void BuildFileNameForIncomingJPG ()


/*
 * Assembles an entire text file by receiving all relevant frames. Writes the complete message to 'PostsOnMyWall.txt'.
 * @param sock - UDP socket
 * @param si_other - the details of the sending side
 * @param read_set - the set of sockets for 'select()'
 * @param timeout - timeout value
 * @param firstPayload - the payload of the 1st frame
 * @param firstPayloadLength - the payload length of the 1st frame
 * @param isLast - indicated whether this is the last frame or not
 * @return 1 if successful
 * @return 0 if timed-out
 * @return -1 if select failed
 */
int UDP_ReceiveTextMessagePayload(int sock,struct sockaddr_in si_other,fd_set read_set, struct timeval timeout,
        char* firstPayload, int firstPayloadLength, int isLast,char* otherUser)
{
    char message[BUF_SIZE+1]={'\0'}; //we assume a text message can't be bigger than BUF_SIZE
    char *buff;
    char temp[2]={'\0'}, port1[7], port2[7];
    struct sockaddr_in si_toCheck;
    int expectedFrame=1,i;
    FILE* file;
    int returnValue, slen=sizeof(si_other);
    int payloadLength, receivedFrame;
    unsigned short ushort1, *ptr; //will be used to read the 1st 2 bytes from every frame
    unsigned short seqMask = 0x8000, isLastMask = 0x4000, lengthMask = 0x3FFF;
    int buff_backup;
    
    buff = malloc (8092*sizeof(char)); //allocate memory for the buffer. We were told we can assume that a text message 
    //won't be more than 8K bytes long
    memcpy(buff,8092,0); //clear the buffer
    buff_backup = buff;
    
    /*Logic: we firstly assemble the entire text message and only then we'll write it to 'PostsOnMyWall.txt' */
    
    for (i=0; i< firstPayloadLength; i++)
        {
        temp[0]=firstPayload[i]; //read a single byte from the initial payload
        strcat(message,temp); //assemble the message
        }
    
    while(!isLast) //if this is NOT the only frame
       {
        /* Set time limit. */
        timeout.tv_sec = 2; //set timeout to 2 seconds 
        timeout.tv_usec = 0; 
   
        returnValue = select(sock+1,&read_set,NULL,NULL,&timeout); //wait for the next frame. 
        if (returnValue==0) //connection timed out
            return 0;
         if (returnValue==-1) //select() failed
            return -1;
        
         if (recvfrom(sock, buff, sizeof(buff), 0, &si_toCheck, &slen)==-1) //read the next datagram
            {
            printf("Reading from UDP socket failed! Exiting.\n");
            exit(1);
            }
                
            ptr = buff; //convert to a pointer of type unsigned-char
            ushort1 = ptr[0]; //extract the first 2 bytes from the received datagram.
            ushort1 = htons(ushort1); //convert the 2 bytes to be displayed as we can read the data easily.
            
            receivedFrame = (ushort1 & seqMask)?1:0; //check the seq # bit.
            isLast = (ushort1 & isLastMask)?1:0; //check the isLast bit.
            payloadLength = ushort1 & lengthMask; //get the payload length
            
            sprintf(port1,"%d",ntohs(si_other.sin_port)); //convert the desired port to a string
            sprintf(port2,"%d",ntohs(si_toCheck.sin_port)); //convert the port that the last message was received from to a string
         
            if (receivedFrame!=expectedFrame || strcmp(inet_ntoa(si_toCheck.sin_addr),inet_ntoa(si_other.sin_addr)) || strcmp(port1,port2) )
                //this isn't the seq # we're expecting or it came from an unexpected source. we need to throw it.
                {                
                isLast=0; //we haven't finished yet.
                UDP_SendAck(sock,si_other,(expectedFrame+1)%2); //send an ACK for the previous frame 
                }//if
            
            else //this is the expected frame
                 {
                  buff+=2; //move the ptr to the 1st byte of the payload
                  for (i=0; i< payloadLength; i++)
                    {
                    temp[0]=buff[i]; //read a single byte from the current payload and convert to string
                    strcat(message,temp); //assemble the message
                    }
                  
                  UDP_SendAck(sock,si_other,expectedFrame); //send an ACK for the current frame 
                  expectedFrame = (expectedFrame+1)%2; //update the expected seq #
                 }//else
         
       }//while(!isLast)
    
    /*If we've reached this point, it means the entire text data was received*/
    
    if( message[strlen(message)-1]=='\n' ) //if the text ends with a \n
        message[strlen(message)-1]='\0';  //remove it
    
    file = fopen("PostsOnMyWall.txt","a+"); //open a file for appending
    if (file==NULL)
        {
        printf("Failed to open PostsOnMyWall.txt! exiting\n");
        exit(1);
        }
    
    fprintf(file,"<tr><td>user %s says: %s</td></tr>\n",otherUser,message); //write the entire message to the file
    fclose(file); //close the file stream
    
    free((char*)buff_backup); //free buff_backup, since buff was destroyed
    
    return 1;
}//end of UDP_ReceiveTextMessagePayload()

/*
 * Sends a single ACK frame via the UDP socket
 */
void UDP_SendAck(int sock,struct sockaddr_in si_other, int value)
{
    unsigned char ack;
    int slen = sizeof(si_other);
    
    ack = (value)? 0x80 : 0; //if the value isn't 0, place '1' is the MSB. Place 0 otherwise.
    
     if (sendto(sock,&ack,1, 0, &si_other, slen)==-1)
        {
        printf("Sending an ACK via the UDP socket has failed! exiting.\n");
        exit(1);
        }    
    
}//end of UDP_SendAck()

/*
 * Checks if the username field is correct.
 * @param buff - a double pointer to the current buffer
 * @param otherUser - the ID of the sending user will be saved here
 * @return - 1 if the frame is valid, 0 otherwise.
 */
int UDP_CheckFrameValidity (char** buff, char* otherUser)
{
    char userName[12]={'\0'};
    char temp[2]={'\0'};
    int ans=1,i=0;
    
    if (*buff[0]!='#') //if theres no '#' after the 'type' field
        ans=0; //set a negative answer
    
    (*buff)++; //move onto the next byte
    
    while (*buff[0]!='#') //while we haven't reached the 2nd '#' char
         {
          if (!isdigit(*buff[0])) //check if the ID string is consisted of digits only
             {
              ans=0; //set a negative answer
              break;
             }
        
          temp[0]=*buff[0]; //make a string out of the current char
          strcat(userName,temp); //assemble the user ID
          (*buff)++; //move onto the next byte
          i++;
          
          if (i>9) //this isn't a valid user ID
                {
                ans=0; //set a negative answer
                break; //exit the loop
                }
         }
    
    if (i==0) //there were 2 consecutive '#'s right at the beginning.
        ans=0;
    
    (*buff)++;
    strcpy(otherUser,userName); //copy the username
    
    return ans;
}//end of UDP_CheckFrameValidity()

/*
 * Creates the WebServerWelcomeSocketThread thread.
 */
void CreateWebServerWelcomeSocketThread (char* WebPort)
{
     pthread_t temp;
    
    if (pthread_create(&temp,NULL,WebServerWelcomeSocketThread,WebPort))
        {
        printf("Error while creating a thread. Exiting.\n");
        exit(1);
        }
 
}//end of CreateWebServerWelcomeSocketThread()

/*
 * Will be used by a thread that will act as the Welcome-socket for all incoming requests.
 * Creates a new thread for every established connection
 * @param WebPort - A string with the port this we server will listen to
 */
void* WebServerWelcomeSocketThread (void* WebPort)
{
  int sock, b, l, sa, i=-1;
  int sockets[200]; //will be used to hold the numbers of different sockets created (because pthread_create want a ptr to sa)
  struct sockaddr_in channel;		/* holds IP address */
  pthread_t temp;

  /* Build address structure to bind to socket. */
  memset(&channel, 0, sizeof(channel));	/* zero channel */
  channel.sin_family = AF_INET;
  channel.sin_addr.s_addr = htonl(INADDR_ANY);
  channel.sin_port = htons(atoi((char*)WebPort)); //convert the web browser's port to an int and make it compatible with big/little endian
 

  /* Passive open. Wait for connection. */
  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /* create socket */
  if (sock < 0) 
        {
         printf("Welcome socket failed!\n");
         exit(1);
        }

  b = bind(sock, (struct sockaddr *) &channel, sizeof(channel));
  if (b < 0) 
        {
         printf("WEB server's welcome-socket bind failed! Make sure the program is run by a SU!\n");
         exit(1);
        }

  l = listen(sock, SYN_REQUEST_QUEUE_SIZE);		/* specify queue size */
  if (l < 0) 
        {
        printf("Welcome socket listen failed!\n");
        exit(1);
        }

  /* Socket is now set up and bound. Wait for connection and process it. */
  while (1) 
        {
           sa = accept(sock, 0, 0);		/* block for connection request */
           if (sa < 0) printf("Welcome socket accept failed!\n");
           
           i++; //advance the ctr
           i%=200; 
           sockets[i]=sa;

           if (pthread_create(&temp,NULL,WebServerSocketThread,&sockets[i])) //create a new connection thread.
                {
                printf("Error while creating a Web-Server socket thread. Exiting.");
                exit(1);
                }
           
           pthread_detach(temp); //detach the thread
           
        }//while (1)
} //CreateWebServerWelcomeSocketThread()

/*
 * Handles all requests arriving to a socket
 * @param _sock - the socket number associated with the current connection
 */
void* WebServerSocketThread (void* _sock)
{
    int sock = *((int*)_sock); //copy the socket's descriptor
    char buff[BUF_SIZE],copy[BUF_SIZE+1]; //will be used to read data from the socket
    char buff2[BUF_SIZE]={'\0'};
    char FileName[300]={"\0"}; //we assume that a file's name can't be bigger then 300 bytes.
    int FileType, readBytes;
    int i=0, temp=0;
       
    readBytes = read(sock,buff,BUF_SIZE);
       
    memcpy(copy,buff,readBytes); //make a copy of the input
    copy[BUF_SIZE]='\0';
    
    
    if (copy[0]=='G' && copy[1]=='E' && copy[2]=='T') //this is a GET message
        {
        copy[6]='\0'; //isolate the string from the 1st char to the 6th char  
        if(strcmp(copy,"GET / ")==0) //if this is a GET HTTP request
            {
            WebServer_html_GET_handler (sock); //handle the html request
            }
        else
            {
            FileType = FindFileNameIn_GET_Message(buff,FileName); //get the file type and name string

            if (FileType!=-1) //if this a valid file request
            WebServer_TXT_Or_Image_GET_handler(sock,FileType,FileName); //deal with the reply file
            }
        }// if it's a GET request
    
    if (copy[0]=='P' && copy[1]=='O' && copy[2]=='S' && copy[3]=='T') //this is a POST message
        {
        
        while (strstr(copy,"filename=")==NULL && strstr(copy,"textline")==NULL) //while we haven't read the "filename" or "textline" field
           {
            
           temp = read(sock,buff2,BUF_SIZE-readBytes); //read again from the socket
           memcpy(buff+readBytes,buff2,temp); //add to the previously read data
           readBytes+=temp; //update the counter
           
           memcpy(copy,buff,readBytes); //make a copy of the input
           }
     
           if (strstr(copy,"textline")!=NULL) //this is text POST
               {
                TXT_POST_Handler(sock,buff); //handle the text post message
               }

          if (strstr(copy,"filename=")!=NULL) //this is text POST
               {
                Image_POST_Handler(sock, buff, readBytes); //handle the image post message
               } 
        
        
        }//if POST
    
    close(sock); //close the socket
    
    pthread_exit(NULL);
    
}//end of WebServerSocketThread()


 /* handles image POST reception and calls the sending function
  * @param sock - the relevant socket
  * @param buffer - a buffer containing maximum BUF_SIZE bytes from the socket's buffer.
  * @param readBytes - the number of bytes read from the socket
  */
 void Image_POST_Handler (int sock,char* buffer, int NumOfBytesInBuffer)
 {
     FILE* img;
     
     char id[10]={"\0"}, postPort[6]={"\0"}, IP[16]={"\0"};
     char temp[2]={"\0"};
     char* ptr, *beginningOfContent, copy[BUF_SIZE], fileName[300]={"\0"};
     char boundaryString[BUF_SIZE]={"\0"}, lengthString[100]={"\0"};
     long length=0; //will hold the image's length
     int numOfBytesWritten=0; //will indicate that number of file bytes copied from the buffer to a file
     
     printf("Entered Image_POST_Handler()!\n");
     
     Send_HTTP_RedirectMessage (sock); //send the browser an HTTP redirect message, causing it to refresh the page.
     
     memcpy(copy,buffer,NumOfBytesInBuffer); //make a full copy of the buffer. Not only until the 1st '\0'
     
     sem_wait(&StrStrLocker); //lock
 
     ptr = strstr(copy,"Content-Length:"); //find the string 'Content-Length:'
     ptr = strstr(ptr," "); //find the string the space char before the length's value.
     ptr++; //move the ptr to the 1st char of the length
     
     while (*ptr!='\r') //assemble the entire length string
         {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
         strcat(lengthString,temp); //assemble the length string
         ptr++;
         }
     
     length = atol(lengthString); //convert the length string to a long
     
     ptr=copy; //reset the pointer
     
     ptr = strstr(ptr,"boundary"); //find the word 'boundary' 
     sem_post(&StrStrLocker); //unlock
     
     ptr+=9; //move the ptr to the beginning of the boundary string
     
     while (*ptr!='\r') //assembling the boundary string
         {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
         strcat(boundaryString,temp); //assemble the boundary string
         ptr++;
         }
   
      sem_wait(&StrStrLocker); //lock
      beginningOfContent = strstr(ptr,"\r\n\r\n")+4; //find where the content begins. will be used to calculate the actual image size later
     
     ptr=copy; //reset the pointer
     ptr = strstr(ptr,boundaryString); //find the 1st boundary string
     ptr++;
     ptr = strstr(ptr,boundaryString); //find the 2nd boundary string
     ptr++; //move the ptr to the 2nd boundary char
     ptr = strstr(ptr,"\r\n\r\n"); //find the 1st empty line before the ID
     sem_post(&StrStrLocker); //unlock
     
     ptr+=4; //move the ptr to the 1st char of the ID
     
     while (*ptr!='\r') //assembling the id string
        {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
        strcat(id,temp); //assemble the user ID
        ptr++;
        }
     
     sem_wait(&StrStrLocker); //lock
     ptr = strstr(ptr,"filename="); //find the filename field
     ptr+=10; //move the ptr to the first char of the name string
     
      while (*ptr!='"') //assembling the file name string
        {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
         strcat(fileName,temp); //assemble the file name string
         ptr++;
        }
     
     ptr = strstr(ptr,"Content-Type:"); //find the 'Content-Type:' field
     ptr++;
     ptr = strstr(ptr,"\r\n"); //find the end of the current line
     ptr+=4; //set the ptr to the 1st byte of image
     
     length -= (ptr-beginningOfContent + 46); //decrease the content header length from the total content length. now we almost have the picture's actual size.
     //after some calculations, we've noticed the content length has an extra 46 bytes that comes after the image data.
     
     sem_post(&StrStrLocker); //unlock
     
     
     img = fopen(fileName,"wb"); //open a file for writing, with the image's name.
     
     if (img==NULL) //if the POST message was an empty one
        {
         printf("Failed to create a new image file! Exiting\n");
         return; //exit the func
        }

     while (numOfBytesWritten<length && (ptr-copy < NumOfBytesInBuffer)) //while we haven't written the entire file and haven't extracted all the 
         //data with the initial read from the socket.
            {
            fwrite(ptr,sizeof(char),1,img); //write a single byte from ptr to the file
            ptr++; //advance the ptr
            numOfBytesWritten++; //count the number of written bytes

            } //while (numOfBytesWritten<length && (ptr-copy < NumOfBytesInBuffer))
     
     //we've finished with the 1st data chunk that was read from the socket. Now we handle the case that there's still data in the socket.
     
     if (numOfBytesWritten!=length) //we haven't got the entire file yet
        {
         while (numOfBytesWritten!=length) //while we haven't written the entire file
            {
            read(sock,temp,1); //read a single byte from the socket
            fwrite(temp,sizeof(char),1,img); //write a single byte from temp to the file 
            numOfBytesWritten++; //count the number of written bytes written 
            
            }//while
        }//if (numOfBytesWritten!=length)
    
     fclose(img); //close the file
     
     if (SearchUserPropertiesByID (id, postPort,IP) ) //if the dest. user exists.
        {
        if (!UDP_SendASingleFile (fileName,IP, postPort) ) //send the picture via UDP
             printf("Failed to send the image post!\n");
        else printf("Image post was sent successfully!\n");
        }

         
 }//end of Image_POST_Handler

 /* handles text POST reception and calls the sending function
  * @param sock - the relevant socket
  * @param buffer - a buffer containing the full HTTP POST message. We assume the entire message can't be bigger then BUF_SIZE
  */
 void TXT_POST_Handler (int sock,char* buffer)
 {
     FILE* txt;
     char id[10]={"\0"}, postPort[6]={"\0"}, IP[16]={"\0"};
     char temp[2]={"\0"};
     char* ptr, *text_ptr, copy[BUF_SIZE];
     char boundaryString[BUF_SIZE]={"\0"};
     
     printf("Entered TXT_POST_Handler()!\n");
     
     Send_HTTP_RedirectMessage (sock); //send the browser an HTTP redirect message, causing it to refresh the page.
     
     strcpy(copy,buffer); //make a copy of the buffer
     
     sem_wait(&StrStrLocker); //lock
     ptr = strstr(copy,"boundary"); //find the word 'boundary'
     
     if(ptr==NULL)
         return;
     
     sem_post(&StrStrLocker); //unlock
     
     ptr+=9; //move the ptr to the beginning of the boundary string
     
     while (*ptr!='\r') //assembling the boundary string
         {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
         strcat(boundaryString,temp); //assemble the boundary string
         ptr++;
         }
   
      sem_wait(&StrStrLocker); //lock
     ptr = strstr(ptr,boundaryString); //find the word 'boundary'
     
     if(ptr==NULL)
     return;
     
     ptr++; //move the ptr to the 2nd boundary char
     ptr = strstr(ptr,"\r\n\r\n"); //find the 1st empty line before the ID
     sem_post(&StrStrLocker); //unlock
     
     ptr+=4; //move the ptr to the 1st char of the ID
     
     while (*ptr!='\r') //assembling the id string
        {
         temp[0] = *ptr; //these 2 lines simply create a string consisted of a single char
         temp[1] = '\0';
        strcat(id,temp); //assemble the user ID
        ptr++;
        }
     
     sem_wait(&StrStrLocker); //lock
     ptr = strstr(ptr,"\r\n\r\n"); //find the 1st empty line in the text
     
     if(ptr==NULL)
     return;
     
     sem_post(&StrStrLocker); //unlock
     
     ptr+=4; //move the ptr to the beginning of the text message
     text_ptr=ptr; //make a copy of the ptr to the text
     
     txt = fopen("TextPost.txt","w"); //open a file for writing
     
     sem_wait(&StrtokLocker);
     fprintf(txt,"%s\n",strtok(ptr,"\r\n")); //get the text message and write it into a file
     sem_post(&StrtokLocker);
     
     fclose(txt); //close the file
     
     //the next issue is to handle an empty text post
     
     if (*text_ptr=='\r' && *(text_ptr+1)=='\n') //if it's any empty text message
        {
         printf("Invalid empty text post! Ignoring.\n");
         return;
        }
     
     if (SearchUserPropertiesByID (id, postPort,IP)) //find the corresponding postPort and IP to the current ID.
         //don't allow self posts.
        {
         if (!UDP_SendASingleFile ("TextPost.txt",IP, postPort) ) //send the text message via UDP
             printf("Failed to send a text post!\n"); 
         else printf("Text post was sent successfully!\n");
        }
         
 }//end of TXT_POST_Handler
 
 /* Creates and sends a redirect message to the browser, causing it to refresh the page after a POST mesage.
  * @param sock - the socket to the browser
  */
 void Send_HTTP_RedirectMessage (int sock)
 {
     char message [100] = {"\0"};
     
     strcat (message,"HTTP/1.1 302 Found\r\nLocation:.\r\n\r\n\r\n");
     
     write(sock, message, strlen(message)); //send the redirect message
     
 }//end of Send_HTTP_RedirectMessage()
 
 /* Searches the Users.txt file for a given ID and it's IP and postPort
  * @param id - the id to search for
  * @param postPort - the postPort of the given ID will be copied here, if found.
  * @param IP - the IP of the given ID will be copied here, if found.
  * @return - 1 if ID was found, 0 otherwise.
  */
 int SearchUserPropertiesByID (char* id, char* postPort, char* IP)
{ 
 
     FILE* file;
     char* ptr;
     char buffer[BUF_SIZE];
     int ans=0;
     
     sem_wait(&UsersOnlineFileLocker);
     file = fopen("Users.txt","rt"); //open the file for reading
     if (file==NULL)
        {
         printf("Failed to open ""Users.txt""! Exiting!");
         exit(1);
        }
     
      while(fgets(buffer, BUF_SIZE-1, file) != NULL) //while reading is successful
      {
        sem_wait(&StrStrLocker); //lock
        ptr = strstr(buffer,id); //find the id in the file
        sem_post(&StrStrLocker); //unlock
        if(ptr) //if the wanted id is found
            {
            sem_wait(&StrtokLocker);    
            strtok(ptr," "); //throw away the id value
            strcpy(IP, strtok(NULL," ")); //copy the ip field to the relevant string
            strcpy(postPort, strtok(NULL,"\n")); //copy the postPort field to the relevant string
            sem_post(&StrtokLocker);
          
            ans=1; //mark the the id was found
            }
               
      }
     
     fclose(file); //close
     sem_post(&UsersOnlineFileLocker); //release
     return ans;
     
}//end of SearchUserPropertiesByID()

/*
 * Retrieves the file name from the client GET message
 * @param buff - a string with the input from the client
 * @param FileName - the file name will be saved here
 * @return - IMAGE if this is an image request, TXT if this is a txt request, -1 if this request is invalid.
 */
int FindFileNameIn_GET_Message (char* buff, char* FileName)
{
    char temp[BUF_SIZE]={"\0"};
    int ans=IMAGE;
    char* tok;
    
    sem_wait(&StrtokLocker); //lock
    
    if ( (tok=(strtok(buff,"/,\n")))==NULL || strlen(tok) !=4) //discard everything left of the filename field. Exit if the request is not
        //a valid GET FILE request
         {
         ans =-1;
         }
    else //this seems to be a valid request
        {
        
        tok = strtok(NULL,"."); //take the file name
        if (tok==NULL) //if this is some sort of invalid GET request (happens sometimes)
           {
            return -1;
           }
        
        strcat(FileName,tok); //save the file name
        strcat(FileName,"."); //add the .
        strcat(temp,strtok(NULL,"\n")); //take the file type + some garbage to the 1st \n char

        sem_post(&StrtokLocker); //unlock

        temp[3]='\0'; //set the string to contain only the 3 initial chars

        if(strcmp(temp,"txt")==0) //if it's a txt file
            {
            ans = TXT;
            }

        strcat(FileName,temp); //add the file type to the name
        }//else
    
    
    return ans;
}//end of FindFileNameIn_GET_Message()

/*
 * Handles txt/image GET requests
 * @param sock - socket id
 * @param type - file type as returned from FindFileNameIn_GET_Message()
 * @param FileName - string with the file's name
 */
void WebServer_TXT_Or_Image_GET_handler (int sock, int type, char* FileName)
{
   char header[200], buff[BUF_SIZE],temp[20];
   unsigned char img_buff[BUF_SIZE], *img_buff_ptr;
   FILE* file;
   int FileSize=0;
   int ReadBytes=0,fd;
   
   if (type == TXT) //if the requested file is *.txt
        {

        file = fopen (FileName,"rt");
        
        if(file==NULL)//illegal file name
            return;
        
        if(strcmp(FileName,"UsersOnLine.txt")==0) //if it's the UsersOnline.txt file
           {
            sem_wait(&UsersOnlineFileLocker); //lock
           }
        
        if(strcmp(FileName,"PostsOnMyWall.txt")==0) //if it's the PostsOnMyWall.txt file
           {
            sem_wait(&PostsOnMyWallLocker); //lock
           }        
        
        fseek(file, 0L, SEEK_END); //move the ptr to the end of the file
         FileSize = ftell(file); //get the file length
         fseek(file, 0L, SEEK_SET); //move the ptr back to the beginning of the file
         
         fclose(file);

         strcpy(header,"HTTP/1.1 200 OK\r\n\\Content-TYPE: text\r\n\\Content-Length: ");
         snprintf(temp, 20, "%d",FileSize); //convert the PostPort from int to string

         strcat(header,temp); //add the file size to the header
         strcat(header," \r\n\r\n"); //end the header

         write(sock,header,strlen(header)); //send the header

             fd = open(FileName,O_RDONLY);

             while (1) //send the html file
                     {
                     ReadBytes = read(fd, buff, BUF_SIZE); /* read from file */
                     if (ReadBytes <= 0) break;		 /* check for end of file */
                     write(sock, buff, ReadBytes);		 /* write bytes to socket */
                     } 
             
             close(fd);
             
             
          if(strcmp(FileName,"UsersOnLine.txt")==0) //if it's the UsersOnline.txt file
           {
            sem_post(&UsersOnlineFileLocker); //unlock
           }
          if(strcmp(FileName,"PostsOnMyWall.txt")==0) //if it's the PostsOnMyWall.txt file
           {
            sem_post(&PostsOnMyWallLocker); //unlock
           }  
             
         }//if (file==TXT)
   else //this is an image request
        {
       
         file = fopen (FileName,"rb");
        
        if(file==NULL)//illegal file name
            return;
        
        fseek(file, 0L, SEEK_END); //move the ptr to the end of the file
         FileSize = ftell(file); //get the file length
         fseek(file, 0L, SEEK_SET); //move the ptr back to the beginning of the file
         
         fclose(file); //close the file
         file = fopen (FileName,"rb"); //reopen the file and reset the ptr

         strcpy(header,"HTTP/1.1 200 OK\r\n\\Content-TYPE: image/gif\r\n\\Content-Length: ");
         snprintf(temp, 20, "%d", FileSize); //convert the PostPort from int to string

         strcat(header,temp); //add the file size to the header
         strcat(header," \r\n\r\n"); //end the header

         write(sock,header,strlen(header)); //send the header


         while (1) //send the image file
            {
            ReadBytes = fread(img_buff,1,sizeof(img_buff),file); // read 1 item of size BUF_SIZE from file into buff 
            if (ReadBytes <= 0) break;		 // check for end of file 
            img_buff_ptr = img_buff;
            send(sock,img_buff_ptr,ReadBytes,0);		 // write 1 item of size BUF_SIZE from buff to sock 
            }  
         
         fclose(file);
        }
   
   
}//end of WebServer_TXT_Or_Image_GET_handler()

/*
 * Handles initial GET html requests
 * @param sock - socket id
 */
void WebServer_html_GET_handler (int sock)
{
   char header[60], buff[BUF_SIZE],temp[20];
   FILE* file;
   int FileSize=0;
   int ReadBytes=0,fd;
   
   file = fopen ("myTimeline.html","rt");
   if (file==NULL)
        {
        printf("HTML file missing! Exiting.\n");
        exit(1);
        }
   
   fseek(file, 0L, SEEK_END); //move the ptr to the end of the file
    FileSize = ftell(file); //get the file length
    fseek(file, 0L, SEEK_SET); //move the ptr back to the beginning of the file
    
    fclose(file);
    
    strcpy(header,"HTTP/1.1 200 OK\r\n\\Content-Length: ");
    snprintf(temp, 20, "%d", FileSize); //convert the length from int to string
    
    strcat(header,temp); //add the file size to the header
    strcat(header,"\r\n\r\n"); //end the header
    
    write(sock,header,strlen(header)); //send the header
   
        fd = open("myTimeline.html",O_RDONLY);
        
        while (1) //send the html file
                {
                ReadBytes = read(fd, buff, BUF_SIZE); /* read from file */
                if (ReadBytes <= 0) break;		 /* check for end of file */
                write(sock, buff, ReadBytes);		 /* write bytes to socket */
                }       
       
   
}//end of WebServer_html_GET_handler()

/*
 * Establishes a TCP connection with the tracker. Doesn't send any data.
 * param tracker_IP - a string with the given tracker IP
 * return - (-1) if the connection establishment failed, Socket number otherwise.
 */
int SetInitialConnectionWithTheTracker (char* tracker_IP)
{
  int connected_socket, sock;			
  struct hostent *h;			/* info about server */
  struct sockaddr_in channel;		/* holds IP address */
  
  h = gethostbyname(tracker_IP);		/* look up host's IP address */
  if (!h)
     {
      printf("gethostbyname() failed");
      return -1; //exit 
     }
  

  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket < 0)
        {
        printf("Tracker socket failed!\n");
        return -1; //exit
        }
  
  memset(&channel, 0, sizeof(channel));
  
  channel.sin_family= AF_INET;
  
  memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
  
  channel.sin_port= htons(TRACKER_PORT);
  
  connected_socket = connect(sock, (struct sockaddr *) &channel, sizeof(channel));
  if (connected_socket < 0)
        {
         return -1; //exit
        }
  
  return sock; //return the socket connected to the tracker
}//end of SetInitialConnectionWithTheTracker()

/*
 * Sends the Login message to the tracker. On reply, calls to PrintTrackerLoginReply().
 * param sock - the socket to the tracker
 * param UserName - a string with the user name 
 * return - 0 if the login failed, 1 if succeeded, 2 if the tracker reply was irrelevant
 */
int LoginWithTheTracker (int sock)
{
    char buffer[BUF_SIZE]; //a buffer that will be used to get data from the socket
    int NumOfReadBytes, isLoginSuccesful=2;
    char LoginMessage[60],PostPortString[10],temp[BUF_SIZE];
 
    
    _PostPort = 12312 + rand()%128; //Set a random PostPort for all UDP connections
    
    strcpy(LoginMessage,"LGIN");
    strcat(LoginMessage,_UserName);
    strcat(LoginMessage,"#");
    snprintf(PostPortString, 10, "%d", _PostPort); //convert the PostPort from int to string
    strcat(LoginMessage, PostPortString);
    strcat(LoginMessage,"\n");
    
    //Login string is ready, now we have to send it:
    write(sock, LoginMessage, strlen(LoginMessage));
    
    //Waiting for the tracker to respond:
    NumOfReadBytes = read(sock, buffer, BUF_SIZE);	/* read from socket (sleeps until data is received)*/
    
    strcpy (temp,buffer); //make a copy of the received data
    temp[4]='\0'; //shorten the string so that it'll contain only the header
    
    if (strcmp(temp,"EROR")==0) //if the received message is an error
        isLoginSuccesful=0;
    
     if (strcmp(temp,"LRES")==0) //if the received message is an error
        {
        isLoginSuccesful=1;
        strcpy(temp,buffer+4); //set temp to point to the beginning of the message
        }
    
    PrintTrackerLoginReply(isLoginSuccesful,temp); //print the tracker's reply
    
    return isLoginSuccesful;
    
    
    
}//end of LoginWithTheTracket()

/*
 * Prints a message according to the tracker's reply
 */
void PrintTrackerLoginReply (int result,char* message)
{
    if (result ==0)
        {
        printf("Message from the tracker: (Code - EROR)\n");
        printf("-----------------------------------------\n");
        printf("Login failed: username incorrect\n");
        printf("-----------------------------------------\n");
        }
      
    if (result ==1)
        {
        printf("Message from the tracker: (Code - LRES)\n");
        printf("-----------------------------------------\n");
        printf("%s\n",message);
        printf("-----------------------------------------\n");
        }
        
    if (result ==2)
        printf("Invalid reply message from the tracker");
        
          
}//end of PrintTrackerLoginReply()

/*
 * Creates the thread that will be responsible for keeping the 'UsersOnLine.txt' file up to date
 */
void CreateUsersOnlineUpdateThread (int* sock)
{
    pthread_t temp;
    
    if (pthread_create(&temp,NULL,UsersOnlineUpdater,sock))
        {
        printf("Error while creating a thread. Exiting.\n");
        exit(1);
        }

}//end of CreateUsersOnlineUpdateThread()

/*
 * This function is run by the thread that keeps the 'UsersOnLine.txt' file up to date
 */
void* UsersOnlineUpdater (void* _sock)
{
    int sock = *((int*) _sock); //make a copy of _sock
    char SingleMessage[60]={"\0"}; //will hold a single message received from the tracker
    char buffer[2]={"\0"}; //will be used to read a single byte at a time from the socket
    char WhoMessage[6], *Username;
    FILE *UsersOnLine, *Users;
    
     strcpy(WhoMessage,"WHO?\n"); //set the query string
    
    while (1)
        {
         write(sock, WhoMessage, strlen(WhoMessage)); //send a WHO? query
         SingleMessage[0]='\0';
         
          sem_wait(&UsersOnlineFileLocker); //lock the file
         
          UsersOnLine = fopen("UsersOnLine.txt","w"); //create a new UsersOnline.txt file
          Users = fopen("Users.txt","w"); //create a new Users.txt file. We use this to store ALL the user data from the tracker
          
          if(UsersOnLine==NULL || Users==NULL)
            {
            printf("Failed to create UsersOnLine.txt or Users.txt.\nRun the program as SU! Exiting!\n");
            exit(1);        
            }
 
         while (strlen(SingleMessage)!=5) //while the final segment hasn't been read
               {
                        
                   buffer[0]='\0'; //reset the recently read byte
                   SingleMessage[0]='\0'; //reset the single message

                   while (buffer[0]!='\n') //while we haven't reached the end of this message
                       {
                       read(sock, buffer, 1); //read a single byte
                       strcat(SingleMessage,buffer); //add the read char to the current message string
                       }//while (strcmp(buffer,"\n")!=0)

                   if(strlen(SingleMessage)!=5) //it's not the end of the segment
                       {
                       //printf ("Message from tracker to write to file: %s\n",SingleMessage);
                       sem_wait(&StrtokLocker); //lock
                       Username = strtok(SingleMessage+4,"#");
                       fprintf(UsersOnLine,"%s\n",Username); //write Username to UsersOnLine.txt
                       fprintf(Users,"%s ",Username); //write Username to Users.txt
                       fprintf(Users,"%s ",strtok(NULL,"#")); //write IP to Users.txt
                       fprintf(Users,"%s\n",strtok(NULL,"\n")); //write postPort to Users.txt
                       sem_post(&StrtokLocker); //unlock
                       }
                   else //it's the end of the segment
                       {
                       fclose(UsersOnLine); //close UsersOnLine.txt
                       fclose(Users); //close Users.txt
                       sem_post(&UsersOnlineFileLocker); //unlock the file
                       sleep(10);
                       }
             
                } //while (strlen(SingleMessage)!=5)
           
         
        }//while (1)
   
}//end of UsersOnlineUpdater()

/*
 * Creates the following 2 files:
 * MyUserID.txt - Created if necessary and overwritten with the current user ID
 * PostsOnMyWall.txt - Created if necessary. Never overwritten, only appended.
 */
void Create_MyUserID_and_PostsOnMyWall_Files ()
{
    FILE *file; //ptr to a file
    
    file = fopen("PostsOnMyWall.txt","a+"); /* apend file (add text to a file or create a file if it does not exist.*/
    fclose(file);
    
    file = fopen("MyUserID.txt","w"); //open the file for overwriting. Create it if it doesn't exist.
    fprintf(file,"%s",_UserName); //write the user id
    fclose(file);
}//end of Create_MyUserID_and_PostsOnMyWall_Files()

/*
 * Returns the length in bytes of a file named 'fileName'.
 * Returns -1 if it failed to read the file.
 */
int GetFileLength (char* fileName)
{
    FILE *file;
    int ans;
    
    file = fopen (fileName,"rt");
    if (file == NULL)
        return -1; //the file doesn't exist

    fseek(file, 0L, SEEK_END); //move the ptr to the end of the file
    ans = ftell(file); //get the file length
    fseek(file, 0L, SEEK_SET); //move the ptr back to the beginning of the file

    fclose(file);
    
    return ans;
}//end of GetFileLength()
