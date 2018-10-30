/*
 *  Simple udp server
 */
#include <stdio.h>  //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFLEN    512 //Max length of buffer

void die(char *s)
{
   perror(s);
   exit(1);
}

int main(int argc, char **argv)
{
   struct sockaddr_in si_me, si_other;

   int   s, slen = sizeof(si_other), recv_len;
   char  buf[BUFLEN];
   char *hostname;
   int   portno = 0;

   /*
    * check command line arguments
    */
   if (argc != 3)
   {
      fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
      exit(0);
   }
   hostname = argv[1];
   portno   = atoi(argv[2]);

   //create a UDP socket
   if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
   {
      die("Error When Create Socket");
   }

   // zero out the structure
   memset((char *)&si_me, 0, sizeof(si_me));
   //ip and port assignment for the server instance.
   si_me.sin_family      = AF_INET;
   si_me.sin_port        = htons(portno);
   si_me.sin_addr.s_addr = inet_addr(hostname);

   //bind socket to port
   if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
   {
      die("Error When Binding Socket Port");
   }

   //listening for incoming data
   while (1)
   {
      printf("Waiting for data...\n");
      fflush(stdout);

      //clear the buffer
      memset(buf, '\0', BUFLEN);

      //sleep for testing asking the task_3 quetsion
      //sleep(10);

      //wait some data from client
      if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen)) == -1)
      {
         die("recvfrom() Error!");
      }

      //print details of the client ip and port
      printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
      puts(buf);

      //read the string received from the client and check if cointains "exit"
      if (strcmp(buf, "exit") == 0)
      {
         //send GoodBye to the client and close the connection
         if (sendto(s, strcpy(buf, "GoodBye"), 7, 0, (struct sockaddr *)&si_other, slen) == -1)
         {
            die("sendto() Error!");
         }
      }
   }

   close(s);

   return(0);
}
