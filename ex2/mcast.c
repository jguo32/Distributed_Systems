#include <arpa/inet.h>
#include "net_include.h"

void printIP(int ip);

int main(int argc, char **argv) {
  struct sockaddr_in recv_addr;
  struct sockaddr_in recv_udp_addr;
  struct sockaddr_in send_addr;
  struct sockaddr_in self_addr;
  struct sockaddr_in next_addr;
  
  struct hostent        h_ent;
  struct hostent        *p_h_ent;
  char                  my_name[NAME_LENGTH] = {'\0'};
  int                   my_ip;
  
  int mcast_addr;

  struct ip_mreq mreq;
  unsigned char ttl_val;

  int ss, sr, ssudp, srudp;
  fd_set mask;
  fd_set dummy_mask, temp_mask;
  int bytes;
  int num;
  char mess_buf[MAX_MESS_LEN];
  char send_buf[80];

  int status;
  int tokenNo;
  
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

  status = WAIT_START_SIGNAL;
  tokenNo = machine_index == 1 ? 0 : -1;

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
  self_addr.sin_family = AF_INET;
  self_addr.sin_addr.s_addr = my_ip;
  self_addr.sin_port = htons(PORT);
  
  mcast_addr = 225 << 24 | 0 << 16 | 1 << 8 | 1; /* (225.0.1.1) */

  sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
  if (sr < 0) {
    perror("Mcast: socket");
    exit(1);
  }

  recv_addr.sin_family = AF_INET;
  recv_addr.sin_addr.s_addr = INADDR_ANY;
  recv_addr.sin_port = htons(PORT);

  if (bind(sr, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
    perror("Mcast: bind");
    exit(1);
  }

  mreq.imr_multiaddr.s_addr = htonl(mcast_addr);

  /* the interface could be changed to a specific interface if needed */
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(sr, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq,
                 sizeof(mreq)) < 0) {
    perror("Mcast: problem in setsockopt to join multicast address");
  }

  ss = socket(AF_INET, SOCK_DGRAM, 0); /* Socket for sending */
  
  if (ss < 0) {
    perror("Mcast: socket");
    exit(1);
  }

  ttl_val = 1;
  if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl_val,
                 sizeof(ttl_val)) < 0) {
    printf("Mcast: problem in setsockopt of multicast ttl %d - ignore in WinNT "
           "or Win95\n",
           ttl_val);
  }

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = htonl(mcast_addr); /* mcast address */
  send_addr.sin_port = htons(PORT);

  ssudp = socket(AF_INET, SOCK_DGRAM, 0);
  if (ssudp < 0) {
    perror("socket error");
    exit(1);
  }

  srudp = socket(AF_INET, SOCK_DGRAM, 0);
  if (srudp < 0) {
    perror("socket error");
    exit(1);
  }

  recv_udp_addr.sin_family = AF_INET;
  recv_udp_addr.sin_addr.s_addr = INADDR_ANY;
  recv_udp_addr.sin_port = htons(PORT);

  if (bind(srudp, (struct sockaddr *)&recv_udp_addr, sizeof(recv_udp_addr)) < 0) {
    perror("bind error");
    exit(1);
  }
  
  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);
  FD_SET(srudp, &mask);
  // FD_SET((long)0, &mask); /* stdin */
 OUTERLOOP:
  for (;;) {
    //printf("status: %d\n", status);
    if (status == RECEIVED_START_SIGNAL || status == CHECK_RECV_IP) {
      if (status == RECEIVED_START_SIGNAL || machine_index == 10) {
	struct INIT_MSG init_msg;
	char init_buf[sizeof(init_msg)];
	init_msg.msg.type = INIT_MCAST;
	init_msg.machine_index = machine_index;
	init_msg.addr = self_addr;
	memcpy(init_buf, &init_msg, sizeof(init_msg));
	sendto(ss, init_buf, sizeof(init_buf), 0,
	       (struct sockaddr *)&send_addr,sizeof(send_addr));
      }

      if (status == CHECK_RECV_IP) {
	//printIP(next_addr.sin_addr.s_addr);
	struct CHECK_IP_RING_MSG check_ip_msg;
	char check_ip_buf[sizeof(check_ip_msg)];
	check_ip_msg.msg.type = TOKEN_RING;
	check_ip_msg.ring_msg.no = tokenNo + 1;
	check_ip_msg.ring_msg.type = CHECK_IP_RECEIVED;
	memcpy(check_ip_buf, &check_ip_msg, sizeof(check_ip_msg));
	sendto(ssudp, check_ip_buf, sizeof(check_ip_buf), 0,
	       (struct sockaddr *)&next_addr,sizeof(next_addr));
      }
      
    } else if (status == DO_MCAST) {

    } else {
      
    }

    // Receive msg packet
    for (;;) {
      temp_mask = mask;
      if (status == WAIT_START_SIGNAL) {
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
      } else if (status == RECEIVED_START_SIGNAL) {
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
      } else {

      }
      
      if (num > 0) {
        struct MSG recv_msg;
	if (FD_ISSET(srudp, &temp_mask)) {

	  printf("get udp\n");
	  
        } else if (FD_ISSET(sr, &temp_mask)) {
          bytes = recv(sr, mess_buf, sizeof(mess_buf), 0);
	  //mess_buf[bytes] = 0;
          memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	  
          if (recv_msg.type == START_MCAST) {
	    
            printf("Machine %d received start_mcast msg.\n", machine_index);
            status = RECEIVED_START_SIGNAL;
	    
          } else if (status == RECEIVED_START_SIGNAL &&
		     recv_msg.type == INIT_MCAST) {
	 
            struct INIT_MSG init_msg;
            memcpy(&init_msg, mess_buf, sizeof(init_msg));

            // Determine the index of next machine
            int next_index = (machine_index == num_of_machines ?
			      1 :machine_index + 1);
           
	    // printIP(init_msg.addr.sin_addr.s_addr);
            if (init_msg.machine_index == next_index) {
              printf("Received next machine IP address.\n");
	      printIP(init_msg.addr.sin_addr.s_addr);

	      next_addr = init_msg.addr;
	      
	      /* pass token ring (for check) starts from machine 1 */
	      if (machine_index == 1) {
		status = CHECK_RECV_IP;
	      }
            } else {
	      /*printf("irrelevant init packet from machine %d, msg type %c.\n",
		init_msg.machine_index, init_msg.msg.type);*/
            }
          } else {

	  }
	  goto OUTERLOOP;
	} else if ((status == RECEIVED_START_SIGNAL ||
		    status == CHECK_RECV_IP) &&
		   recv_msg.type == TOKEN_RING) {
	  printf("get token\n");
	  struct RING_MSG ring_msg; 
	  memcpy(&ring_msg, mess_buf, sizeof(ring_msg)); 
	  if (ring_msg.type == CHECK_IP_RECEIVED) { 
	    status = CHECK_RECV_IP; 
	    
	    struct CHECK_IP_RING_MSG check_ip_msg; 
	    memcpy(&check_ip_msg, mess_buf, sizeof(check_ip_msg)); 
	    if (tokenNo == -1 || 
	   	tokenNo + num_of_machines == check_ip_msg.ring_msg.no) { 
	      tokenNo = check_ip_msg.ring_msg.no; 
	    } 
	  } else if (ring_msg.type == MULTICAST_PACK) { 
	    //status = 
	  }
	}
      }
    }
  }
  return 0;
}

void printIP(int ip) {
  printf("IP address is: %d.%d.%d.%d\n", (htonl(ip) & 0xff000000)>>24, 
	 (htonl(ip) & 0x00ff0000)>>16,
	 (htonl(ip) & 0x0000ff00)>>8,
	 (htonl(ip) & 0x000000ff) );
}
