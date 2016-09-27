#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include "net_include.h"

#define MIN(x, y) (x > y ? y : x)

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; // not used
  FILE *fw;
  int nwritten;
  
  int mess_len;
  int neto_len;
  int num;
  int sr;
  
  int recv_s;
  int getName;
  
  char mess_buf[PACKET_DATA_SIZE+100];

  char my_name[NAME_LENGTH] = {'\0'};  
  char host_name[NAME_LENGTH] = {'\0'};  

  long on = 1;
  
  /* Variables for TCP file transfer */
  struct sockaddr_in name;

  fd_set mask;
  fd_set dummy_mask, temp_mask;


  gethostname(my_name, NAME_LENGTH);
  printf("My host name is %s, ready to receive files...\n", my_name);

  /* Initialize the socket for receiving files */
  sr = socket(AF_INET, SOCK_STREAM, 0);
  if (sr < 0) {
    perror("rcv: socket");
    exit(1);
  }

  if (setsockopt(sr, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
    perror("Net_server: setsockopt error \n");
    exit(1);
  }
  
  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(PORT);

  if (bind(sr, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("rcv: bind");
    exit(1);
  }

  if (listen(sr,4) < 0) {
    perror("rcv: listen");
    exit(1);
  }

  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);

  /* receiver file name first */
  getName = 0;
  
  while (1) {

    temp_mask = mask;
    num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);

    if (num > 0) {
      if (FD_ISSET(sr, &temp_mask)) {
	recv_s = accept(sr, 0, 0);
	FD_SET(recv_s, &mask);
      }

      mess_len = 0;
      if (FD_ISSET(recv_s, &temp_mask)) {
	if (recv(recv_s, &mess_len, sizeof(mess_len), 0) > 0) {
	 
	  neto_len = mess_len - sizeof(mess_len);
	  if (neto_len > PACKET_DATA_SIZE)
	    continue;
	  
	  // printf("mess_len : %d\n", mess_len);	
	  // printf("neto_len : %d\n\n", neto_len);

	  recv(recv_s, mess_buf, neto_len, 0);      	 
	  mess_buf[neto_len] = '\0';
	  
	  if (getName == 0) {
	    printf("open file: %s and prepare to write.\n", mess_buf);
	    if ((fw = fopen(mess_buf, "w")) == NULL) {
	      perror("fopen");
	      exit(0);
	    }
	    getName = 1;
	  } else {
	    nwritten = fwrite(mess_buf, 1, neto_len, fw);
	  }
	} else {
	  printf("closing socket \n");
	  FD_CLR(recv_s, &mask);
	  close(recv_s);
	  break;
	}
      }
    }
  }
  
  fclose(fw);

  return 0;
}
