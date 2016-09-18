#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define NAME_LENGTH 80
#define ACK_BUF_SIZE 80

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; //not used 
  FILE *fw;
  char buf[BUF_SIZE];
  int nwritten;
  int bytes;
  int num;
  char mess_buf[MAX_MESS_LEN];
  char ack_buf[ACK_BUF_SIZE];

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
  while (1) {
    temp_mask = mask;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);

    if (num > 0) {
      if (FD_ISSET(sr, &temp_mask)) {
        from_len = sizeof(from_addr);
        bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
                         (struct sockaddr *)&from_addr, &from_len);

        if (mess_buf[0] == '0') {
          from_ip = from_addr.sin_addr.s_addr;

	  ack_buf[0] = '1';
	  sendto(sr, ack_buf, strlen(ack_buf), 0,
	  	 (struct sockaddr *)&from_addr, sizeof(from_addr));

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

        } else if (mess_buf[0] == '2') {
          // TODO: handle the case of closing message

        } else if (mess_buf[0] == '1') {

          printf("Received message: %s\n", mess_buf + sizeof(char));
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
          }
        } else {
          perror("Package error.\n");
          exit(0);
        }
      }
    }
  }
  fclose(fw);

  return 0;
}
