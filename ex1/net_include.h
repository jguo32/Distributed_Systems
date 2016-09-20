#include <stdio.h>

#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>

#include <errno.h>

#define PORT	     10270

#define MAX_MESS_LEN 1400
#define BUF_SIZE 100

#define NAME_LENGTH 80
#define PACKET_DATA_SIZE 80
#define WIN_SIZE 5

#define READ_BUF_SIZE PACKET_DATA_SIZE*WIN_SIZE
#define CONN_BUF_SIZE NAME_LENGTH+1
#define ACK_BUF_SIZE WIN_SIZE


struct MSG {
  char type;
};

/* Data Transfer: Msg from Sender to Receiver */
struct STOR_MSG {
  struct MSG msg;
  int packageNo;
  char data[PACKET_DATA_SIZE]; 
};

/* Data Transfer: Msg from Receiver to Sender */
struct RTOS_MSG {
  struct MSG msg;
  int ackNo;
};

/* Open Connection */
struct OPEN_CONN_MSG {
  struct MSG msg;
  char filename[CONN_BUF_SIZE];
};

/* Close Connectin */
struct CLOSE_CON_MSG {
  struct MSG msg;
};
