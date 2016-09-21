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

#define FILE_NAME_LEN 20
#define PACKET_DATA_SIZE 10
#define WIN_SIZE 5

#define READ_BUF_SIZE PACKET_DATA_SIZE*WIN_SIZE
#define ACK_BUF_SIZE WIN_SIZE

#define RTOS_START_CONN   '1'
#define RTOS_ACK_COMES    '2'
#define RTOS_CLOSE_CONN   '3'

#define STOR_START_CONN   '0'
#define STOR_COMFIRM_CONN '1'
#define STOR_PACKET_COMES '2'
#define STOR_CLOSE_CONN   '3'

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
