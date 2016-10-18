#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>

#define PORT_MULTI_CAST	  10580
#define PORT_UNI_CAST     10270

#define MAX_MESS_LEN 10000
#define NAME_LENGTH 80
#define DUMMY_DATA_LENGTH 1300
#define NACK_LIST_LEN 1000
#define SEND_DATA_WIN_SIZE 10
#define WRITE_THRESHOLD 1000
#define RECV_CONTENT_LEN WRITE_THRESHOLD*3
#define MAX_MACHINE_NUM 20

#define RAND_MAX_NUM 1000000

/* Type of packets */
#define START_MCAST 's'  // Starting signal sent by start_mcast
#define INIT_MCAST  'i'  // Initial packet sent by each mcast process
#define MCAST       'm'  // Multicast    
#define TOKEN_RING  't'  // Transfer token ring

/* Type of ring message type */
#define CHECK_IP_RECEIVED 'c' // Check if machine got the IP address of next one
#define PASS_PACK         'p' // multi-cast package

/* Status number for mcast machines */
#define WAIT_START_SIGNAL 0
#define RECEIVED_START_SIGNAL 1
#define CHECK_RECV_IP 2
#define DO_MCAST 4

#define RECV_WAIT_TIME 0.001
#define TOKEN_PASS_TIME 0.01

struct MSG {
  char type;
};

struct START_MSG {
  struct MSG msg;
};

struct INIT_MSG {
  struct MSG msg;
  int machine_index;
  struct sockaddr_in addr;
};

struct MULTI_CAST_CONTENT {
  int machine_index;
  int packet_index;
  int rand_number;
};

struct MULTI_CAST_MSG {
  struct MSG msg;
  struct MULTI_CAST_CONTENT content;
  char dummyData[DUMMY_DATA_LENGTH];
};

struct RING_MSG {
  struct MSG msg;
  char type;
  int no;
};
      
struct CHECK_IP_RING_MSG {
  struct RING_MSG ring_msg;
};

struct MULTI_CAST_RING_MSG {
  struct RING_MSG ring_msg;
  int seq;
  int aru;
  int machine_index; /* the machine that lower the aru in token */
  int nack_list[NACK_LIST_LEN];
  int send_pack_num[MAX_MACHINE_NUM];
  int ready_to_terminate[MAX_MACHINE_NUM];
};
