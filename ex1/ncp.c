#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "net_include.h"
#include "sendto_dbg.h"

int main(int argc, char **argv) {
  /* Variables for file read */
  char *file_name;
  char *dest_file_name;
  FILE *fr;
  char mess_buf[MAX_MESS_LEN];            // message buffer from receiver
  struct STOR_MSG send_package[WIN_SIZE]; // packages struct;
  // char send_buf[sizeof(send_package[0])]; //send data buffer to receiver
  char read_buf[READ_BUF_SIZE]; // read data buffer from file
  char data_buf[READ_BUF_SIZE]; // copy data from read_buf to data_buf according
                                // mapping index;
  int mapping_index[WIN_SIZE];  // read_buf mapping to data_buf
  int ack_buf[WIN_SIZE];        // check if receive ack

  int readLen;    // the length that the sender will read from file
  int nread;      // the actual length of data read from file
  int bytes;      // the length of content receives from rcv
  int num;        // check if the there is msg coming
  int status;     // sender status (0:init connection, 1:file transfer, 2:close
                  // conection)
  int sendPackNo; // the package No. that sender will start to send
  int lastPackNo; // the last package No.
  int lastPackDataSize; 

  /* test
     send_package[0].type = '9';
     send_package[0].packageNo = 24;
     memcpy(send_nebuf, &send_package[0], sizeof(send_buf));
     struct STOR_MSG test_package;
     memcpy(&test_package, send_buf, sizeof(send_buf));

     printf("0: %d \n",send_buf[0]);
     printf("1: %d \n",send_buf[1]);
     printf("2: %d \n",send_buf[2]);
     printf("3: %d \n",send_buf[3]);
     printf("4: %c \n",send_buf[4]);
     printf("5: %c \n",send_buf[5]);
     printf("%d \n", send_package[0].packageNo);

     printf("%d\n", test_package.packageNo);
     printf("%c\n", test_package.type);
     return 0; */

  /*
    struct timespec start, end;
    double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int i;
    for (i=0; i< 1000000000; i++) {
    int a = 90;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec);
    elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("%f\n", elapsed); */

  /* Variables for UDP transfer */
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

  //  struct timeval timeout;

  /* Other parameters */
  int loss_rate;
  char *token;

  if (argc != 4) {
    printf("Usage: ncp <loss_rate_percent> <source_file_name> "
           "<dest_file_name>@<comp_name>\n");
    exit(0);
  }

  /* Parse the commandline parameters */
  loss_rate = atoi(argv[1]);
  file_name = argv[2];

  token = strtok(argv[3], "@");
  dest_file_name = (char *)malloc(strlen(token));
  strcpy(dest_file_name, token);

  token = strtok(NULL, "@");
  if (strlen(token) > NAME_LENGTH) {
    printf("ncp: host name is too long!\n");
    exit(1);
  }
  strcpy(host_name, token);

  gethostname(my_name, NAME_LENGTH);
  p_h_ent = gethostbyname(host_name);
  if (p_h_ent == NULL) {
    printf("ncp: gethostbyname error.\n");
    exit(1);
  }

  printf("Sending file from %s to %s @ %s.\n", my_name, dest_file_name,
         host_name);

  /* Initialize the socket for receiving messages from rcv */
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

  /* Initialize the socket for sending files */
  ss = socket(AF_INET, SOCK_DGRAM, 0);
  if (ss < 0) {
    perror("ncp: socket");
    exit(1);
  }

  memcpy(&h_ent, p_h_ent, sizeof(h_ent));
  memcpy(&host_num, h_ent.h_addr_list[0], sizeof(host_num));

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = host_num;
  send_addr.sin_port = htons(PORT);

  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(ss, &mask);

  /* Open the source file for reading */
  if ((fr = fopen(file_name, "r")) == NULL) {
    perror("fopen");
    exit(0);
  }
  printf("Opened %s for reading...\n", file_name);

  /* Init status */
  status = 0;

  /* Init read Length that sender will read from receiver */
  readLen = READ_BUF_SIZE;

  /* Init send package No, lastPackNo */
  sendPackNo = 0;
  lastPackNo = -1; //-1 : read haven't reach the end of file
  lastPackDataSize = 0;
  
  /* Init ack_buf and mapping_index, the inital val will be :
   * [0,1,2,3,...,WIN_SIZE-1] */
  for (int i = 0; i < WIN_SIZE; i++) {
    ack_buf[i] = i;
    mapping_index[i] = i;
  }

LOOP:
  while (1) {

    // send
    // printf("~~~~~~~~~~ send ~~~~~~~~~~~~\n");
    // printf("status: %d\n", status);

    if (status == SENDER_INIT_CONN) { // init connection
      /* Send the hello package to rcv */
      struct OPEN_CONN_MSG conn_msg;
      conn_msg.msg.type = STOR_START_CONN; // Header for hello packet
      memcpy(conn_msg.filename, dest_file_name, strlen(dest_file_name) + 1);

      char conn_buf[sizeof(conn_msg)];
      memcpy(conn_buf, &conn_msg, sizeof(conn_msg));
      sendto(ss, conn_buf, sizeof(conn_buf), 0, (struct sockaddr *)&send_addr,
             sizeof(send_addr));

    } else if (status == SENDER_DATA_TRANSFER) {

      // step 1. read data from file
      /* Read in a chunk of the file */

      // printf("lastPackNo: %d\n", lastPackNo);
      // printf("readLen: %d\n", readLen);

      if (lastPackNo == -1 && readLen > 0) {

        memset(read_buf, 0, READ_BUF_SIZE);

        nread = fread(read_buf, 1, readLen, fr);

        // printf("nread: %d\n", nread);

        /* fread returns a short count either at EOF or when an error occurred
         */
        if (nread < readLen) {
          if (feof(fr)) {
            printf("Finished reading data from files.\n");
            /* compute the package the last package No */
            lastPackNo = sendPackNo - readLen / PACKET_DATA_SIZE + WIN_SIZE -
                         1 + (nread / PACKET_DATA_SIZE +
                              (nread % PACKET_DATA_SIZE > 0 ? 1 : 0));
	    lastPackDataSize = nread%PACKET_DATA_SIZE;
            printf("lastPackNo: %d\n", lastPackNo);
	    printf("last time of read, data size: %d\n", lastPackDataSize);
          } else {
            printf("An error occured...\n");
            exit(0);
          }
        }

        /*copy data from read_buf to data_buf through mapping index*/
        for (int i = 0; i < WIN_SIZE; i++) {
          int m = mapping_index[i];
          if (m == -1)
            continue;
          memcpy(data_buf + i * PACKET_DATA_SIZE,
                 read_buf + m * PACKET_DATA_SIZE, PACKET_DATA_SIZE);
        }
        // printf("read_buf %s\n", read_buf);
        // printf("data_buf %s\n", data_buf);
        readLen = 0;
      }

      // step 2. pack the packages and send

      for (int n = 0; n < WIN_SIZE; n++) {

        // xixi kande feijin ba
        int p = sendPackNo % WIN_SIZE + n -
	  (sendPackNo % WIN_SIZE + n >= WIN_SIZE ? WIN_SIZE : 0);

        // printf("p: %d\n", p);

        /*check if the package was already received*/
        if (ack_buf[p] - sendPackNo > WIN_SIZE - 1)
          continue;

        /*check if the packageNo is larger than the last packageNo */
        if (lastPackNo != -1 && ack_buf[p] > lastPackNo)
          continue;

        /* Set the header of the package */
        /* Set the msg type */
        send_package[n].msg.type = STOR_PACKET_COMES;

        /* Set the package No */
        send_package[n].packageNo = ack_buf[p];
	if (send_package[n].packageNo == lastPackNo) {
	  send_package[n].lastPackage = '1';
	  send_package[n].dataSize = lastPackDataSize;
	} else {
	  send_package[n].lastPackage = '0';
	  send_package[n].dataSize = lastPackDataSize;
	}

        /* Copy data */
        int readPos = p * PACKET_DATA_SIZE;
        char send_buf[sizeof(send_package[n])];
        // TO-DO: copy error, 4 size maximum
        memcpy(send_package[n].data, data_buf + readPos, PACKET_DATA_SIZE);
        memcpy(send_buf, &send_package[n], sizeof(send_package[n]));

        //printf("pack No.%d: %s\n", send_package[n].packageNo, send_package[n].data);

        sendto(ss, send_buf, sizeof(send_buf), 0, (struct sockaddr *)&send_addr,
               sizeof(send_addr));
      }

    } else if (status == SENDER_CLOSE_CONN) {
      /*notify receiver that the data were all transfered, prepare for close
       * connection*/
      struct CLOSE_CONN_MSG close_msg;
      close_msg.msg.type = STOR_CLOSE_CONN;

      char close_buf[sizeof(close_msg)];
      memcpy(close_buf, &close_msg, sizeof(close_msg));
      sendto(ss, close_buf, sizeof(close_buf), 0, (struct sockaddr *)&send_addr,
             sizeof(send_addr));
    } else if (status == SENDER_WAIT_CONN) {
      // TODO: let the sender to wait the receiver's ack to establish connection

    } else {
      perror("Sender status error.\n");
      exit(0);
    }

    // receive
    // TO-DO: use while loop, until timeout (DONE!)
    // printf("~~~~~~~~~~ recv ~~~~~~~~~~~~\n");
    // printf("status: %d\n", status);
    struct timespec startTime, endTime;
    double totalTime, elapsedTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    /* Init total time */
    totalTime = RECV_WAIT_TIME;

    while (1) {

      // printf("remaining time: %f\n", totalTime-elapsedTime);
      temp_mask = mask;

      struct timeval timeout;
      if (status == SENDER_WAIT_CONN) {
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
      } else {
        timeout.tv_sec = 0;
        timeout.tv_usec = (totalTime - elapsedTime) * 1000000;
        num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
      }

      //      num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask,
      //      &timeout);

      if (num > 0) {
        if (FD_ISSET(ss, &temp_mask)) {

          from_len = sizeof(from_addr);

          // memset(mess_buf, 0, sizeof(mess_buf));
          bytes = recvfrom(ss, mess_buf, sizeof(mess_buf), 0,
                           (struct sockaddr *)&from_addr, &from_len);

          struct MSG recv_msg;
          memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
          //printf("message type: %c\n", recv_msg.type);

          if (recv_msg.type == RTOS_START_CONN) {
            if (status == SENDER_INIT_CONN || status == SENDER_DATA_TRANSFER) {
              // sender would send another ack and then change to file transfer
              // status
              printf("get receiver ack, start to send ack to receiver.\n");

              struct CONFIRM_CONN_MSG conn_msg;
              conn_msg.msg.type = STOR_COMFIRM_CONN; // confirm

              char conn_buf[sizeof(conn_msg)];
              memcpy(conn_buf, &conn_msg, sizeof(conn_msg));
              sendto(ss, conn_buf, strlen(conn_buf), 0,
                     (struct sockaddr *)&send_addr, sizeof(send_addr));

              status = SENDER_DATA_TRANSFER;

              goto LOOP;
            }
          } else if (recv_msg.type == RTOS_ACK_COMES) { // ack comes
            /* extract package ack no */

            struct RTOS_MSG recv_pack;
            memcpy(&recv_pack, mess_buf, sizeof(recv_pack));
	    if (recv_pack.ackNo%WIN_SIZE == 0) {
	      printf("receive ack from receiver. (ack no: %d)\n",
		     recv_pack.ackNo);
	    }

            // sendPackNo += (sendPackNo == recv_pack.ackNo ? 1:0);
            int p = recv_pack.ackNo % WIN_SIZE;
            ack_buf[p] = recv_pack.ackNo + WIN_SIZE;

            /*
            printf("ackbuf0: %d\n", ack_buf[0]);
            printf("ackbuf1: %d\n", ack_buf[1]);
            printf("ackbuf2: %d\n", ack_buf[2]);
            printf("ackbuf3: %d\n", ack_buf[3]);
            printf("ackbuf4: %d\n", ack_buf[4]);*/

            // continue;
          } else if (recv_msg.type == RTOS_CLOSE_CONN) {
            status = SENDER_TERMINATE;
          } else if (recv_msg.type == RTOS_WAIT_CONN) {
            status = SENDER_WAIT_CONN;
            goto LOOP;
          } else if (recv_msg.type == RTOS_AWAKE) {
            status = SENDER_INIT_CONN;
            break;
          } else {
            perror("Message type error.\n");
            exit(0);
          }
        }
      }

      clock_gettime(CLOCK_MONOTONIC, &endTime);
      elapsedTime = (endTime.tv_sec - startTime.tv_sec);
      elapsedTime += (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0;
      //      printf("elapsed time: %f\n", elapsedTime);

      if (elapsedTime > totalTime)
        break;
    } /* end while (1) */

    if (status == SENDER_DATA_TRANSFER) {
      /* Update sendPackNo, data refresh array */
      memset(mapping_index, -1, sizeof(mapping_index));

      int p = sendPackNo % WIN_SIZE;
      int i = 0; // mapping index starts from 0
      int incre = 0;
      while (ack_buf[p] != sendPackNo) {
        mapping_index[p] = i++;
        sendPackNo++;
        p = sendPackNo % WIN_SIZE;
        incre++;
      }

      /* Compute how much data should be read */
      readLen = incre * PACKET_DATA_SIZE;

      // printf("sendPackNo: %d\n", sendPackNo);
      
      if (sendPackNo == lastPackNo + 1) {
	printf("lastPackNo: %d\n", lastPackNo);
	printf("data was already completely transfered! \n");
        status = SENDER_CLOSE_CONN; // prepare to close connection
      }

      // if (sendPackNo == 10) break; // for test
      // printf("time out ... haven't receive any ack.\n");
    } else if (status == SENDER_TERMINATE) {
      printf("File transmission complete !\n");
      break;
    }
  }

  /* Cleaup files */
  fclose(fr);

  /* Call this once to initialize the coat routine */
  // sendto_dbg_init(rate);

  return 0;
}
