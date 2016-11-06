#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_MESSLEN       102400
#define MAX_MEMBERS       100
#define MAX_PROCESSES     30
#define WIN_SIZE          500
#define DUMMY_DATA_LENGTH 1200
#define RAND_MAX_NUM      1000000
#define FINISH_SENDING    'f'
#define REGULAR_MSG       'r'

#define MIN(x, y) (x > y ? y : x)

static	char	User[80];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;

struct CONTENT {
  int process_index;
  int message_index;
  int random_number;
};

struct MSG {
  char type;
};

struct FINISH_MSG {
  struct MSG msg;
  int process_index;
};

struct MCAST_MSG {
  struct MSG msg;
  struct CONTENT content;
  int last_packet;     /* last packet in this send round */
  char dummy_data[DUMMY_DATA_LENGTH];
};

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  // exit(0);
}

int main( int argc, char *argv[] )
{
  int ret;
  int round;
  int send_seq;
  int aru;
  int last_aru;
  int finished_send;
  int exit;

  struct timespec start_time, end_time;
  double elapsed_time;

  FILE *fw;
  
  char            mess[MAX_MESSLEN];
  char            sender[MAX_GROUP_NAME];
  char            target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
  membership_info memb_info;
  int             service_type;
  int             num_groups;
  int             endian_mismatch;
  int16           mess_type;

  struct CONTENT recv_buf[MAX_PROCESSES][WIN_SIZE];
  int end_processes[MAX_PROCESSES];
  int finished_processes[MAX_PROCESSES];
  
  /* Command line input parameters */
  int num_of_messages;
  int process_index;
  int num_of_processes;
  
  if (argc != 4) {
    printf("Usage: mcast <num_of_messages> <process_index> "
	   "<num_of_processes> \n");
    // exit(0);
  }

  num_of_messages = atoi(argv[1]);
  process_index = atoi(argv[2]);
  num_of_processes = atoi(argv[3]);

  /* init output file */
  char file_name[20];
  sprintf(file_name, "%d.out", process_index);
  printf("Open file: %s\n", file_name);

  if ((fw = fopen(file_name, "w")) == NULL) {
    perror("fopen error");
    // exit(0);
  }

  sprintf(User, "yx_jy");
  sprintf(Spread_name, "4803");
  
  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret != ACCEPT_SESSION) {
    SP_error(ret);
    Bye();
  }

  ret = SP_join(Mbox, "group");
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }

  printf("Waiting for other processes to join the group.\n");
  /* wait for all the processes have joined the group */
  while (1) {
    ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups,
		     &mess_type, &endian_mismatch, sizeof(mess), mess);
    if (ret < 0) {
      SP_error(ret);
      Bye();
    }
    if (Is_membership_mess(service_type)) {
      ret = SP_get_memb_info(mess, service_type, &memb_info);
      if (ret < 0) {
	SP_error(ret);
	Bye();
      }

      int membNum = memb_info.gid.id[2];
      if (membNum >= num_of_processes)
	break;
    }
  }

  printf("Every processes have joined the group!\nStart to transmit packages.\n");
  
  round = 0;
  send_seq = 0;
  aru = 0;
  last_aru = 0;
  finished_send = 0;
  exit = 1;

  clock_gettime(CLOCK_MONOTONIC, &start_time);
	 
  while (exit) {

    memset(end_processes, -1, MAX_PROCESSES * sizeof(int));
    
    // send step
    if (!finished_send) {
      // printf("send\n");
      if (send_seq == num_of_messages) {
	// printf("send finish\n");

	struct FINISH_MSG finish_msg;
	finish_msg.msg.type = FINISH_SENDING;
	finish_msg.process_index = process_index;
	
	ret = SP_multicast(Mbox, AGREED_MESS, "group", 0, sizeof(finish_msg), (char *)&finish_msg);
	finished_send = 1;
	
      } else {
	
	int sendNum = MIN(WIN_SIZE, num_of_messages - send_seq);

	for (int i = 0; i < sendNum; i ++) {
	            
	  struct MCAST_MSG mcast_msg;
	  mcast_msg.msg.type = REGULAR_MSG;
	  mcast_msg.content.process_index = process_index;
	  mcast_msg.content.message_index = send_seq;
	  mcast_msg.content.random_number = ((rand() + 1) % RAND_MAX_NUM);
	  mcast_msg.last_packet = (i == sendNum - 1);

	  ret = SP_multicast(Mbox, AGREED_MESS, "group", 0, sizeof(mcast_msg), (char *)&mcast_msg);

	  /*
	  printf("processs_i: %d, msg_i: %d, num: %d, last: %d\n",  process_index, send_seq,
		 mcast_msg.content.random_number, mcast_msg.last_packet);
	  */
	  
	  send_seq += 1;
	}
      }
    }
    
    // recv step
    // printf("recv\n");
    int totalNum = -1;
    int recvNum = 0;
    while (totalNum == -1 || recvNum < totalNum){
      
      ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups,
		       &mess_type, &endian_mismatch, sizeof(mess), mess);

      struct MSG msg;
      memcpy(&msg, mess, sizeof(msg));

      // printf("type: %c\n", msg.type);
      
      if (msg.type == FINISH_SENDING) {

	struct FINISH_MSG finish_msg;
	memcpy(&finish_msg, mess, sizeof(finish_msg));
	finished_processes[finish_msg.process_index - 1] = 1;
	// printf("end of process: %d\n", finish_msg.process_index);
	
      } else if (msg.type == REGULAR_MSG) {

	struct MCAST_MSG mcast_msg;
	memcpy(&mcast_msg, mess, sizeof(mcast_msg));
	recv_buf[mcast_msg.content.process_index - 1][mcast_msg.content.message_index - round * WIN_SIZE] = mcast_msg.content;
	
	if (mcast_msg.last_packet) {
	  end_processes[mcast_msg.content.process_index - 1] = (mcast_msg.content.message_index - round * WIN_SIZE);
	  // printf("last packet: %d\n", mcast_msg.content.message_index);
	}

	/*
	printf("process: process_i: %d, msg_i: %d, num: %d\n", mcast_msg.content.process_index,
	       mcast_msg.content.message_index, mcast_msg.content.random_number); 
	*/

	recvNum += 1;
      }

      if (totalNum == -1) {
	totalNum = 0;
	for (int i = 0; i < num_of_processes; i ++) {
	  if (finished_processes[i] == 1){
	    continue;
	  } else if (end_processes[i] != -1) {
	    totalNum += (end_processes[i] + 1);
	  } else {
	    totalNum = -1;
	    break;
	  }
	}
	// printf("totalNum: %d\n", totalNum);
      }
      
    }

    for (int i = 0; i < num_of_processes; i ++) {
      // printf("end num: %d\n", end_processes[i]);
      for (int n = 0; n <= end_processes[i]; n ++) {
   	fprintf(fw, "%2d, %8d, %8d\n", recv_buf[i][n].process_index, aru, recv_buf[i][n].random_number);
	aru += 1;

	if (aru%1000 == 0 && aru > last_aru) {
	  printf("Received %d packets.\n", aru);
	  last_aru = aru;
	}
	
      }
    }
    
    int count = 0;
    for (int i = 0; i < num_of_processes; i ++) {
      if (finished_processes[i] == 1) count ++;
    }

    if (count == num_of_processes)
      exit = 0;
    
    round ++;
  }

  clock_gettime(CLOCK_MONOTONIC, &end_time);
  elapsed_time = (end_time.tv_sec - start_time.tv_sec);
  elapsed_time += (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

  printf("%.2fs: %d packages received.\n", elapsed_time, aru);
  
  printf("Packages transmission complete!\n");
  printf("Bye.\n");
  SP_disconnect(Mbox);
  
  fclose(fw);
  
  return(0);
}


