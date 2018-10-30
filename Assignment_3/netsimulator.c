#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <netinet/in.h>

/* ******************************************************************
   This code should be used for unidirectional data transfer protocols
   (from A to B)
   Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for Pipelined ARQ), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
  int seqnum;
  int acknum;
  int checksum;
  char payload[20];
};


/*- Your Definitions
  ---------------------------------------------------------------------------*/

/* Please use the following values in your program */

#define   A    0
#define   B    1
#define   FIRST_SEQNO   0
#define BUFFER 50

/*- Declarations ------------------------------------------------------------*/
void	restart_rxmt_timer(void);
void	tolayer3(int AorB, struct pkt packet);
void	tolayer5(char datasent[20]);

void	starttimer(int AorB, double increment);
void	stoptimer(int AorB);

/* WINDOW_SIZE, RXMT_TIMEOUT and TRACE are inputs to the program;
   We have set an appropriate value for LIMIT_SEQNO.
   You do not have to concern but you have to use these variables in your
   routines --------------------------------------------------------------*/

extern int WINDOW_SIZE;      // size of the window
extern int LIMIT_SEQNO;      // when sequence number reaches this value,                                     // it wraps around
extern double RXMT_TIMEOUT;  // retransmission timeout
extern int TRACE;            // trace level, for your debug purpose
extern double time_now;         // simulation time, for your debug purpose


// debug info trigger
// 1 = true show debug info
// 0 = false hide debug info
#define DEBUG    1

//Set the max dimension of the message recived from Layer5
#define PAYLOAD 19

// Declaring variables for statistics
int sender_pkt_counter;      // counter fot sender (A) , count the packets sent from Sender (A side)
int corrupt_pkt_counter; // count the corrupted packets received by Receiver (B side)
int rxmt_counter;       // count the retransmitted packets
int ok_pkt_counter;    // count the correct packet sent with the correct ACK return.

// Sender side (A)
struct sender
{
  int seq_num;
  int nextSeqNum;
  int nextMsg;
  struct msg *msg_buffer;
  struct pkt *packetBuffer;
  int msgCount;
  int buffer_dim;
}sender;

// Receiver side (B)
struct receiver
{
  int expectSeqNum;
  int lastAckNum;
  int first_iteration;
  struct pkt *received_pkt;
  int *sendedToLayer5;
  int lastSeqNum;
} receiver;

/********* YOU MAY ADD SOME ROUTINES HERE ********/
// Calculate checksum of packet
int calcChecksum(struct pkt packet)
{
  int checksum = 0;
  checksum += packet.seqnum;
  checksum += packet.acknum;
  for (int i = 0; i < 20; i++)
    checksum += packet.payload[i];
  return checksum;
}

// Check packet for errors
int isCorrupt(struct pkt packet)
{
  return calcChecksum(packet) - packet.checksum;
}
// Check if number is within window
int isWithinWindow(int base, int i)
{
  // Number is located on the right of base
  int right = i >= base && i < base + WINDOW_SIZE;

  // Number is located on the left of base
  int left = i < base && i + LIMIT_SEQNO < base + WINDOW_SIZE;

  return right || left;
}

/*Recursive function to calculate the correct ack.*/
int check_correct_ack(int exp)
{
  receiver.first_iteration++;
  int index = exp % WINDOW_SIZE;
  if (receiver.received_pkt[index].seqnum == exp)
  {
    return check_correct_ack((exp + 1) % LIMIT_SEQNO);
  }
  return exp;
}
/*Recursive function to send to layer 5 the correct packets.*/
void send_toLayer5(int exp)
{
  int i = exp % WINDOW_SIZE;
  if (receiver.received_pkt[i].seqnum == exp)
  {
    if (receiver.sendedToLayer5[i] == 0)
    {
      tolayer5(receiver.received_pkt[i].payload);
      receiver.sendedToLayer5[i] = 1;
    }
    send_toLayer5((exp + 1) % LIMIT_SEQNO);
  }
  return;
}
/********* STUDENTS WRITE THE NEXT SIX ROUTINES *********/

/* called from layer 5, passed the data to be sent to other side */
void
A_output (message)
  struct msg message;
{
  puts("A: Receiving MSG from layer 5...");
  #if DEBUG
  printf("####[DEBUG] DATA=> %.*s\n", 19, message.data);
  #endif

  // Add message to message buffer , icrement the counter for statistics
  sender.msg_buffer[sender.msgCount] = message;
  sender.msgCount++;
  sender.buffer_dim++;
  if (sender.buffer_dim > BUFFER)
  {
    puts("The Buffer is Full, exiting the program...");
    exit(0);
  }
  // Check if the message is inside the window
  if (isWithinWindow(sender.seq_num, sender.nextSeqNum))
  {
    struct pkt new_packet;

    // Create packet with the data from Layer5
    new_packet.seqnum = sender.nextSeqNum;
    new_packet.acknum = 0;
    strncpy(new_packet.payload, sender.msg_buffer[sender.nextMsg].data,PAYLOAD);
    new_packet.checksum = calcChecksum(new_packet);

    // Insert the new packet insdie the buffer
    sender.packetBuffer[sender.nextSeqNum] = new_packet;

    // Send packet to Layer3
    puts("A: Sending new DATA to B...");

    #if DEBUG
    printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
    printf("Packet Msg=> %.*s\n", 19, new_packet.payload);
    #endif

    tolayer3(A, new_packet);
    sender_pkt_counter++;

    // Start the timer if the packet is the first inside the window
    (sender.nextSeqNum == sender.seq_num)? starttimer(A, RXMT_TIMEOUT):0;
    // Set the next sequence number.
    sender.nextSeqNum = (sender.nextSeqNum + 1) % LIMIT_SEQNO;
    sender.nextMsg++;
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void
A_input(packet)
  struct pkt packet;
{
  puts("A: Receiving ACK from B...");

  #if DEBUG
  printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", packet.seqnum, packet.acknum);
  #endif

  // Check if the packet is corrupt and inside the window
  if (!isCorrupt(packet) && isWithinWindow(sender.seq_num, packet.acknum))
  {
    int window_shift;

    puts("A: Checking ACK from B...");
    // Stop the A side timer
    stoptimer(A);
    // Find the number of packet correctly acked.
    if (packet.acknum < sender.seq_num)
      window_shift = packet.acknum - sender.seq_num + LIMIT_SEQNO;
    else
      window_shift = packet.acknum - sender.seq_num;

    // Set the new seq_num for sender (A side)
    sender.seq_num = (packet.acknum + 1) % LIMIT_SEQNO;

    for (int i = 0; i < window_shift + 1; i++)
    {
      // if there are message inside the window send it to receiver (B side)
      if (sender.nextMsg < sender.msgCount)
      {
        // Create new packet
        struct pkt new_packet;
        new_packet.seqnum = sender.nextSeqNum;
        new_packet.acknum = 0;
        strncpy(new_packet.payload, sender.msg_buffer[sender.nextMsg].data,PAYLOAD);
        new_packet.checksum = calcChecksum(new_packet);

        // Add to packet buffer
        sender.packetBuffer[sender.nextSeqNum] = new_packet;

        // Send packet to receiver (B side)
        puts("A: Sending new DATA to B...");

        #if DEBUG
        printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
        printf("Packet Msg=> %.*s\n", 19, new_packet.payload);
        #endif
        tolayer3(A, new_packet);

        //Update the counter with the current number of packets sent
        sender_pkt_counter++;

        // Update next sequence number
        sender.nextSeqNum = (sender.nextSeqNum + 1) % LIMIT_SEQNO;
        sender.nextMsg++;
      }
      ok_pkt_counter++;
      sender.buffer_dim--;
    }

    // Set timer if there are some packets to send
    if (sender.seq_num != sender.nextSeqNum)
      starttimer(A, RXMT_TIMEOUT);
  }
  else
  { //Check if the packet is corrupt.
    if (isCorrupt(packet))
    {
      //Update the corrupt packets counter
      corrupt_pkt_counter++;
      puts("A: ACK is corrupted.");
    }
    // Rejecting packets
    printf("A: Rejecting ACK from B...  Waiting for new ACK %d\n", sender.seq_num);
  }
}

/* called when A's timer goes off */
void
A_timerinterrupt (void)
{
  struct pkt new_packet;
  int sender_seq_num = sender.seq_num;

  while (sender_seq_num != sender.nextSeqNum)
  {
    // Resend packet to receiver (B side)
    new_packet = sender.packetBuffer[sender_seq_num];
    puts("A: Resending DATA to B...");

    #if DEBUG
    printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
    printf("Packet Msg=> %.*s\n", 19, new_packet.payload);
    #endif

    tolayer3(A, new_packet);

    // Set the new value of the current for:
    //=> Packet sent
    //=> Retransmitted packets
    sender_pkt_counter++;
    rxmt_counter++;
    sender_seq_num = (sender_seq_num + 1) % LIMIT_SEQNO;
  }

  // Start new timer for sender (A side)
  starttimer(A, RXMT_TIMEOUT);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void
A_init (void)
{
  sender.seq_num = FIRST_SEQNO;
  sender.nextSeqNum = FIRST_SEQNO;
  // Init the statistics variables
  sender_pkt_counter = 0;
  corrupt_pkt_counter = 0;
  rxmt_counter = 0;
  ok_pkt_counter = 0;
  // Init the buffers and variables for sender side (A side)
  sender.nextMsg = 0;
  sender.msgCount = 0;
  sender.msg_buffer = (struct msg *)malloc(sizeof(struct msg) * BUFFER);
  sender.packetBuffer = (struct pkt *)malloc(sizeof(struct pkt) * LIMIT_SEQNO);
  sender.buffer_dim = 0;

}

/* called from layer 3, when a packet arrives for layer 4 at B*/
void
B_input (packet)
  struct pkt packet;
{
  puts("B: Receiving DATA from A...");

  #if DEBUG
  printf("Expected SEQ_NUM=> %d\n", receiver.expectSeqNum);
  printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", packet.seqnum, packet.acknum);
  printf("Packet Msg=> %.*s\n", 19, packet.payload);
  #endif
  struct pkt new_packet;
  // Check if the packets aren't Corrupted
  if (!isCorrupt(packet))
  {
    //Check the expected seqnum and is inside window
    if (packet.seqnum >= receiver.expectSeqNum && packet.seqnum < receiver.expectSeqNum + WINDOW_SIZE)
    {
      // insert the new sequence number inside the buffer
      receiver.sendedToLayer5[(packet.seqnum % WINDOW_SIZE)] = 0;
      receiver.received_pkt[(packet.seqnum % WINDOW_SIZE)].seqnum = packet.seqnum;
      strncpy(receiver.received_pkt[packet.seqnum % WINDOW_SIZE].payload, packet.payload, PAYLOAD);
      int index = (check_correct_ack(receiver.expectSeqNum)) % WINDOW_SIZE;

      if (receiver.first_iteration == 1)
      {
        new_packet.seqnum = 0;
        new_packet.acknum = receiver.lastAckNum;
        new_packet.checksum = calcChecksum(new_packet);
        // Send ACK packets to sender (A side)
        puts("B: Resending previous ACK to A...");
        #if DEBUG
        printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
        #endif
        tolayer3(B, new_packet);
      }
      else
      { // send the ack for the correct packet.
        if (index > 0)
          index--;
        else
          index = WINDOW_SIZE - 1;

        send_toLayer5((receiver.lastSeqNum + 1) % LIMIT_SEQNO);
        new_packet.seqnum = 0;
        new_packet.acknum = receiver.received_pkt[index].seqnum;

        new_packet.checksum = calcChecksum(new_packet);
        receiver.lastAckNum = new_packet.acknum;
        receiver.lastSeqNum = receiver.lastAckNum;
        // Update expected sequence number
        receiver.expectSeqNum = (new_packet.acknum + 1) % LIMIT_SEQNO;
        // Send packet to network
        puts("B: Sending ACK to A...");
        #if DEBUG
        printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
        #endif
        tolayer3(B, new_packet);
      }
    }
    else if (packet.seqnum < receiver.expectSeqNum && packet.seqnum < receiver.expectSeqNum - WINDOW_SIZE)
    {
      // Passed the max seq_num
      receiver.sendedToLayer5[packet.seqnum % WINDOW_SIZE] = 0;
      // insert the new seq_num into the buffer.
      receiver.received_pkt[(packet.seqnum % WINDOW_SIZE)].seqnum = packet.seqnum;
      strncpy(receiver.received_pkt[packet.seqnum % WINDOW_SIZE].payload, packet.payload,PAYLOAD);
      int index = check_correct_ack(receiver.expectSeqNum) % WINDOW_SIZE;
      send_toLayer5((receiver.lastSeqNum + 1) % LIMIT_SEQNO);
      new_packet.seqnum = 0;
      new_packet.acknum = receiver.received_pkt[index].seqnum;
      new_packet.checksum = calcChecksum(new_packet);
      receiver.lastAckNum = new_packet.acknum;
      receiver.lastSeqNum = receiver.lastAckNum;
      receiver.expectSeqNum = (new_packet.acknum + 1) % LIMIT_SEQNO; // update expected seq num.
      // Send ACK to sender (A side)
      puts("B: Sending ACK to A...");
      #if DEBUG
      printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
      #endif
      tolayer3(B, new_packet);
    }
    else
    {
      // Create ACK packet
      new_packet.seqnum = 0;
      new_packet.acknum = receiver.lastAckNum;
      new_packet.checksum = calcChecksum(new_packet);
      // Send packet to sender side (A side)
      puts("B: Resending previous ACK to A...");
      #if DEBUG
      printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
      #endif
      tolayer3(B, new_packet);
    }
    receiver.first_iteration = 0;
  }
  // The packet is corrupted
  else
  {
    // Update corrupted packet counter
    corrupt_pkt_counter++;
    // Create ACK packet
    new_packet.seqnum = 0;
    new_packet.acknum = receiver.lastAckNum;
    new_packet.checksum = calcChecksum(new_packet);
    // Send packet to sender side (A side)
    puts("B: Resending previous ACK to A...");
    #if DEBUG
    printf("Packet SEQ_NUM=> %d\nPacket ACK=> %d\n", new_packet.seqnum, new_packet.acknum);
    #endif
    tolayer3(B, new_packet);
  }
}


/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void
B_init (void)
{
  // State variables
  receiver.expectSeqNum = FIRST_SEQNO;
  receiver.lastAckNum = FIRST_SEQNO - 1 < 0 ? LIMIT_SEQNO - 1 : FIRST_SEQNO - 1;
  receiver.received_pkt = (struct pkt *)malloc(sizeof(struct pkt) * WINDOW_SIZE);
  receiver.sendedToLayer5 = malloc(sizeof(int) * WINDOW_SIZE);
  receiver.first_iteration = 0;
  for (int i = 0; i < WINDOW_SIZE; i++)
  {
    receiver.received_pkt[i].seqnum = -1;
    receiver.sendedToLayer5[i] = 0;
  }
  receiver.lastSeqNum = -1;
}

// Called at end of simulation to print final statistics
void Simulation_done()
{
  puts(" ");
  puts("==================================");
  puts("==   END SIMULATION STATISTICS  ==");
  puts("==================================");
  puts(" ");
  puts("----------------------------------");
  printf("|  Messages arrived to A:  %d    |\n", sender.msgCount);
  printf("|  Packets sent from A:    %d    |\n", sender_pkt_counter);
  printf("|  Packets retransmitted:   %d     |\n", rxmt_counter);
  printf("|  Correct acks:            %d     |\n", ok_pkt_counter);
  printf("|  Corrupted packets:       %d     |\n", corrupt_pkt_counter);
  puts("----------------------------------");
}
/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event
{
  double evtime;      /* event time */
  int evtype;         /* event type code */
  int eventity;       /* entity where event occurs */
  struct pkt *pktptr; /* ptr to packet (if any) assoc w/ this event */
  struct event *prev;
  struct event *next;
};
struct event *evlist = NULL; /* the event list */

/* Advance declarations. */
void init(void);
void generate_next_arrival(void);
void insertevent(struct event *p);

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER5 1
#define FROM_LAYER3 2

#define OFF 0
#define ON 1

int TRACE = 0; /* for debugging purpose */
int fileoutput;
double time_now = 0.000;
int WINDOW_SIZE;
int LIMIT_SEQNO;
double RXMT_TIMEOUT;
double lossprob;    /* probability that a packet is dropped  */
double corruptprob; /* probability that one bit is packet is flipped */
double lambda;      /* arrival rate of messages from layer 5 */
int ntolayer3;      /* number sent into layer 3 */
int nlost;          /* number lost in media */
int ncorrupt;       /* number corrupted by media*/
int nsim = 0;
int nsimmax = 0;
unsigned int seed[5]; /* seed used in the pseudo-random generator */

int main(int argc, char **argv)
{
  struct event *eventptr;
  struct msg msg2give;
  struct pkt pkt2give;

  int i, j;

  init();
  A_init();
  B_init();

  while (1)
  {
    eventptr = evlist; /* get next event to simulate */
    if (eventptr == NULL)
      goto terminate;
    evlist = evlist->next; /* remove this event from event list */
    if (evlist != NULL)
      evlist->prev = NULL;
    if (TRACE >= 2)
    {
      printf("\nEVENT time: %f,", eventptr->evtime);
      printf("  type: %d", eventptr->evtype);
      if (eventptr->evtype == 0)
        printf(", timerinterrupt  ");
      else if (eventptr->evtype == 1)
        printf(", fromlayer5 ");
      else
        printf(", fromlayer3 ");
      printf(" entity: %d\n", eventptr->eventity);
    }
    time_now = eventptr->evtime; /* update time to next event time */
    if (eventptr->evtype == FROM_LAYER5)
    {
      generate_next_arrival(); /* set up future arrival */
      /* fill in msg to give with string of same letter */
      j = nsim % 26;
      for (i = 0; i < 20; i++)
        msg2give.data[i] = 97 + j;
      msg2give.data[19] = '\n';
      nsim++;
      if (nsim == nsimmax + 1)
        break;
      A_output(msg2give);
    }
    else if (eventptr->evtype == FROM_LAYER3)
    {
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      for (i = 0; i < 20; i++)
        pkt2give.payload[i] = eventptr->pktptr->payload[i];
      if (eventptr->eventity == A) /* deliver packet by calling */
        A_input(pkt2give);         /* appropriate entity */
      else
        B_input(pkt2give);
      free(eventptr->pktptr); /* free the memory for packet */
    }
    else if (eventptr->evtype == TIMER_INTERRUPT)
    {
      A_timerinterrupt();
    }
    else
    {
      printf("INTERNAL PANIC: unknown event type \n");
    }
    free(eventptr);
  }
terminate:
  printf("Simulator terminated at time %.12f\n", time_now);
  Simulation_done();
  return (0);
}

void init(void) /* initialize the simulator */
{
  int i = 0;
  printf("----- * ARQ Network Simulator Version 1.1 * ------ \n\n");
  printf("Enter number of messages to simulate: ");
  scanf("%d", &nsimmax);
  printf("Enter packet loss probability [enter 0.0 for no loss]:");
  scanf("%lf", &lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%lf", &corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%lf", &lambda);
  printf("Enter window size [>0]:");
  scanf("%d", &WINDOW_SIZE);
  LIMIT_SEQNO = 2 * WINDOW_SIZE;
  printf("Enter retransmission timeout [> 0.0]:");
  scanf("%lf", &RXMT_TIMEOUT);
  printf("Enter trace level:");
  scanf("%d", &TRACE);
  printf("Enter random seed: [>0]:");
  scanf("%d", &seed[0]);
  /*nsimmax=2;
  lossprob=0.0;
  corruptprob=0.0;
  lambda=10;
  LIMIT_SEQNO=8;
  RXMT_TIMEOUT=20;
  TRACE=2;
  seed[0]=2233;*/
  for (i = 1; i < 5; i++)
    seed[i] = seed[0] + i;
  fileoutput = open("OutputFile", O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fileoutput < 0)
    exit(1);
  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;
  time_now = 0.0;          /* initialize time to 0.0 */
  generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* mrand(): return a double in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
int nextrand(int i)
{
  seed[i] = seed[i] * 1103515245 + 12345;
  return (unsigned int)(seed[i] / 65536) % 32768;
}

double mrand(int i)
{
  double mmm = 32767;
  double x;              /* individual students may need to change mmm */
  x = nextrand(i) / mmm; /* x should be uniform in [0,1] */
  if (TRACE == 0)
    printf("%.16f\n", x);
  return (x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
void generate_next_arrival(void)
{
  double x, log(), ceil();
  struct event *evptr;

  if (TRACE > 2)
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

  x = lambda * mrand(0) * 2; /* x is uniform on [0,2*lambda] */
  /* having mean of lambda        */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time_now + x;
  evptr->evtype = FROM_LAYER5;
  evptr->eventity = A;
  insertevent(evptr);
}

void
    insertevent(p) struct event *p;
{
  struct event *q, *qold;

  if (TRACE > 2)
  {
    printf("            INSERTEVENT: time is %f\n", time_now);
    printf("            INSERTEVENT: future time will be %f\n", p->evtime);
  }
  q = evlist; /* q points to header of list in which p struct inserted */
  if (q == NULL)
  { /* list is empty */
    evlist = p;
    p->next = NULL;
    p->prev = NULL;
  }
  else
  {
    for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
      qold = q;
    if (q == NULL)
    { /* end of list */
      qold->next = p;
      p->prev = qold;
      p->next = NULL;
    }
    else if (q == evlist)
    { /* front of list */
      p->next = evlist;
      p->prev = NULL;
      p->next->prev = p;
      evlist = p;
    }
    else
    { /* middle of list */
      p->next = q;
      p->prev = q->prev;
      q->prev->next = p;
      q->prev = p;
    }
  }
}

void printevlist(void)
{
  struct event *q;
  printf("--------------\nEvent List Follows:\n");
  for (q = evlist; q != NULL; q = q->next)
  {
    printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
  }
  printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void
    stoptimer(AorB) int AorB; /* A or B is trying to stop timer */
{
  struct event *q /* ,*qold */;
  if (TRACE > 2)
    printf("          STOP TIMER: stopping timer at %f\n", time_now);
  for (q = evlist; q != NULL; q = q->next)
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
    {
      /* remove this event */
      if (q->next == NULL && q->prev == NULL)
        evlist = NULL;          /* remove first and only event on list */
      else if (q->next == NULL) /* end of list - there is one in front */
        q->prev->next = NULL;
      else if (q == evlist)
      { /* front of list - there must be event after */
        q->next->prev = NULL;
        evlist = q->next;
      }
      else
      { /* middle of list */
        q->next->prev = q->prev;
        q->prev->next = q->next;
      }
      free(q);
      return;
    }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void
    starttimer(AorB, increment) int AorB; /* A or B is trying to stop timer */
double increment;
{
  struct event *q;
  struct event *evptr;

  if (TRACE > 2)
    printf("          START TIMER: starting timer at %f\n", time_now);
  /* be nice: check to see if timer is already started, if so, then  warn */
  /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
  for (q = evlist; q != NULL; q = q->next)
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
    {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }

  /* create future event for when timer goes off */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time_now + increment;
  evptr->evtype = TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}

/************************** TOLAYER3 ***************/
void tolayer3(AorB, packet) int AorB; /* A or B is trying to stop timer */
struct pkt packet;
{
  struct pkt *mypktptr;
  struct event *evptr, *q;
  double lastime, x;
  int i;

  ntolayer3++;

  /* simulate losses: */
  if (mrand(1) < lossprob)
  {
    nlost++;
    if (TRACE > 0)
      printf("          TOLAYER3: packet being lost\n");
    return;
  }

  /* make a copy of the packet student just gave me since he/she may decide */
  /* to do something with the packet after we return back to him/her */
  mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
  mypktptr->seqnum = packet.seqnum;
  mypktptr->acknum = packet.acknum;
  mypktptr->checksum = packet.checksum;
  for (i = 0; i < 20; i++)
    mypktptr->payload[i] = packet.payload[i];
  if (TRACE > 2)
  {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
           mypktptr->acknum, mypktptr->checksum);
  }

  /* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
  evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */
  /* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
  lastime = time_now;
  for (q = evlist; q != NULL; q = q->next)
    if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
      lastime = q->evtime;
  evptr->evtime = lastime + 1 + 9 * mrand(2);

  /* simulate corruption: */
  if (mrand(3) < corruptprob)
  {
    ncorrupt++;
    if ((x = mrand(4)) < 0.75)
      mypktptr->payload[0] = '?'; /* corrupt payload */
    else if (x < 0.875)
      mypktptr->seqnum = 999999;
    else
      mypktptr->acknum = 999999;
    if (TRACE > 0)
      printf("          TOLAYER3: packet being corrupted\n");
  }

  if (TRACE > 2)
    printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
}

void tolayer5(datasent) char datasent[20];
{
  write(fileoutput, datasent, 20);
}
