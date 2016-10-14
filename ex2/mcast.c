#include "net_include.h"

int main(int argc, char **argv) {
  struct sockaddr_in name;
  struct sockaddr_in send_addr;

  int mcast_addr;

  struct ip_mreq mreq;
  unsigned char ttl_val;

  int ss, sr;
  fd_set mask;
  fd_set dummy_mask, temp_mask;
  int bytes;
  int num;
  char mess_buf[MAX_MESS_LEN];
  char send_buf[80];

  int status;

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

  mcast_addr = 225 << 24 | 0 << 16 | 1 << 8 | 1; /* (225.0.1.1) */

  sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving */
  if (sr < 0) {
    perror("Mcast: socket");
    exit(1);
  }

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(PORT);

  if (bind(sr, (struct sockaddr *)&name, sizeof(name)) < 0) {
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

  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);
// FD_SET((long)0, &mask); /* stdin */
OUTERLOOP:
  for (;;) {
    if (status == RECEIVED_START_SIGNAL) {
      struct INIT_MSG init_msg;
      init_msg.msg.type = INIT_MCAST;
      init_msg.machine_index = machine_index;
      init_msg.addr = send_addr;
      memcpy(send_buf, &init_msg, sizeof(init_msg));
      sendto(ss, send_buf, strlen(send_buf), 0, (struct sockaddr *)&send_addr,
             sizeof(send_addr));
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
        if (FD_ISSET(sr, &temp_mask)) {
          bytes = recv(sr, mess_buf, sizeof(mess_buf), 0);
          mess_buf[bytes] = 0;
          memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	  
          if (recv_msg.type == START_MCAST) {
            printf("Machine %d received start_mcast msg.\n", machine_index);
            status = RECEIVED_START_SIGNAL;
          } else if (status == RECEIVED_START_SIGNAL && recv_msg.type == INIT_MCAST) {
            struct INIT_MSG init_msg;
            memcpy(&init_msg, mess_buf, sizeof(init_msg));

            // Determine the index of next machine
            int next = machine_index + 1;
            if (next > num_of_machines) {
              next = 1;
            }
            if (init_msg.machine_index == next) {
              printf("received next machine info: %d %ld\n", next,
                     init_msg.addr.sin_addr.s_addr);
	      status = DO_MCAST;
            } else {
              printf("irrelevant init packet from machine %d, msg type %c.\n", init_msg.machine_index, init_msg.msg.type);
            }
          } else {

	  }
          goto OUTERLOOP;
        } else if (FD_ISSET(0, &temp_mask)) {

          // bytes = read(0, input_buf, sizeof(input_buf));
          // input_buf[bytes] = 0;
          // printf("there is an input: %s\n", input_buf);
          // sendto(ss, input_buf, strlen(input_buf), 0,
          //       (struct sockaddr *)&send_addr, sizeof(send_addr));
        }
      }
    }
  }

  return 0;
}
