#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include "net_include.h"
#include "sendto_dbg.h"
#include "sendto_dbg.c"

#define MIN(x, y) (x > y ? y : x)

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; // not used
  FILE *fw;
  int nwritten;
  int bytes;
  int num;
  int status;
  char mess_buf[RECEIVER_MAX_MESS_LEN];
  char write_buf[WRITE_BUF_SIZE];
  int rcv_buf[PACK_BUF_SIZE];   //record currect received package number
  struct RTOS_MSG sendAck_pack; //ack message
  
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
  int sr;
  int rcvPackNo;    // the package No. that receiver has already received
  int startWriteNo;   // write package No.
  int lastPackDataSize; //last package data size;
  int lastPackNo; //last package No.
  
  struct SENDER_NODE *head = malloc(sizeof(
					   struct SENDER_NODE)); // Use a list of addr to maintain the sender list
  struct SENDER_NODE *curr = head;

  fd_set mask;
  fd_set dummy_mask, temp_mask;

  struct timeval timeout;

  /* Other parameters */
  int loss_rate;

  /*
  int  b[10];
  for (int i=0; i<10; i++)
    printf("data%d : %d\n", i, b[i]);
  memset(b, 0, 10*sizeof(int));
  for (int i=0; i<10; i++)           
    printf("data%d : %d\n", i, b[i]);
  b[4] = 20; b[5] = 30; b [7] = 8; b[9] = 200;
  for (int i=0; i<10; i++)           
    printf("data%d : %d\n", i, b[i]);
  //memcpy(b, b+5*sizeof(int), 5*sizeof(int));
  memcpy(b, b+5, 5*sizeof(int));
  memset(b+5, 0,5*sizeof(int)); 
  for (int i=0; i<10; i++)           
    printf("data%d : %d\n", i, b[i]);
  return;*/
  
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

  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);
  status = RECEIVER_FREE;

  sendto_dbg_init(loss_rate);
  
  /* Init receiver package number */
  rcvPackNo = -1;

  /* Init write package number */
  startWriteNo = 0;

  /* Init last package number */
  lastPackNo = -1;
  
  /* Init last package data size */
  lastPackDataSize = 0;

  memset(rcv_buf, 0, PACK_BUF_SIZE*sizeof(int));
  memset(write_buf, 0, WRITE_BUF_SIZE);
  memset(mess_buf, 0, RECEIVER_MAX_MESS_LEN);


  struct timespec start_all, end_all, last_all; /*record the transfer time*/ 
  double elapsed_all, elapsed_total;                        
  clock_gettime(CLOCK_MONOTONIC, &start_all);
  last_all = start_all;

  int count = 0;
 LOOP:
  while (1) {
    // printf("~~~~~~~~~~ recv ~~~~~~~~~~~\n");
    // printf("status: %d\n", status);

    struct timespec startTime, endTime;
    double totalTime, elapsedTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    /* Init total time */
    totalTime = RECV_WAIT_TIME;

    /* Init number of received packages */
    int ackNum = 0;
    
    /* Clear message buffer */
    // memset(mess_buf, 0, MAX_MESS_LEN);

    while (1) {
      temp_mask = mask;
      timeout.tv_sec = 0;
      timeout.tv_usec = (totalTime - elapsedTime) * 1000000;
      num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);

      if (num > 0) {
	if (FD_ISSET(sr, &temp_mask)) {
	  from_len = sizeof(from_addr);
	  bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
			   (struct sockaddr *)&from_addr, &from_len);
	  from_ip = from_addr.sin_addr.s_addr;

	  /* check msg type */
	  struct MSG recv_msg;
	  memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
	  // printf("message type: %c\n", recv_msg.type);

	  if (recv_msg.type == STOR_START_CONN) {
	    if (status == RECEIVER_FREE || status == RECEIVER_WAIT_NEXT) {
	      status = RECEIVER_START_CONN; // change status, start connection
	      /* Open or create the destination file for writing */
	      struct OPEN_CONN_MSG open_conn_msg;
	      memcpy(&open_conn_msg, mess_buf, sizeof(open_conn_msg));
	      if ((fw = fopen(open_conn_msg.filename, "w")) == NULL) {
		perror("fopen");
		exit(0);
	      }
	    } else {
	      /* check if the sender is current sender that receiver is talking to */
	      unsigned long current_ip = head->next->from_addr.sin_addr.s_addr;
	      if (from_ip != current_ip) {
		/* send busy message */
		struct WAIT_MSG wait_msg;
		wait_msg.msg.type = RTOS_WAIT_CONN;
		char wait_buf[sizeof(wait_msg)];
		memcpy(wait_buf, &wait_msg, sizeof(wait_msg));
		sendto_dbg(sr, wait_buf, sizeof(wait_buf), 0,
		       (struct sockaddr *)&from_addr, sizeof(from_addr));
	      }
	    }

	    // Check if the incoming sender is already in the list
	    int exist = 0;
	    struct SENDER_NODE *temp = head->next;
	    while (temp != NULL) {
	      if (temp->from_addr.sin_addr.s_addr == from_ip) {
		exist = 1;
		break;
	      }
	      temp = temp->next;
	    }

	    // Append the incoming address to the end of list
	    if (exist == 0) {
	      struct SENDER_NODE *ptr =
		(struct SENDER_NODE *)malloc(sizeof(struct SENDER_NODE));
	      ptr->from_addr = from_addr;
	      curr->next = ptr;
	      curr = ptr;
	      // Print out new sender's information
	      printf("Received transfer request from (%d.%d.%d.%d), "
		     "destination "
		     "file name: %s\n",
		     (htonl(from_ip) & 0xff000000) >> 24,
		     (htonl(from_ip) & 0x00ff0000) >> 16,
		     (htonl(from_ip) & 0x0000ff00) >> 8,
		     (htonl(from_ip) & 0x000000ff), mess_buf + sizeof(char));
	    }
	  } else if (recv_msg.type == STOR_CLOSE_CONN) {
	    // TODO: handle the case of closing message
	    if (status == RECEIVER_DATA_TRANSFER) {
	      printf("Receive file completely transfered msg, prepare to close "
		     "file writter. \n");
	      // printf("rcv   len: %d\n", strlen(rcv_buf));		   
	      // printf("write len: %d\n", strlen(write_buf));		   
	      printf("last package %d data size: %d\n",
		     lastPackNo, lastPackDataSize);
	      /* Write last data into file */

	      //printf("rcvPackNo : %d\n", rcvPackNo);
	      //printf("startWriteNo : %d\n", startWriteNo); 
	      int dataLen = rcvPackNo - startWriteNo +
		(lastPackDataSize == 0 ? 1:0);
	      nwritten = fwrite(write_buf, 1,                                       
				PACKET_DATA_SIZE*dataLen+lastPackDataSize, fw);
	      fclose(fw);
	      printf("count: %d\n", count);
	      printf("Data written to file complete !\n");

	      clock_gettime(CLOCK_MONOTONIC, &end_all);                   	 
	      elapsed_all = (end_all.tv_sec - start_all.tv_sec);                  
	      elapsed_all += (end_all.tv_nsec - start_all.tv_nsec) / 1000000000.0;

	      double transfered = (lastPackNo*1000)/(1024*1024);
	      double trans_rate = ((lastPackNo*1000+lastPackDataSize)*8)/(1000000*elapsed_all);
	      printf("Total Time: %f sec\nFile data were completely transfered, and %f Mbytes were successfully transfered.\n", elapsed_all, transfered);
	      printf("The average transfer rate is: %f Mbits/sec\n", trans_rate);

	      // TODO: Remove current sender (head) from the list
	      // and send ack to the next sender (if there is one)
	      struct SENDER_NODE *next_sender = head->next;
	      head->next = next_sender->next;
	      free(next_sender);
	      if (head->next == NULL) {
		status = RECEIVER_FREE;
		curr = head;
	      } else {
		printf("Start to notified next Sender");
		from_addr = head->from_addr;
		status = RECEIVER_WAIT_NEXT;
	      }
	    }

	    /*
	      while (head != NULL) {
	      from_ip = head->from_addr.sin_addr.s_addr;
	      printf("current node: %d.%d.%d.%d \n",
	      (htonl(from_ip) & 0xff000000) >> 24,
	      (htonl(from_ip) & 0x00ff0000) >> 16,
	      (htonl(from_ip) & 0x0000ff00) >> 8,
	      (htonl(from_ip) & 0x000000ff));
	      head = head->next;
	      }
	    */
	    struct CLOSE_CONN_MSG close_msg;
	    close_msg.msg.type = RTOS_CLOSE_CONN;
	    char close_buf[sizeof(close_msg)];
	    memcpy(close_buf, &close_msg, sizeof(close_msg));
	    sendto_dbg(sr, close_buf, sizeof(close_buf), 0,
		   (struct sockaddr *)&from_addr,
		   sizeof(from_addr));
	   	    
	    /* Reset receiver package number */
	    rcvPackNo = -1;                                   
	    /* Reset write package number */   
	    startWriteNo = 0;
	    /* Reset last package data size */
	    lastPackDataSize = 0;
	    /* Reset lastPackNo */
	    lastPackNo = -1;
	    
	    /* Clear buffer */
	    memset(write_buf, 0, WRITE_BUF_SIZE);
	    memset(rcv_buf, 0, PACK_BUF_SIZE*sizeof(int));

	  } else if (recv_msg.type == STOR_PACKET_COMES) {
	    if (status == RECEIVER_DATA_TRANSFER) {

	      struct STOR_MSG recv_pack;
	      memcpy(&recv_pack, mess_buf, sizeof(recv_pack));

	      // printf("Receive package (no: %d)\n", recv_pack.packageNo);

	      int len = recv_pack.packageNo-startWriteNo;			  
	      if (len >= 0 && rcv_buf[len] == 0) {
		// printf("write len:%d, rcv:%d\n", len, rcv_buf[len]);
		// printf("sendpageNo :%d\n",recv_pack.packageNo);
		memcpy(write_buf+PACKET_DATA_SIZE*len,
		       recv_pack.data, sizeof(recv_pack.data));

		/*
		for (int i=0; i<WRITE_BUF_SIZE; i++) {
		  printf("%d:%c ", i, write_buf[i]);
		}
		printf("\n");printf("\n");
		*/
		
		//memcpy(write_buf+len, recv_pack.data, 			    
		//MIN(sizeof(recv_pack.data), strlen(recv_pack.data)));	     
		rcv_buf[len] = (recv_pack.packageNo == 0 ? 1:recv_pack.packageNo);

		if (recv_pack.lastPackNo >= 0) {
		  lastPackDataSize = recv_pack.dataSize;
		  lastPackNo = recv_pack.lastPackNo;
		}
	      }
	      
	      // printf("size %d\n", sizeof(recv_pack.data));
	      // printf("len %d\n", strlen(recv_pack.data));
	      // printf("data: %s\n", recv_pack.data);

	      // nwritten =
	      // fwrite(recv_pack.data, 1,
	      //      MIN(sizeof(recv_pack.data), strlen(recv_pack.data)), fw);

	      // printf("ack %d: %d\n", ackNum, recv_pack.packageNo);
	      sendAck_pack.ackNo[ackNum] = recv_pack.packageNo;
	      ackNum ++;
	      
	      // break; // for test
	      /*
		if (bytes > 0) {
		nwritten = fwrite(mess_buf + sizeof(char), 1, bytes -
		sizeof(char), fw);
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
	    // receive connection ack from sender, start file transfer
	    printf("Received connection ack from sender, start file transfer...\n");
	    status = RECEIVER_DATA_TRANSFER;
	  } else {
	    perror("Packet error.\n");
	    exit(0);
	  }
	}
      }

      clock_gettime(CLOCK_MONOTONIC, &endTime);
      elapsedTime = (endTime.tv_sec - startTime.tv_sec);
      elapsedTime += (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0;

      // printf("elapsed time: %f\n", elapsedTime);
      // printf("send pack no %d\n", rcvPackNo);
      // printf("write no %d\n", startWriteNo);
      // printf("%d\n", rcvPackNo-startWriteNo);
      // printf("1:%d\n", rcv_buf[rcvPackNo-startWriteNo]);
      /*
	for (int i=0; i<PACK_BUF_SIZE; i++) {
	printf("data %d:%d\n", i, rcv_buf[i]);
	}
	printf("\n");
      */
      
      if (elapsedTime > totalTime)
        break;
    } // end rcv while loop

    if (status == RECEIVER_DATA_TRANSFER) {
    
      // write data and update concerning variable
      while ((rcvPackNo == -1 && rcv_buf[0] == 1) ||
	     (rcvPackNo != -1 && rcvPackNo+1 == rcv_buf[rcvPackNo+1-startWriteNo])) {
	rcvPackNo ++;

	if (rcvPackNo != 0 && rcvPackNo % 100000000 == 0) {
	  clock_gettime(CLOCK_MONOTONIC, &end_all);                   	 
	  elapsed_all = (end_all.tv_sec - last_all.tv_sec);                  
	  elapsed_all += (end_all.tv_nsec - last_all.tv_nsec) / 1000000000.0;

	  elapsed_total = (end_all.tv_sec - start_all.tv_sec);                  
	  elapsed_total += (end_all.tv_nsec - start_all.tv_nsec) / 1000000000.0;
	  
	  double transfered = ((rcvPackNo+1)*1000)/(1024*1024);
	  double trans_rate = (100*1024*1024*8)/(1000000*elapsed_all);
	  printf("Time: %f sec passed.\n100Mbytes data were received, and %f Mbytes were successfully transfered.\n", elapsed_total, transfered);
	  printf("The current average transfer rate is: %f Mbits/sec.\n\n", trans_rate);

	  last_all = end_all;
	}
      }

      // printf("send pack no %d\n", rcvPackNo);
      // printf("write no %d\n", startWriteNo);

      /* if rcvPackNo-startWriteNo larger that WIN_SIZE, write data to file */
      if (rcvPackNo-startWriteNo > WIN_SIZE) {
	/* write startWriteNo -> startWriteNo+WIN_SIZE */
	//	printf("write to file, from %d to %d\n",
	//     startWriteNo, startWriteNo+WIN_SIZE);
	if (startWriteNo + WIN_SIZE >= lastPackNo && lastPackNo >=0) {
	  printf("lastPackNo : %d\n", lastPackNo);
	  nwritten = fwrite(write_buf, 1,
		    PACKET_DATA_SIZE*(lastPackNo-startWriteNo)+lastPackDataSize, fw);
	} else {
	  nwritten = fwrite(write_buf, 1, PACKET_DATA_SIZE*WIN_SIZE, fw);
	}
	/* shift 
	(WIN_SIZE, WIN_SIZE+(rcvPackNo-startWriteNo)) -> (0, rcvPackNo-startWriteNo)
	*/

	/*
        for (int i=0; i<WRITE_BUF_SIZE; i++) {
          printf("%d:%c ", i, write_buf[i]);  
        }                                     
        printf("\n");printf("\n");            	
	*/
	count ++;
	memcpy(write_buf, write_buf+PACKET_DATA_SIZE*WIN_SIZE,
	       WRITE_BUF_SIZE-WIN_SIZE*PACKET_DATA_SIZE);
	memset(write_buf+WRITE_BUF_SIZE-WIN_SIZE*PACKET_DATA_SIZE,
	       0, PACKET_DATA_SIZE*WIN_SIZE);

	/*
	for (int i=0; i<WRITE_BUF_SIZE; i++) {
	  printf("%d:%c ", i, write_buf[i]);
	}
	printf("\n");printf("\n");
	*/	

	memcpy(rcv_buf, rcv_buf+WIN_SIZE,
	       (PACK_BUF_SIZE-WIN_SIZE)*sizeof(int));
	memset(rcv_buf+(PACK_BUF_SIZE-WIN_SIZE), 0,
	       WIN_SIZE*sizeof(int));

	startWriteNo += WIN_SIZE;
	/*
	printf("ddd\n");
	for (int i=0; i<PACK_BUF_SIZE; i++)
	  printf("%d:%d ", i, rcv_buf[i]);
	printf("....\n");
	printf("\n");printf("\n");
	*/
      }
    }
    
    // printf("~~~~~~~~~~ send ~~~~~~~~~~~\n");
    // printf("status: %d\n", status);
    // send
    if (status == RECEIVER_START_CONN) { // send connection ack
      printf("Send out connection ack.\n");
      struct START_CONN_MSG conn_msg;
      conn_msg.msg.type = RTOS_START_CONN;

      char conn_buf[sizeof(conn_msg)];
      memcpy(conn_buf, &conn_msg, sizeof(conn_msg));
      sendto_dbg(sr, conn_buf, strlen(conn_buf), 0,
             (struct sockaddr *)&(head->next->from_addr),
             sizeof(head->next->from_addr));
    } else if (status == RECEIVER_WAIT_NEXT) {
      struct AWAKE_MSG awake_msg;
      awake_msg.msg.type = RTOS_AWAKE;
      char awake_buf[sizeof(awake_msg)];
      memcpy(awake_buf, &awake_msg, sizeof(awake_msg));
      sendto_dbg(sr, awake_buf, strlen(awake_buf), 0,
             (struct sockaddr *)&(head->next->from_addr),
             sizeof(head->next->from_addr));
    } else if (status == RECEIVER_DATA_TRANSFER) {

      // printf("ackNum: %d\n", ackNum);
      sendAck_pack.msg.type = RTOS_ACK_COMES;
      sendAck_pack.ackNum = ackNum;

      /*
      for (int iOA=0; i<sendAck_pack.ackNum; i++) {
       	printf("ack: %d\n", sendAck_pack.ackNo[i]);
      }
      */

      char send_buf[sizeof(sendAck_pack)];     
      memcpy(send_buf, &sendAck_pack, sizeof(sendAck_pack));	        

      sendto_dbg(sr, send_buf, sizeof(send_buf), 0,		  
	     (struct sockaddr *)&(head->next->from_addr),       
	     sizeof(head->next->from_addr));

    } else {
      
    }
  }
  //  fclose(fw);

  return 0;
}
