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
  char conn_buf[CONN_BUF_SIZE]; //connection buffer to receiver
  char mess_buf[MAX_MESS_LEN];  //message buffer from receiver
  struct STOR_MSG send_package[WIN_SIZE]; //packages struct;
  //char send_buf[sizeof(send_package[0])]; //send data buffer to receiver
  char read_buf[READ_BUF_SIZE]; //read data buffer from file
  int ack_buf[WIN_SIZE];  //check if receive ack

  int readLen; //the length that the sender will read from file
  int nread;   //the actual length of data read from file
  int bytes;   //the length of content receives from rcv
  int num;     //check if the there is msg coming
  int status;  //sender status (0:init connection, 1:file transfer, 2:close conection)
  int readPos; //the position that sender will read from file
  int sendPackNo; //the package NO.  that sender will start to send
  int finishedRead; //finished reading file
  
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
  
  struct timeval timeout;

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
  
  /* Send the hello package to rcv */
  conn_buf[0] = '0'; // Header for hello packet
  memcpy(conn_buf + sizeof(char), dest_file_name, strlen(dest_file_name) + 1);
 
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

  /* Init file read and send package position */
  readPos = 0;
  sendPackNo = 0;

  /* Init if file is completely read */
  finishedRead = 0;

  /* Init ack buf, the inital val will be : [0,1,2,3,...,WIN_SIZE-1] */
  for (int i=0; i<WIN_SIZE; i++)
    ack_buf[i] = i;
  
  while (1) {

    // send
    printf("~~~~~~~~~~ send ~~~~~~~~~~~~\n");
    printf("status: %d\n", status);
    
    if (status == 0) { // init connection
      sendto(ss, conn_buf, strlen(conn_buf), 0,
	     (struct sockaddr *)&send_addr, sizeof(send_addr));
    } else if (status == 1) {

      // step 1. read data from file
      /* Read in a chunk of the file */
      if (!finishedRead) {
	nread = fread(read_buf + readPos*PACKET_DATA_SIZE, 1, readLen, fr);
	readLen = 0;
      }
      
      /* fread returns a short count either at EOF or when an error occurred */
      if (nread < readLen) {
	if (feof(fr)) {
	  printf("Finished reading data from files.\n");
	  finishedRead = 1;
	  /* compute the package the last package No */
	  // TO-DO get end position (endPos = ...)
	  //break;
	} else {
	  printf("An error occured...\n");
	  exit(0);
	}
      }
            
      // step 2. pack the packages and send
      // TO-DO Math.min(WIN_SIE, (endPos<sendPos ? ACK_BUF_SIZE:0)+endPos-sendPos);
      for (int n = 0; n<WIN_SIZE; n++) {
       	int p = (sendPackNo%WIN_SIZE+n >= WIN_SIZE ? sendPackNo%WIN_SIZE+n-WIN_SIZE : sendPackNo%WIN_SIZE+n); //xixi kande feijin ba

	//printf("p: %d\n", p);

	/*check if the package was already received*/
	if (ack_buf[p]-sendPackNo > WIN_SIZE-1) continue;

	/* Set the header of the package */
	/* Set the msg type */
	send_package[n].msg.type = '1';

	/* Set the package No */
	send_package[n].packageNo = ack_buf[p];

	/* Copy data */
      	int readPos = p*PACKET_DATA_SIZE;
	char send_buf[sizeof(send_package[n])];
	memcpy(send_package[n].data, read_buf+readPos, PACKET_DATA_SIZE);
	memcpy(send_buf, &send_package[n], sizeof(send_package[n]));


	printf("pack No: %d \n", send_package[n].packageNo);
	  /*  printf("p: %c \n", send_package[n].type);
	  printf("array: %d \n", send_buf[0]);
	  printf("array: %c \n", send_buf[4]); */
	
	sendto(ss, send_buf, sizeof(send_buf), 0, (struct sockaddr *)&send_addr,
	       sizeof(send_addr));
      }
   
    } else if (status == 2) {

    }

    // receive 
    // TO-DO: use while loop, until timeout

    printf("~~~~~~~~~~ recv ~~~~~~~~~~~~\n");
    printf("status: %d\n", status);  

    struct timespec start, end;
    double totalTime, elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Init total time */
    totalTime = 0.1;
    
    while (1) {
      
      temp_mask = mask;
      printf("remaining time: %f\n", totalTime-elapsed);
      timeout.tv_sec = 0;
      timeout.tv_usec = (totalTime-elapsed)*1000000;
   
      num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
         
      if (num > 0) {
	if (FD_ISSET(ss, &temp_mask)) {
	  printf("msg type: %c\n", mess_buf[0]);
	  from_len = sizeof(from_addr);

	  // memset(mess_buf, 0, sizeof(mess_buf));
	  bytes = recvfrom(ss, mess_buf, sizeof(mess_buf), 0,
			   (struct sockaddr *)&from_addr, &from_len);

	  struct MSG msg;
	  memcpy(&msg, mess_buf, sizeof(msg));
	  printf("message type: %c\n", msg.type);
	
	  if (mess_buf[0] == '1') {
	    if (status == 0 || status == 2) {
	      //sender would send another ack and then change to file transfer status
	      printf("get receiver ack, start to send ack to receiver.\n");
	      conn_buf[0] = '3';
	      sendto(ss, conn_buf, strlen(conn_buf), 0,
		     (struct sockaddr *)&send_addr, sizeof(send_addr));
	      status = 1;

	      break;
	    }
	  } else if (mess_buf[0] == '2') { //ack comes
	    /* extract package ack no */
	  
	    struct RTOS_MSG recv_pack;
	    memcpy(&recv_pack, mess_buf, sizeof(recv_pack));
	    printf("receive ack from receiver. (ack no: %d)\n", recv_pack.ackNo);

	    sendPackNo += (sendPackNo == recv_pack.ackNo ? 1:0);
	    int p = recv_pack.ackNo%WIN_SIZE;
	    ack_buf[p] = recv_pack.ackNo+WIN_SIZE;
	    printf("ackbuf0: %d\n", ack_buf[0]);
	    printf("ackbuf1: %d\n", ack_buf[1]);
	    printf("ackbuf2: %d\n", ack_buf[2]);
	    printf("ackbuf3: %d\n", ack_buf[3]);
	    printf("ackbuf4: %d\n", ack_buf[4]);
	    
	    //continue;
	  }
	}
      }

      clock_gettime(CLOCK_MONOTONIC, &end);
      elapsed = (end.tv_sec - start.tv_sec);
      elapsed += (end.tv_nsec - start.tv_nsec) / 1000000000.0;
      printf("elapsed time: %f\n", elapsed);

      if (elapsed > totalTime) break;
    } /* end while (1) */

    printf("sendPackNo: %d\n", sendPackNo);
    if (sendPackNo == 10) break;
    
    // printf("time out ... haven't receive any ack.\n");    
  }

  /* Cleaup files */
  fclose(fr);

  /* Call this once to initialize the coat routine */
  // sendto_dbg_init(rate);

  return 0;
}
