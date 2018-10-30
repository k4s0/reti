/*
 *  Simple udp client
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
   struct sockaddr_in si_other;
   int   s, slen = sizeof(si_other);
   char  buf[BUFLEN];
   char  message[BUFLEN];
   char *hostname;
   int   portno, stop = 0;

   /* check command line arguments */
   if (argc != 3)
   {
      fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
      exit(1);
   }
   hostname = argv[1];
   portno   = atoi(argv[2]);

   //create a UDP socket
   if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
   {
      die("Error When Create Socket");
   }

   memset((char *)&si_other, 0, sizeof(si_other));
   si_other.sin_family = AF_INET;
   si_other.sin_port   = htons(portno);

   if (inet_aton(hostname, &si_other.sin_addr) == 0)
   {
      fprintf(stderr, "inet_aton() Error!\n");
      exit(1);
   }

   while (!stop)
   {
      printf("Enter Message : ");
      fgets(message, BUFLEN, stdin);
      message[strlen(message) - 1] = '\0';

      //send the message
      if (sendto(s, message, strlen(message), 0, (struct sockaddr *)&si_other, slen) == -1)
      {
         die("sendto() Error!");
      }

      if (strcmp(message, "exit") == 0)
      {
         //receive data and print it to console
         //clear the buffer
         memset(buf, '\0', BUFLEN);
         //try to receive some data
         if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen) == -1)
         {
            die("recvfrom() Error!");
         }

         if (strcmp(buf, "GoodBye") == 0) //check the reply of the server
         {
            puts(buf);                    //print the server message
            puts("Client Shutting Down !");
            close(s);                     //close the socket
            stop = 1;
         }
      }
   }

   close(s);     //close the socket

   return(0);
}
