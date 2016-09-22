#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define MIN(x,y) (x > y ? y:x)

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; //not used 
  FILE *fw;
  int nwritten;
  int bytes;
  int num;
  int status; //(-1:free; 0: connection,send ack to sender; 1:file transfer, send packet ack; 2: )
  char mess_buf[MAX_MESS_LEN];

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
  status = RECEIVER_FREE;
  
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
	struct MSG recv_msg;
	memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	printf("message type: %c\n", recv_msg.type);
	
        if (recv_msg.type == STOR_START_CONN) {
	  if (status == RECEIVER_FREE) {
	    from_ip = from_addr.sin_addr.s_addr;
	    status = RECEIVER_START_CONN; //change status, start connection
	  
	    /* Open or create the destination file for writing */
	    if ((fw = fopen(mess_buf + sizeof(char), "w")) == NULL) {
	      perror("fopen");
	      exit(0);
	    }

	    printf("Received transfer request from (%d.%d.%d.%d), destination "
		   "file name: %s\n",
		   (htonl(from_ip) & 0xff000000) >> 24,
		   (htonl(from_ip) & 0x00ff0000) >> 16,
		   (htonl(from_ip) & 0x0000ff00) >> 8,
		   (htonl(from_ip) & 0x000000ff), mess_buf + sizeof(char));
	  }
        } else if (recv_msg.type == STOR_CLOSE_CONN) {
          // TODO: handle the case of closing message
	  if (status == RECEIVER_DATA_TRANSFER) {
	    printf("Receive file completely transfered msg, prepare to close file writter. \n");
	    fclose(fw);
	    status = RECEIVER_FREE;
	  }
	  struct CLOSE_CONN_MSG close_msg;
	  close_msg.msg.type = RTOS_CLOSE_CONN;
	  char close_buf[sizeof(close_msg)];
	  memcpy(close_buf, &close_msg, sizeof(close_msg));
	  sendto(sr, close_buf, sizeof(close_buf), 0,          
		 (struct sockaddr *)&from_addr, sizeof(from_addr));
	  
        } else if (recv_msg.type == STOR_PACKET_COMES) {
	  if (status == RECEIVER_DATA_TRANSFER) {

	    struct STOR_MSG recv_pack;
	    memcpy(&recv_pack, mess_buf, sizeof(recv_pack));

	    printf("Receive package (no: %d)\n", recv_pack.packageNo);
	    printf("size %d\n", sizeof(recv_pack.data));		     
	    printf("len %d\n", strlen(recv_pack.data));
	    printf("data: %s\n", recv_pack.data);

	    nwritten = fwrite(recv_pack.data, 1,
			      MIN(sizeof(recv_pack.data), strlen(recv_pack.data)), fw);

	    struct RTOS_MSG send_pack;
	    send_pack.msg.type = RTOS_ACK_COMES;
	    send_pack.ackNo = recv_pack.packageNo;
	      
	    char send_buf[sizeof(send_pack)];
	    memcpy(send_buf, &send_pack, sizeof(send_pack));

	    /*TO-DO, pack all the ack together and send out */
	    sendto(sr, send_buf, sizeof(send_buf), 0,
	     (struct sockaddr *)&from_addr, sizeof(from_addr));
	    
	    //break; // for test
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
        } else if (recv_msg.type == STOR_COMFIRM_CONN) {
	  //receive connection ack from sender, start file transfer
	  printf("Received connection ack from sender, start file transfer...\n");
	  status = RECEIVER_DATA_TRANSFER; 
        } else {
          perror("Packet error.\n");
          exit(0);
        }
      }
    }

    printf("~~~~~~~~~~ send ~~~~~~~~~~~\n");
    printf("status: %d\n", status);
    //send
    if (status == RECEIVER_START_CONN) { //send connection ack
      printf("Send out connection ack.\n");
      struct START_CONN_MSG conn_msg;
      conn_msg.msg.type = RTOS_START_CONN;

      char conn_buf[sizeof(conn_msg)];
      memcpy(conn_buf, &conn_msg, sizeof(conn_msg));
      sendto(sr, conn_buf, strlen(conn_buf), 0,
	     (struct sockaddr *)&from_addr, sizeof(from_addr));
    }
  }
  //  fclose(fw);

  return 0;
}
