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

#include "net_include.h"
#include "recv_dbg.h"
#include "recv_dbg.c"

#define MIN(x, y) (x > y ? y : x)

void printIP(int ip);

int main(int argc, char **argv) {
  struct sockaddr_in recv_multi_addr;
  struct sockaddr_in send_multi_addr;

  struct sockaddr_in recv_uni_addr;
  struct sockaddr_in send_uni_addr; // ip of next machine
  struct sockaddr_in self_uni_addr;
  struct sockaddr_in first_uni_addr; // ip of first machine

  
  struct hostent        h_ent;
  struct hostent        *p_h_ent;
  char                  my_name[NAME_LENGTH] = {'\0'};
  int                   my_ip;
  
  int mcast_addr; 

  struct ip_mreq mreq;
  unsigned char ttl_val;

  int ss_multi, sr_multi;
  int ss_uni, sr_uni;
  fd_set mask;
  fd_set dummy_mask, temp_mask;
  struct timeval timeout;
  
  int bytes;
  int num;
  char mess_buf[MAX_MESS_LEN];
  int status;
  int last_safe_aru = PRINT_PACKET_GAP;
  
  int tokenNo;
  int haveToken = 0;
  int local_aru = 0;
  int safe_aru = 0;
  int send_seq = 0;
  int sent_pack_num = 0;
  int ready_to_terminate[MAX_MACHINE_NUM];
  int check_terminate_times = 0;
  int terminate = 0;
  
  int clear_times = 0;

  struct MULTI_CAST_RING_MSG last_token_ring;
  struct MULTI_CAST_RING_MSG multi_cast_ring_msg;
  struct MULTI_CAST_CONTENT recvData[RECV_CONTENT_LEN];
  int recvDataCheck[RECV_CONTENT_LEN];
  int terminate_machine[MAX_MACHINE_NUM];

  FILE *fw;
  
  /* create random number generator */
  time_t t;
  srand((unsigned) time(&t));
  
  /* Command line input parameters */
  int num_of_packets;
  int machine_index;
  int num_of_machines;
  int loss_rate;

  if (argc != 5) {
    printf("Usage: mcast <num_of_packets> <machine_index> "
           "<num_of_machines> <loss_rate>\n");
    // exit(0);
  }

  num_of_packets = atoi(argv[1]);
  machine_index = atoi(argv[2]);
  num_of_machines = atoi(argv[3]);
  loss_rate = atoi(argv[4]);
  
  /* init file */
  char file_name[20];
  sprintf(file_name, "%d.out", machine_index);
  printf("Open file: %s\n", file_name);
  
  if ((fw = fopen(file_name, "w")) == NULL) {
    perror("fopen error");
    exit(0);
  }

  memset(ready_to_terminate, -1, MAX_MACHINE_NUM * sizeof(int));

  /* init recv_dbg */
  recv_dbg_init(loss_rate, machine_index);
  
  /* init status */
  status = WAIT_START_SIGNAL;
  
  /* get host name and ip */
  gethostname(my_name, NAME_LENGTH );
  printf("My host name is %s.\n", my_name);

  p_h_ent = gethostbyname(my_name);
  if ( p_h_ent == NULL ) {
    printf("myip: gethostbyname error.\n");
    exit(1);
  }

  memcpy( &h_ent, p_h_ent, sizeof(h_ent));
  memcpy( &my_ip, h_ent.h_addr_list[0], sizeof(my_ip) );

  printIP(my_ip);
  /* get local ip addr*/
  self_uni_addr.sin_family = AF_INET;
  self_uni_addr.sin_addr.s_addr = my_ip;
  self_uni_addr.sin_port = htons(PORT_UNI_CAST);

  if (machine_index == 1) {
    first_uni_addr = self_uni_addr;
  }

  /* init unicast send socket */
  ss_uni = socket(AF_INET, SOCK_DGRAM, 0);
  if (ss_uni < 0) {
    perror("socket error");
    exit(1);
  }

  /* init unicast recv socket */  
  sr_uni = socket(AF_INET, SOCK_DGRAM, 0);
  if (sr_uni < 0) {
    perror("socket error");
    exit(1);
  }

  /* init unicast recv addr */
  recv_uni_addr.sin_family = AF_INET;
  recv_uni_addr.sin_addr.s_addr = INADDR_ANY;
  recv_uni_addr.sin_port = htons(PORT_UNI_CAST);

  if (bind(sr_uni, (struct sockaddr *)&recv_uni_addr, sizeof(recv_uni_addr)) < 0) {
    perror("bind error");
    exit(1);
  }

  /* init multicast recv socket */
  mcast_addr = 225 << 24 | 0 << 16 | 1 << 8 | 1; /* (225.0.1.1) */

  sr_multi = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
  if (sr_multi < 0) {
    perror("Mcast: socket");
    exit(1);
  }

  /* init multicast recv addr */
  recv_multi_addr.sin_family = AF_INET;
  recv_multi_addr.sin_addr.s_addr = INADDR_ANY;
  recv_multi_addr.sin_port = htons(PORT_MULTI_CAST);

  if (bind(sr_multi, (struct sockaddr *)&recv_multi_addr, sizeof(recv_multi_addr)) < 0) {
    perror("Mcast: bind");
    exit(1);
  }

  mreq.imr_multiaddr.s_addr = htonl(mcast_addr);

  /* the interface could be changed to a specific interface if needed */
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(sr_multi, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq,
                 sizeof(mreq)) < 0) {
    perror("Mcast: problem in setsockopt to join multicast address");
  }

  /* init multicast send socket */
  ss_multi = socket(AF_INET, SOCK_DGRAM, 0); 
  
  if (ss_multi < 0) {
    perror("Mcast: socket");
    exit(1);
  }
  
  ttl_val = 1;
  if (setsockopt(ss_multi, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl_val,
                 sizeof(ttl_val)) < 0) {
    printf("Mcast: problem in setsockopt of multicast ttl %d - ignore in WinNT "
           "or Win95\n",
           ttl_val);
  }

  /* init multicast send addr */
  send_multi_addr.sin_family = AF_INET;
  send_multi_addr.sin_addr.s_addr = htonl(mcast_addr); /* mcast address */
  send_multi_addr.sin_port = htons(PORT_MULTI_CAST);

   /* init token ring */
  tokenNo = machine_index == 1 ? 0 : -1; /* only use for check ip */

  multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
  multi_cast_ring_msg.ring_msg.type = PASS_PACK;
  multi_cast_ring_msg.ring_msg.no = 0;
  
  if (machine_index == 1) {
      
    multi_cast_ring_msg.aru = MIN(num_of_packets, SEND_DATA_WIN_SIZE);
    multi_cast_ring_msg.seq = multi_cast_ring_msg.aru;
    multi_cast_ring_msg.machine_index = -1;
    
    memcpy(multi_cast_ring_msg.ready_to_terminate, ready_to_terminate,
			   MAX_MACHINE_NUM * sizeof(int));
    memset(multi_cast_ring_msg.nack_list, -1, NACK_LIST_LEN * sizeof(int));
    memset(multi_cast_ring_msg.send_complete, 0, MAX_MACHINE_NUM * sizeof(int));
	
    multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
    multi_cast_ring_msg.ring_msg.no = 1;
    multi_cast_ring_msg.ring_msg.type = PASS_PACK;

    // local_aru = multi_cast_ring_msg.aru;
    
  } else {
    multi_cast_ring_msg.aru = 0;
  }
    
  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr_multi, &mask);
  FD_SET(sr_uni, &mask);
  // FD_SET((long)0, &mask); /* stdin */

  /*
    printf("ss_multi: %d\n", ss_multi);
    printf("sr_multi: %d\n", sr_multi);
    printf("ss_uni: %d\n", ss_uni);
    printf("sr_uni: %d\n", sr_uni);
  */
  printf("Machine Index: %d\n",machine_index);
  printf("Waiting for start ...\n");

  int lastStatus = status;

  struct timespec tokenStartTime, tokenEndTime;
  double tokenTotalTime, tokenElapsedTime;
  tokenTotalTime = tokenElapsedTime = TOKEN_PASS_TIME;
  
  struct timespec recvStartTime, recvEndTime;
  double recvTotalTime, recvElapsedTime;
  recvTotalTime = TOKEN_PASS_TIME;

  struct timespec closeStartTime, closeEndTime;
  double closeTotalTime, closeElapsedTime;
  closeTotalTime = WAIT_END_TIME;

  struct timespec mcastStartTime, mcastEndTime;
  double  mcastElapsedTime;
  
  for (;;) {

    // printf("~~~~~~~~\n");
    
    if (lastStatus != status) {
      //printf("status: %d\n", status);
      lastStatus = status;
    }

    if (status == RECEIVED_START_SIGNAL) {

      struct INIT_MSG init_msg;
      char init_buf[sizeof(init_msg)];
      init_msg.msg.type = INIT_MCAST;
      init_msg.machine_index = machine_index;
      init_msg.addr = self_uni_addr;
      memcpy(init_buf, &init_msg, sizeof(init_msg));
      sendto(ss_multi, init_buf, sizeof(init_buf), 0,
	     (struct sockaddr *)&send_multi_addr,sizeof(send_multi_addr));
      
    } else if (status == CHECK_RECV_IP) {

      /* machine 1 will keep multicast it's ip addr until 
	 it receives token ring from last machine */
      
      if (machine_index == 1) {
	struct INIT_MSG init_msg;
	char init_buf[sizeof(init_msg)];
	init_msg.msg.type = INIT_MCAST;
	init_msg.machine_index = machine_index;
	init_msg.addr = self_uni_addr;
	memcpy(init_buf, &init_msg, sizeof(init_msg));
	sendto(ss_multi, init_buf, sizeof(init_buf), 0,
	       (struct sockaddr *)&send_multi_addr,sizeof(send_multi_addr));
      }

      //printIP(send_uni_addr.sin_addr.s_addr);
      struct CHECK_IP_RING_MSG check_ip_msg;
      char check_ip_buf[sizeof(check_ip_msg)];
      check_ip_msg.ring_msg.msg.type = TOKEN_RING;
      check_ip_msg.ring_msg.no = tokenNo + 1;
      check_ip_msg.ring_msg.type = CHECK_IP_RECEIVED;
      memcpy(check_ip_buf, &check_ip_msg, sizeof(check_ip_msg));
      sendto(ss_uni, check_ip_buf, sizeof(check_ip_buf), 0,
	     (struct sockaddr *)&send_uni_addr,sizeof(send_uni_addr));
    
    } else if (status == DO_MCAST || status == READY_TO_CLOSE) {

      if (haveToken) {

	/* multicast its content */
	sent_pack_num += multi_cast_ring_msg.seq - send_seq;
	/*
	printf("send packets from %d to %d, %d packets sent out\n",
	send_seq, multi_cast_ring_msg.seq, sent_pack_num);*/
	
	int i = send_seq;

	struct timespec StartTime, EndTime;
	double ElapsedTime;
	clock_gettime(CLOCK_MONOTONIC, &StartTime);
	int send_count = 0;
	
	for (; i<multi_cast_ring_msg.seq; i++) {
	  struct MULTI_CAST_CONTENT multi_cast_content;
	  multi_cast_content.machine_index = machine_index;
	  multi_cast_content.packet_index = i;
	  multi_cast_content.rand_number = ((rand()+1) % RAND_MAX_NUM);
	  send_count ++;
	  /* copy the data to local */

	  /*
	  printf("send packets from %d to %d, %d packets sent out\n",
	  send_seq, multi_cast_ring_msg.seq, sent_pack_num); */
	  
	  int pos = i - clear_times * CLEAR_THRESHOLD;

	  /*
	  printf("seq : %d, pos : %d\n", i, pos);
	  printf("safe aru : %d\n", safe_aru);
	  printf("clear times : %d\n", clear_times);
	  */
	  
	  recvData[pos] = multi_cast_content;
	  recvDataCheck[pos] = 1;
	  
	  struct MULTI_CAST_MSG multi_cast_msg;
	  char multi_cast_buf[sizeof(multi_cast_msg)];
	  multi_cast_msg.msg.type = MCAST;
	  multi_cast_msg.content = multi_cast_content;

	  /* multicast content */
	  memcpy(multi_cast_buf, &multi_cast_msg, sizeof(multi_cast_msg));
	  sendto(ss_multi, multi_cast_buf, sizeof(multi_cast_buf), 0,
		 (struct sockaddr *)&send_multi_addr,sizeof(send_multi_addr));
	}
	
	/*  multicast the packet in nack list (if machine has it) */ 

	// printf("send packet in nack list\n");
	
	int nack_list[NACK_LIST_LEN];
	memset(nack_list, -1, NACK_LIST_LEN * sizeof(int));
	
	i = 0;
	int j = 0;
	while (i < NACK_LIST_LEN && multi_cast_ring_msg.nack_list[i] != -1) {
	  int nack = multi_cast_ring_msg.nack_list[i];
	  if (nack < safe_aru) {
	    i ++;
	    continue;
	  }
	  // printf("%d ", nack);
	  /* copy the new nack (filter out unecessary nacks which smaller than safe_aru */
	  nack_list[j++] = nack;
	
	  int pos = nack - clear_times * CLEAR_THRESHOLD;
	  if (recvDataCheck[pos] == 1) {
	    /* machine has this data */

	    struct MULTI_CAST_MSG multi_cast_msg;
	    char multi_cast_buf[sizeof(multi_cast_msg)];
	    multi_cast_msg.msg.type = MCAST;
	    multi_cast_msg.content = recvData[pos];

	    /* multicast content */
	    memcpy(multi_cast_buf, &multi_cast_msg, sizeof(multi_cast_msg));
	    sendto(ss_multi, multi_cast_buf, sizeof(multi_cast_buf), 0,
		   (struct sockaddr *)&send_multi_addr,sizeof(send_multi_addr));

	    send_count ++;
	  }
	  
	  i ++;
	}

	clock_gettime(CLOCK_MONOTONIC, &EndTime);
	ElapsedTime = (EndTime.tv_sec - StartTime.tv_sec);
	ElapsedTime += (EndTime.tv_nsec - StartTime.tv_nsec) / 1000000000.0;
	// printf("used time : %f, send count : %d\n", ElapsedTime, send_count);

	// printf("nack list length : %d\n", j);
	
	/* add the nack to nack list, update related parameter
	   from local_aru to multi_cast_ring_msg.seq */

	// printf("pack nack list\n");
	// printf("nack\n");
	i = local_aru;
	for (; i < multi_cast_ring_msg.seq; i ++) {
	  int pos = i - clear_times * CLEAR_THRESHOLD;
	  if (recvDataCheck[pos] != 1) {
	    nack_list[j++] = i;
	    // printf("%d ", i);
	  }
	}
	// printf("\n");

	memcpy(multi_cast_ring_msg.nack_list, nack_list, NACK_LIST_LEN * sizeof(int));
	memcpy(multi_cast_ring_msg.ready_to_terminate, ready_to_terminate,
	       MAX_MACHINE_NUM * sizeof(int));

	// printf("send token\n");

	/* test */
	/*
	printf("send token\n");
	i = 0;
	for (; i<num_of_machines; i++) {
	  int num = multi_cast_ring_msg.send_pack_num[i];
	  printf("num : %d\n", num);
	}*/
	
	char multi_cast_ring_buf[sizeof(multi_cast_ring_msg)];

    	memcpy(multi_cast_ring_buf, &multi_cast_ring_msg,
	       sizeof(multi_cast_ring_msg));

	/* send out two duplicate token ring */
	int n = 0; 
	while (n++ < 3) {
	  sendto(ss_uni, multi_cast_ring_buf, sizeof(multi_cast_ring_buf), 0,
		 (struct sockaddr *)&send_uni_addr,sizeof(send_uni_addr));
	}

	// printf("sent out token\n");
	/* reset total time for pass token */
	haveToken = 0;
	tokenElapsedTime = 0.0;
	clock_gettime(CLOCK_MONOTONIC, &tokenStartTime);
	
      } else {
	/* check if time out, if it does, resend token */

	/* check timeout first */
	clock_gettime(CLOCK_MONOTONIC, &tokenEndTime);
	tokenElapsedTime = (tokenEndTime.tv_sec - tokenStartTime.tv_sec);
	tokenElapsedTime += (tokenEndTime.tv_nsec - tokenStartTime.tv_nsec) / 1000000000.0;

	if (tokenElapsedTime >= tokenTotalTime) {

	  // printf("resend token\n");
	  /* resend current token */
	  char multi_cast_ring_buf[sizeof(multi_cast_ring_msg)];
	  memcpy(multi_cast_ring_buf, &multi_cast_ring_msg, sizeof(multi_cast_ring_msg));
	  sendto(ss_uni, multi_cast_ring_buf, sizeof(multi_cast_ring_buf), 0,
		 (struct sockaddr *)&send_uni_addr,sizeof(send_uni_addr));

	  /* reset total time for pass token */
	  tokenElapsedTime = 0.0;
	  clock_gettime(CLOCK_MONOTONIC, &tokenStartTime);
	}
      }
    } else if (status == NOTIFY_TO_CLOSE) {

      /* machine 1 will multicast terminate meesage to every other machines */
      struct MULTI_CAST_CLOSE_MSG multi_cast_terminate_msg;
      multi_cast_terminate_msg.msg.type = CLOSE;
      multi_cast_terminate_msg.first_uni_addr = first_uni_addr;

      char multi_cast_terminate_buf[sizeof(multi_cast_terminate_msg)];
      memcpy(multi_cast_terminate_buf, &multi_cast_terminate_msg, sizeof(multi_cast_terminate_msg));

      sendto(ss_multi, multi_cast_terminate_buf, sizeof(multi_cast_terminate_buf), 0,
	     (struct sockaddr *)&send_multi_addr,sizeof(send_multi_addr));
          
    } else {
      // printf("error : unclear about the status!\n");
    }

    // Receive msg packet
    recvElapsedTime = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &recvStartTime);
    // printf("recv start sec : %1ld, nsec : %.9ld \n", recvStartTime.tv_sec, recvStartTime.tv_nsec);
    // printf("get \n");

    for (;;) {

      timeout.tv_sec = 0;
      timeout.tv_usec = (recvTotalTime - recvElapsedTime) * 1000000;
      num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
      
      temp_mask = mask;
      
      if (status == WAIT_START_SIGNAL) {
	num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
      } else {
	num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
      }
     
      if (num > 0) {

	/* check if the message from uni cast */
	if (FD_ISSET(sr_uni, &temp_mask)) {
	  // printf("get uni \n");
	  struct MSG recv_msg;
	  
	  if (status == WAIT_START_SIGNAL) {
	    bytes = recv(sr_uni, mess_buf, sizeof(mess_buf), 0);
	  } else {
	    bytes = recv_dbg(sr_uni, mess_buf, sizeof(mess_buf), 0);
	  }
	  mess_buf[bytes] = 0;
	  memcpy(&recv_msg, mess_buf, sizeof(recv_msg));

	  // printf("unicast, type:%c\n", recv_msg.type);

	  /* check the type is Token ring */
	  if (recv_msg.type == TOKEN_RING) {
	    
	    struct RING_MSG ring_msg; 
	    memcpy(&ring_msg, mess_buf, sizeof(ring_msg));

	    if (ring_msg.type == CHECK_IP_RECEIVED) {
	      
	      if (status == RECEIVED_START_SIGNAL || status == CHECK_RECV_IP) {
	    
		// printf("get token, type:%c\n", ring_msg.type);
		// printf("get check receive ip token, no:%d\n", ring_msg.no);
		//return;
		status = CHECK_RECV_IP; 
	    
		struct CHECK_IP_RING_MSG check_ip_msg; 
		memcpy(&check_ip_msg, mess_buf, sizeof(check_ip_msg));
		
		if (tokenNo == -1 || 
		    tokenNo + num_of_machines == check_ip_msg.ring_msg.no) {
		  if (machine_index == 1) {
		    /* check ip token pass around */
		    // printf("machine 1 start to perform multicast,\n");
		    status = DO_MCAST;
		    haveToken = 1;
		  } else {
		    tokenNo = check_ip_msg.ring_msg.no;
		  }
		} 	    
	      } 
	    } else if (ring_msg.type == PASS_PACK) {
	      /* get token ring for permission of multicast */
	    
	      if (status == CHECK_RECV_IP || status == DO_MCAST) {
		status = DO_MCAST;
	      }

	      if (status == DO_MCAST) {
		/* check local last token number, 
		   then check if it owns the permission */

		// printf("get token\n");
		if (multi_cast_ring_msg.ring_msg.no >= ring_msg.no) {
		  /* machine has used this token number, only pass token ring */
		  // printf("already got this token.\n");
		  tokenElapsedTime = tokenTotalTime;

		} else {
		  
		  if (multi_cast_ring_msg.ring_msg.no == 0 ||
		      multi_cast_ring_msg.ring_msg.no + num_of_machines - 1 == ring_msg.no) {

		    haveToken = 1;

		    struct MULTI_CAST_RING_MSG multi_cast_ring_recv_msg;
		    memcpy(&multi_cast_ring_recv_msg, mess_buf, sizeof(multi_cast_ring_recv_msg));
		    safe_aru = MIN(multi_cast_ring_msg.aru, multi_cast_ring_recv_msg.aru);
		    
		    int aru = multi_cast_ring_recv_msg.aru;
		    
		    if (multi_cast_ring_recv_msg.machine_index == -1 ||
			multi_cast_ring_recv_msg.machine_index == machine_index) {
		      if (multi_cast_ring_recv_msg.aru == local_aru) {
			aru = local_aru + MIN(SEND_DATA_WIN_SIZE, num_of_packets - sent_pack_num);
		      } else if (multi_cast_ring_recv_msg.aru > local_aru) {
			aru = local_aru; /* lower local_aru */
			multi_cast_ring_msg.machine_index = machine_index;
		      } else {
			aru = multi_cast_ring_recv_msg.aru;
		      }
		    } else {
		      aru = local_aru;
		      multi_cast_ring_msg.machine_index = -1;
		    }
		    		    		    
		    send_seq = multi_cast_ring_recv_msg.seq;

		    int send_seq_end = send_seq +
		      MIN(SEND_DATA_WIN_SIZE, num_of_packets - sent_pack_num);

		    send_seq_end = (send_seq_end - safe_aru >= NACK_LIST_LEN/2 ?
				    send_seq : send_seq_end);
		   
		    multi_cast_ring_msg.aru = aru;
		    multi_cast_ring_msg.seq = send_seq_end;

		    memcpy(multi_cast_ring_msg.nack_list, multi_cast_ring_recv_msg.nack_list,
			   NACK_LIST_LEN * sizeof(int));
		    memcpy(ready_to_terminate, multi_cast_ring_recv_msg.ready_to_terminate,
			   MAX_MACHINE_NUM * sizeof(int));
		    memcpy(multi_cast_ring_msg.send_complete, multi_cast_ring_recv_msg.send_complete,
			   MAX_MACHINE_NUM * sizeof(int));

		    multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
		    multi_cast_ring_msg.ring_msg.no = multi_cast_ring_recv_msg.ring_msg.no + 1;
		    multi_cast_ring_msg.ring_msg.type = PASS_PACK;
		   
		    /* check if the packets are completely sent */
		    if (sent_pack_num >= num_of_packets) {
		      multi_cast_ring_msg.send_complete[machine_index-1] = 1;

		      /* continue to check if it could be terminated as every other machines 
			 received all packets */
		      
		      int count = 0;
		      int i = 0;
		      for (; i < num_of_machines; i++) {
			if (multi_cast_ring_msg.send_complete[machine_index-1] == 1) {
			  count ++;
			}
		      }
		      
		      if (count == num_of_machines &&
			  last_token_ring.seq == multi_cast_ring_recv_msg.seq &&
			  safe_aru == last_token_ring.seq) {
			ready_to_terminate[machine_index-1] = 1;
		      }
		    }

		    memcpy(&last_token_ring, &multi_cast_ring_recv_msg,
			   sizeof(struct MULTI_CAST_RING_MSG));
		 
		    /* test */
		    /*
		      printf("bb : %d\n", multi_cast_ring_msg.bb[0]);
		      printf("size : %d\n", sizeof(multi_cast_ring_recv_msg));
		      printf("get token\n");
		      printf("seq: %d\n", multi_cast_ring_msg.seq);
		      printf("aru: %d\n", aru);
		      printf("nack[0] : %d\n", multi_cast_ring_msg.nack_list[0]);
		    
		      int i = 0;
		      for (; i<num_of_machines; i++) {
		      int num = multi_cast_ring_recv_msg.send_pack_num[i];
		      printf("num : %d\n", num);
		      }
		    */
		   
		  } else {
		    
		  }
		  
		}
		
	      }
	      
	    } else if (ring_msg.type == GET_CLOSE){

	      struct CLOSE_CONFIRMATION_MSG close_confirm_msg;
	      memcpy(&close_confirm_msg, mess_buf, sizeof(close_confirm_msg));
	      int index = close_confirm_msg.machine_index;

	      terminate_machine[index - 1] = 1;

	      int i = 0;
	      int terminate_count;
	      for (; i<num_of_machines; i++) {
		if (terminate_machine[index-1] == 1) {
		  terminate_count ++;
		}
	      }

	      if (terminate_count == num_of_machines)
		terminate = 1;
	    }
	    
	  } 

	  /* Check if the message from multicast */
	} else if (FD_ISSET(sr_multi, &temp_mask)) {

	  // printf("get multi");
	  
	  struct MSG recv_msg;
	  
	  bytes = recv(sr_multi, mess_buf, sizeof(mess_buf), 0);
	  //mess_buf[bytes] = 0;
	  memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	  //printf("multicast, type:%c\n", recv_msg.type);
	  
	  if (recv_msg.type == START_MCAST) {
	    
	    // printf("Machine %d received start_mcast msg.\n", machine_index);
	    clock_gettime(CLOCK_MONOTONIC, &mcastStartTime);
	    status = RECEIVED_START_SIGNAL;
	    
	  } else if (recv_msg.type == INIT_MCAST) {

	    if (status == RECEIVED_START_SIGNAL) {
	      struct INIT_MSG init_msg;
	      memcpy(&init_msg, mess_buf, sizeof(init_msg));

	      /* Determine the index of next machine */
	      int next_index = (machine_index == num_of_machines ?
				1 :machine_index + 1);
           
	      // printIP(init_msg.addr.sin_addr.s_addr);
	      if (init_msg.machine_index == next_index) {
		// printf("Received next machine IP address.\n");
		// printIP(init_msg.addr.sin_addr.s_addr);

		send_uni_addr = init_msg.addr;
	      
		/* pass token ring (for check) starts from machine 1 */
		if (machine_index == 1) {
		  status = CHECK_RECV_IP;
		}
	      } else {
		/* printf("irrelevant init packet from machine %d, msg type %c.\n",
		   init_msg.machine_index, init_msg.msg.type); */
	      }
	    }
	    
	  } else if (recv_msg.type == MCAST) {
	   
	    struct MULTI_CAST_MSG multi_cast_msg; 
	    memcpy(&multi_cast_msg, mess_buf, sizeof(multi_cast_msg));
	   	    
	    if (status == CHECK_RECV_IP) {
	      status = DO_MCAST;
	    }
	    
	    if (status == DO_MCAST) {
	      /* save message data */
	      struct MULTI_CAST_CONTENT multi_cast_content;
	      multi_cast_content = multi_cast_msg.content;

	      /* copy the data to local */

	      int pos = multi_cast_content.packet_index - clear_times * CLEAR_THRESHOLD;
	      if (pos >= 0) {
		recvData[pos] = multi_cast_content;
		recvDataCheck[pos] = 1;
	      }
	      	      
	      // printf("move aru\n");
	      // printf("%d ", multi_cast_content.packet_index);
	      /* move aru */
	      while (recvDataCheck[local_aru - clear_times * CLEAR_THRESHOLD] == 1) {
		/* write data to file */
		struct MULTI_CAST_CONTENT content = recvData[local_aru - clear_times * CLEAR_THRESHOLD];
		fprintf(fw, "%2d, %8d, %8d\n", content.machine_index,
			content.packet_index, content.rand_number);

		local_aru ++;
	      }

	      // printf("finish move aru\n");
	      // printf("current local_aru: %d\n", local_aru);
	    } 
	   	    
	  } else if (recv_msg.type == CLOSE) {

	    struct MULTI_CAST_CLOSE_MSG multi_cast_terminate_msg;
	    memcpy(&multi_cast_terminate_msg, mess_buf, sizeof(multi_cast_terminate_msg));
	    first_uni_addr = multi_cast_terminate_msg.first_uni_addr;
	    
	    /* send back terminate confirmation */
	    
	    struct CLOSE_CONFIRMATION_MSG close_confirm_msg;
	    close_confirm_msg.machine_index = machine_index;
	    
	    char close_confirm_buf[sizeof(close_confirm_msg)];
	    memcpy(close_confirm_buf, &close_confirm_msg,
		   sizeof(close_confirm_msg));

	    /* send out two duplicate token ring */
	    int n = 0; 
	    while (n++ < 3) {
	      sendto(ss_uni, close_confirm_buf, sizeof(close_confirm_buf), 0,
		     (struct sockaddr *)&first_uni_addr,sizeof(first_uni_addr));
	    }

	    terminate = 1;
	  }
	}	
      }
 
      clock_gettime(CLOCK_MONOTONIC, &recvEndTime);
      recvElapsedTime = (recvEndTime.tv_sec - recvStartTime.tv_sec);
      recvElapsedTime += (recvEndTime.tv_nsec - recvStartTime.tv_nsec) / 1000000000.0;

      /*
	printf("recv start sec : %1ld, nsec : %.9ld \n", recvStartTime.tv_sec, recvStartTime.tv_nsec);
	printf("recv end   sec : %1ld, nsec : %.9ld \n", recvEndTime.tv_sec, recvEndTime.tv_nsec);
      
	printf("recv elapse time : %f\n", recvElapsedTime);
	printf("recv total time  : %f\n", recvTotalTime);
      */
      
      if (recvElapsedTime > recvTotalTime)
	break;
    }

    if (safe_aru >= last_safe_aru) {
      // printf("~~~~~~~~~~~~~~~\n");
      clock_gettime(CLOCK_MONOTONIC, &mcastEndTime);
      mcastElapsedTime = (mcastEndTime.tv_sec - mcastStartTime.tv_sec);
      mcastElapsedTime += (mcastEndTime.tv_nsec - mcastStartTime.tv_nsec) / 1000000000.0;
      printf("%.2fs: %d packages received\n", mcastElapsedTime, safe_aru / PRINT_PACKET_GAP * 10000);
      mcastElapsedTime = 0.0;
      last_safe_aru = last_safe_aru + PRINT_PACKET_GAP;
    }

    // check if clear first 1000 data
    if (safe_aru > (clear_times + 1) * CLEAR_THRESHOLD) {
      clear_times ++;
      // printf("move\n");
      memcpy(recvData, recvData + CLEAR_THRESHOLD,
	     (RECV_CONTENT_LEN - CLEAR_THRESHOLD) * sizeof(struct MULTI_CAST_CONTENT));
      memset(recvData + (RECV_CONTENT_LEN - CLEAR_THRESHOLD), 0,
	     CLEAR_THRESHOLD * sizeof(struct MULTI_CAST_CONTENT));

      memcpy(recvDataCheck, recvDataCheck + CLEAR_THRESHOLD,
	     (RECV_CONTENT_LEN - CLEAR_THRESHOLD) * sizeof(int));
      memset(recvDataCheck + (RECV_CONTENT_LEN - CLEAR_THRESHOLD), 0,
	     CLEAR_THRESHOLD * sizeof(int));
      // printf("after move\n");
    }

    // printf("check : %d\n", check);
    // printf("total number of packet : %d\n", total_pack_num);

    // printf("local aru : %d\n", local_aru);
    // printf("safe aru : %d\n", safe_aru);

    /* check if all the machines could be terminated */
    int machine_count = 0;
    int i = 0;
    for (; i<num_of_machines; i++) {
      int num = ready_to_terminate[i];
      if (num == 1) {
	machine_count ++;
      }
    }
    
    if (machine_count == num_of_machines) {
      if (machine_index == 1) {
	if (check_terminate_times < 2) {

	  check_terminate_times ++;
	  
	  if (check_terminate_times >= 2) {
	    status = NOTIFY_TO_CLOSE;
	    clock_gettime(CLOCK_MONOTONIC, &closeStartTime);
	    closeElapsedTime = 0.0;
	  }
	}
      } else {
	status = READY_TO_CLOSE;
      }     
    }

    /* check timeout */
    if (status == CLOSE) {
      clock_gettime(CLOCK_MONOTONIC, &closeEndTime);
      closeElapsedTime = (closeEndTime.tv_sec - closeStartTime.tv_sec);
      closeElapsedTime += (closeEndTime.tv_nsec - closeStartTime.tv_nsec) / 1000000000.0;

      if (closeElapsedTime > closeTotalTime)
	terminate = 1;
    }
    
    if (terminate)
      break;
 
  }

  fclose(fw);

  clock_gettime(CLOCK_MONOTONIC, &mcastEndTime);
  mcastElapsedTime = (mcastEndTime.tv_sec - mcastStartTime.tv_sec);
  mcastElapsedTime += (mcastEndTime.tv_nsec - mcastStartTime.tv_nsec) / 1000000000.0;

  printf("Packages transfer complete, total time: %.2fs.\n", mcastElapsedTime);
  
  return 0;
}

void printIP(int ip) {
  printf("IP address is: %d.%d.%d.%d\n", (htonl(ip) & 0xff000000)>>24, 
	 (htonl(ip) & 0x00ff0000)>>16,
	 (htonl(ip) & 0x0000ff00)>>8,
	 (htonl(ip) & 0x000000ff) );
}
