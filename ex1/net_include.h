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
#define NAME_LENGTH 80

#define FILE_NAME_LEN 80
#define PACKET_DATA_SIZE 1000
#define WIN_SIZE 500

#define READ_BUF_SIZE PACKET_DATA_SIZE*WIN_SIZE
#define ACK_BUF_SIZE WIN_SIZE

// Package types from receiver to sender
#define RTOS_START_CONN   '1'
#define RTOS_ACK_COMES    '2'
#define RTOS_CLOSE_CONN   '3'
#define RTOS_WAIT_CONN    '4'
#define RTOS_AWAKE        '5'

// Package types from sender to receiver
#define STOR_START_CONN   '0'
#define STOR_COMFIRM_CONN '1'
#define STOR_PACKET_COMES '2'
#define STOR_CLOSE_CONN   '3'

// Sender status types
#define SENDER_INIT_CONN       0
#define SENDER_DATA_TRANSFER   1
#define SENDER_CLOSE_CONN      2
#define SENDER_TERMINATE       3
#define SENDER_WAIT_CONN       4

// Receiver status
#define RECEIVER_FREE         -1
#define RECEIVER_START_CONN    0
#define RECEIVER_DATA_TRANSFER 1
#define RECEIVER_WAIT_NEXT     2

struct MSG {
  char type;
};

/* ~~~~~~~~~~~~~~~~~ Data Transfer struct ~~~~~~~~~~~~~~~~~ */
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

/* ~~~~~~~~~~~~~~~~~ Connection struct ~~~~~~~~~~~~~~~~~ */
/* Open Connection, Send to Receiver*/
struct OPEN_CONN_MSG {
  struct MSG msg;
  char filename[FILE_NAME_LEN];
};

/* Start Connection, Receiver to Sender */
struct START_CONN_MSG {
  struct MSG msg;
};

/* Confirm Connection from Sender */
struct CONFIRM_CONN_MSG {
  struct MSG msg;
};


/* ~~~~~~~~~~~~~~~~~ Close Connection struct ~~~~~~~~~~~~~~~~~ */
/* Close Connectin */
struct CLOSE_CONN_MSG {
  struct MSG msg;
};

struct WAIT_MSG {
  struct MSG msg;
};	

struct AWAKE_MSG {
  struct MSG msg;
};

/* ~~~~~~~~~~~~~~~~~ Sender Node struct ~~~~~~~~~~~~~~~~~ */
struct SENDER_NODE {
  struct sockaddr_in from_addr;
  struct SENDER_NODE* next;
};
