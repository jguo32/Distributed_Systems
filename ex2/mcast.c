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

#define MIN(x, y) (x > y ? y : x)

void printIP(int ip);

int main(int argc, char **argv) {
  struct sockaddr_in recv_multi_addr;
  struct sockaddr_in send_multi_addr;

  struct sockaddr_in recv_uni_addr;
  struct sockaddr_in send_uni_addr; //ip of next machine
  struct sockaddr_in self_uni_addr;

  
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

  int tokenNo;
  int haveToken = 0;
  int local_aru = 0;
  int safe_aru = 0;
  int send_seq = 0;
  int sent_pack_num = 0;
  int send_pack_nums[MAX_MACHINE_NUM];
    
  int clear_times = 0;

  struct MULTI_CAST_RING_MSG last_token_ring;
  struct MULTI_CAST_RING_MSG multi_cast_ring_msg;
  struct MULTI_CAST_CONTENT recvData[RECV_CONTENT_LEN];
  int recvDataCheck[RECV_CONTENT_LEN];

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
  printf("open file: %s\n", file_name);
  
  if ((fw = fopen(file_name, "w")) == NULL) {
    perror("fopen error");
    exit(0);
  }

  memset(send_pack_nums, -1, MAX_MACHINE_NUM * sizeof(int));
  
  /* init token ring */

  tokenNo = machine_index == 1 ? 0 : -1; /* only use for check ip */

  multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
  multi_cast_ring_msg.ring_msg.type = PASS_PACK;
  multi_cast_ring_msg.ring_msg.no = 0;
  
  if (machine_index == 1) {
    send_pack_nums[0] = num_of_packets;
      
    multi_cast_ring_msg.aru = MIN(num_of_packets, SEND_DATA_WIN_SIZE);
    multi_cast_ring_msg.seq = multi_cast_ring_msg.aru;
    multi_cast_ring_msg.machine_index = -1;
    memset(multi_cast_ring_msg.nack_list, -1, NACK_LIST_LEN * sizeof(int));
    memcpy(multi_cast_ring_msg.send_pack_num, send_pack_nums, MAX_MACHINE_NUM * sizeof(int));
	
    multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
    multi_cast_ring_msg.ring_msg.no = 1;
    multi_cast_ring_msg.ring_msg.type = PASS_PACK;

    // local_aru = multi_cast_ring_msg.aru;
    
  } else {
    multi_cast_ring_msg.aru = 0;
  }

  last_token_ring = multi_cast_ring_msg;

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

  int lastStatus = status;

  struct timespec tokenStartTime, tokenEndTime;
  double tokenTotalTime, tokenElapsedTime;
  tokenTotalTime = tokenElapsedTime = TOKEN_PASS_TIME;
  
  struct timespec recvStartTime, recvEndTime;
  double recvTotalTime, recvElapsedTime;
  recvTotalTime = TOKEN_PASS_TIME;

  for (;;) {

    printf("~~~~~~~~\n");
    
    if (lastStatus != status) {
      printf("status: %d\n", status);
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
    
    } else if (status == DO_MCAST) {

      if (haveToken) {

	/* multicast its content */
	sent_pack_num += multi_cast_ring_msg.seq - send_seq;
	printf("send packets from %d to %d, %d packets sent out\n",
	       send_seq, multi_cast_ring_msg.seq, sent_pack_num);
	int i = send_seq;
	for (; i<multi_cast_ring_msg.seq; i++) {
	  struct MULTI_CAST_CONTENT multi_cast_content;
	  multi_cast_content.machine_index = machine_index;
	  multi_cast_content.packet_index = i;
	  multi_cast_content.rand_number = ((rand()+1) % RAND_MAX_NUM);

	  /* copy the data to local */
	  int pos = i - clear_times * WRITE_THRESHOLD;
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

	  /* copy the new nack (filter out unecessary nacks which smaller than safe_aru */
	  nack_list[j++] = nack;

	  int pos = nack - clear_times * WRITE_THRESHOLD;
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

	  }
	  
	  i ++;
	}
			
	/* add the nack to nack list, update related parameter
	   from local_aru to multi_cast_ring_msg.seq */
	i = local_aru;
	for (; i < multi_cast_ring_msg.seq; i ++) {
	  int pos = i - clear_times * WRITE_THRESHOLD;
	  if (recvDataCheck[i] != 1) {
	    nack_list[j++] = i;
	  }
	}

	memcpy(multi_cast_ring_msg.nack_list, nack_list, NACK_LIST_LEN * sizeof(int));
	multi_cast_ring_msg.send_pack_num[machine_index-1] = num_of_packets;
	// printf("sent out token\n");

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
	sendto(ss_uni, multi_cast_ring_buf, sizeof(multi_cast_ring_buf), 0,
	       (struct sockaddr *)&send_uni_addr,sizeof(send_uni_addr));

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
    } else {
      
    }

    // Receive msg packet
    recvElapsedTime = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &recvStartTime);
    
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

	  struct MSG recv_msg;	  	  
	  bytes = recv(sr_uni, mess_buf, sizeof(mess_buf), 0);
	  // mess_buf[bytes] = 0;
	  memcpy(&recv_msg, mess_buf, sizeof(recv_msg));

	  // printf("unicast, type:%c\n", recv_msg.type);

	  /* check the type is Token ring */
	  if (recv_msg.type == TOKEN_RING) {

	    struct RING_MSG ring_msg; 
	    memcpy(&ring_msg, mess_buf, sizeof(ring_msg));

	    if (ring_msg.type == CHECK_IP_RECEIVED) {
	      
	      if (status == RECEIVED_START_SIGNAL || status == CHECK_RECV_IP) {
	    
		// printf("get token, type:%c\n", ring_msg.type);
		printf("get check receive ip token, no:%d\n", ring_msg.no);
		//return;
		status = CHECK_RECV_IP; 
	    
		struct CHECK_IP_RING_MSG check_ip_msg; 
		memcpy(&check_ip_msg, mess_buf, sizeof(check_ip_msg));
		
		if (tokenNo == -1 || 
		    tokenNo + num_of_machines == check_ip_msg.ring_msg.no) {
		  if (machine_index == 1) {
		    /* check ip token pass around */
		    printf("machine 1 start to perform multicast,\n");
		    status = DO_MCAST;
		    haveToken = 1;
		  } else {
		    tokenNo = check_ip_msg.ring_msg.no;
		  }
		} 	    
	      } 
	    } else if (ring_msg.type == PASS_PACK) {
	      /* get token ring for permission of multicast */
	      printf("get multicast token, no : %d\n", ring_msg.no);
	      if (status == CHECK_RECV_IP || status == DO_MCAST) {
		status = DO_MCAST;
	      }

	      if (status == DO_MCAST) {
		/* check local last token number, 
		   then check if it owns the permission */
		
		if (last_token_ring.ring_msg.no >= ring_msg.no) {
		  /* machine has used this token number, only pass token ring */
		  // printf("already got this token.\n");
		  tokenElapsedTime = tokenTotalTime;

		} else {
		  
		  if (last_token_ring.ring_msg.no == 0 ||
		      last_token_ring.ring_msg.no + num_of_machines - 1 == ring_msg.no) {

		    haveToken = 1;

		    struct MULTI_CAST_RING_MSG multi_cast_ring_recv_msg;
		    memcpy(&multi_cast_ring_recv_msg, mess_buf, sizeof(multi_cast_ring_recv_msg));
		    
		    safe_aru = MIN(last_token_ring.aru, multi_cast_ring_recv_msg.aru);
		    
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
		    multi_cast_ring_msg.aru = aru;
		    multi_cast_ring_msg.seq = send_seq +
		      MIN(SEND_DATA_WIN_SIZE, num_of_packets - sent_pack_num);
		    memcpy(multi_cast_ring_msg.nack_list, multi_cast_ring_recv_msg.nack_list,
			   NACK_LIST_LEN * sizeof(int));
		    memcpy(multi_cast_ring_msg.send_pack_num,
			   multi_cast_ring_recv_msg.send_pack_num,
			   MAX_MACHINE_NUM * sizeof(int));
		    memcpy(send_pack_nums, multi_cast_ring_recv_msg.send_pack_num,
			   MAX_MACHINE_NUM * sizeof(int));
		    
		    multi_cast_ring_msg.ring_msg.msg.type = TOKEN_RING;
		    multi_cast_ring_msg.ring_msg.no = multi_cast_ring_recv_msg.ring_msg.no + 1;
		    multi_cast_ring_msg.ring_msg.type = PASS_PACK;

		    last_token_ring = multi_cast_ring_msg;

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
		    printf("pass token error!\n");
		  }
		  
		}
		
	      }
	      
	    }
	    
	  } 

	  /* Check if the message from multicast */
	} else if (FD_ISSET(sr_multi, &temp_mask)) {

	  struct MSG recv_msg;	  
	  bytes = recv(sr_multi, mess_buf, sizeof(mess_buf), 0);
	  //mess_buf[bytes] = 0;
	  memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	  //printf("multicast, type:%c\n", recv_msg.type);
	  
	  if (recv_msg.type == START_MCAST) {
	    
	    printf("Machine %d received start_mcast msg.\n", machine_index);
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
		printf("Received next machine IP address.\n");
		printIP(init_msg.addr.sin_addr.s_addr);

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
	      
	      int pos = multi_cast_content.packet_index - clear_times * WRITE_THRESHOLD;
	      recvData[pos] = multi_cast_content;
	      recvDataCheck[pos] = 1;

	      /* move aru */
	      while (recvDataCheck[local_aru - clear_times * WRITE_THRESHOLD] == 1) {
		/* write data to file */
		struct MULTI_CAST_CONTENT content = recvData[local_aru - clear_times * WRITE_THRESHOLD];
		fprintf(fw, "%2d, %8d, %8d\n", content.machine_index,
			content.packet_index, content.rand_number);

		local_aru ++;
	      }

	      // printf("current local_aru: %d\n", local_aru);
	    }
	    
	  }
	}	
      }

      clock_gettime(CLOCK_MONOTONIC, &recvEndTime);
      recvElapsedTime = (recvEndTime.tv_sec - recvStartTime.tv_sec);
      recvElapsedTime += (recvEndTime.tv_nsec - recvStartTime.tv_nsec) / 1000000000.0;

      if (recvElapsedTime > recvTotalTime)
	break;
    }

    // check if clear first 1000 data
    if (safe_aru > WRITE_THRESHOLD) {
      clear_times ++;
      memcpy(recvData, recvData + WRITE_THRESHOLD,
	     (RECV_CONTENT_LEN - WRITE_THRESHOLD) * sizeof(struct MULTI_CAST_CONTENT));
      memset(recvData + (RECV_CONTENT_LEN - WRITE_THRESHOLD), 0,
	      WRITE_THRESHOLD * sizeof(struct MULTI_CAST_CONTENT));
    }

    /* check if the machine could be terminate */
    int total_pack_num = 0;
    int i = 0;
    int check = 1;
    for (; i<num_of_machines; i++) {
      int num = send_pack_nums[i];
      // printf("num : %d\n", num);
      if (num != -1) {
	total_pack_num += num;
      } else {
	check = 0;
      }
    }
    // printf("check : %d\n", check);
    // printf("total number of packet : %d\n", total_pack_num);
    printf("local aru : %d\n", local_aru);
    printf("safe aru : %d\n", safe_aru);

    if (check && safe_aru >= total_pack_num) {
      printf("terminate!\n");
      break;
    }
  }

  fclose(fw);
  
  return 0;
}

void printIP(int ip) {
  printf("IP address is: %d.%d.%d.%d\n", (htonl(ip) & 0xff000000)>>24, 
	 (htonl(ip) & 0x00ff0000)>>16,
	 (htonl(ip) & 0x0000ff00)>>8,
	 (htonl(ip) & 0x000000ff) );
}
