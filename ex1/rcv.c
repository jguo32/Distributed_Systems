#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define ACK_MSG_SIZE 32

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; //not used 
  FILE *fw;
  char buf[BUF_SIZE];
  int nwritten;
  int bytes;
  int num;
  int status; //(-1:free; 0: connection,send ack to sender; 1:file transfer, send packet ack; 2: )
  char mess_buf[MAX_MESS_LEN];
  char ack_msg[ACK_MSG_SIZE];

  /* Variables for UDP file transfer */
  struct sockaddr_in name;
  struct sockaddr_in send_addr;
  struct sockaddr_in from_addr;
  socklen_t from_len;

  struct hostent h_ent;
  struct hostent *p_h_ent;
  char my_name[NAME_LENGTH] = {'\0'};
  char host_name[NAME_LENGTH] = {'\0'}; // Host to send to
  int host_num;
  int from_ip;
  int ss, sr;

  fd_set mask;
  fd_set dummy_mask, temp_mask;

  struct timeval timeout;

  /* Other parameters */
  int loss_rate;

  if (argc != 2) {
    printf("Usage: rcv <loss_rate_percent>\n");
    exit(0);
  }

  loss_rate = atoi(argv[1]);

  gethostname(my_name, NAME_LENGTH);
  printf("My host name is %s, ready to receive files...\n", my_name);

  /* Initialize the socket for receiving files */
  sr = socket(AF_INET, SOCK_DGRAM, 0);
  if (sr < 0) {
    perror("ncp: socket");
    exit(1);
  }

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(PORT);

  if (bind(sr, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("ncp: bind");
    exit(1);
  }

  /* Initialize the socket for sending messages */
  /*ss = socket(AF_INET, SOCK_DGRAM, 0);
  if (ss < 0) {
    perror("ncp: socket");
    exit(1);
  }

  memcpy(&h_ent, p_h_ent, sizeof(h_ent));
  memcpy(&host_num, h_ent.h_addr_list[0], sizeof(host_num));

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = host_num;
  send_addr.sin_port = htons(PORT);*/
  
  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);
  // FD_SET((long)0, &mask);
  status = -1;
  
  while (1) {

    //receive

    printf("~~~~~~~~~~ recv ~~~~~~~~~~~\n");
    printf("status: %d\n", status);
    
    temp_mask = mask;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);

    if (num > 0) {
      if (FD_ISSET(sr, &temp_mask)) {
        from_len = sizeof(from_addr);
        bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
                         (struct sockaddr *)&from_addr, &from_len);

	/* check msg type */
	struct MSG msg;
	memcpy(&msg, mess_buf, sizeof(msg));
	printf("message type: %c\n", msg.type);
	
        if (mess_buf[0] == '0') {
	  if (status == -1) {
	    from_ip = from_addr.sin_addr.s_addr;
	    status = 0; //change status, start connection
	  
	    /* Open or create the destination file for writing */
	    if ((fw = fopen(mess_buf + sizeof(char), "w")) == NULL) {
	      perror("fopen");
	      exit(0);
	    }

	    printf("Received transfer request from (%d.%d.%d.%d), destination "
		   "file: %s\n",
		   (htonl(from_ip) & 0xff000000) >> 24,
		   (htonl(from_ip) & 0x00ff0000) >> 16,
		   (htonl(from_ip) & 0x0000ff00) >> 8,
		   (htonl(from_ip) & 0x000000ff), mess_buf + sizeof(char));
	  }
        } else if (mess_buf[0] == '2') {
          // TODO: handle the case of closing message
	  
        } else if (mess_buf[0] == '1') {
	  if (status == 1) {

	    struct STOR_MSG recv_pack;
	    memcpy(&recv_pack, mess_buf, sizeof(recv_pack));
	    printf("receive package: %d\n", recv_pack.packageNo, recv_pack.msg.type);

	    struct RTOS_MSG send_pack;
	    send_pack.msg.type = '2';
	    send_pack.ackNo = recv_pack.packageNo;
	      
	    char send_buf[sizeof(send_pack)];
	    memcpy(send_buf, &send_pack, sizeof(send_pack));
	    
	    sendto(sr, send_buf, strlen(send_buf), 0,
	     (struct sockaddr *)&from_addr, sizeof(from_addr));
	    /*
	    if (bytes > 0) {
	      nwritten = fwrite(mess_buf + sizeof(char), 1, bytes - sizeof(char), fw);
	      if (nwritten != (bytes - sizeof(char))) {
		perror("fwrite");
		exit(0);
	      }
	    }

	    // Why this line??
	    mess_buf[bytes] = 0;
	    if (bytes < BUF_SIZE) {
	      break;
	    }*/

	  }
        } else if (mess_buf[0] == '3') {
	  //receive connection ack from sender, start file transfer
	  printf("received connection ack from sender, start file transfer...\n");
	  status = 1; 
        } else {
          perror("Package error.\n");
          exit(0);
        }
      }
    }

    printf("~~~~~~~~~~ send ~~~~~~~~~~~\n");
    printf("status: %d\n", status);
    //send
    if (status == 0) { //send connection ack
      printf("send out connection ack.\n");
      ack_msg[0] = '1';
      sendto(sr, ack_msg, strlen(ack_msg), 0,
	     (struct sockaddr *)&from_addr, sizeof(from_addr));
    }
  }
  fclose(fw);

  return 0;
}
