#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define BUF_SIZE 10
#define NAME_LENGTH 80

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name;
  FILE *fw;
  char buf[BUF_SIZE];
  int nwritten;
  int bytes;
  int num;
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
  /*
  ss = socket(AF_INET, SOCK_DGRAM, 0);
  if (ss < 0) {
    perror("ncp: socket");
    exit(1);
  }

  printf("here\n.");
  memcpy(&h_ent, p_h_ent, sizeof(h_ent));
  memcpy(&host_num, h_ent.h_addr_list[0], sizeof(host_num));

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = host_num;
  send_addr.sin_port = htons(PORT);
  */

  /* Open or create the destination file for writing */
  // TODO: get the file name from sender
  if ((fw = fopen("receivedfile", "w")) == NULL) {
    perror("fopen");
    exit(0);
  }

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
        if (bytes > 0) {
          nwritten = fwrite(mess_buf, 1, bytes, fw);
          if (nwritten != bytes) {
            perror("fwrite");
            exit(0);
          }
        }
	mess_buf[bytes] = 0;
        from_ip = from_addr.sin_addr.s_addr;
        printf("Received from (%d.%d.%d.%d): %s\n",
               (htonl(from_ip) & 0xff000000) >> 24,
               (htonl(from_ip) & 0x00ff0000) >> 16,
               (htonl(from_ip) & 0x0000ff00) >> 8,
               (htonl(from_ip) & 0x000000ff), mess_buf);
      }
    }
  }
  fclose(fw);

  return 0;
}
