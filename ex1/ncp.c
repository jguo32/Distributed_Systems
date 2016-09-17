#include <arpa/inet.h>
#include <netdb.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define NAME_LENGTH 80
#define CONN_BUF_SIZE 81

int main(int argc, char **argv) {
  /* Variables for file read */
  char *file_name;
  char *dest_file_name;
  FILE *fr;
  char buf[BUF_SIZE];
  char conn_buf[CONN_BUF_SIZE];
  int nread;

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
  
  /* Send the hello package to rcv */
  conn_buf[0] = '0'; // Header for hello packet
  memcpy(conn_buf + sizeof(char), dest_file_name, strlen(dest_file_name) + 1);
  sendto(ss, conn_buf, strlen(conn_buf), 0,
		 (struct sockaddr *)&send_addr, sizeof(send_addr)); 

  /* Open the source file for reading */
  if ((fr = fopen(file_name, "r")) == NULL) {
    perror("fopen");
    exit(0);
  }
  printf("Opened %s for reading...\n", file_name);

  while (1) {
    
    /* Set the header of the package */
    buf[0] = '1';

    /* Read in a chunk of the file */
    nread = fread(buf + sizeof(char), 1, BUF_SIZE, fr);

    /* If there is something to write, write it */
    if (nread > 0) {
      sendto(ss, buf, nread + sizeof(char), 0, (struct sockaddr *)&send_addr,
             sizeof(send_addr));
    }

    /* fread returns a short count either at EOF or when an error occurred */
    if (nread < BUF_SIZE) {
      if (feof(fr)) {
        printf("Finished sending files.\n");
        break;
      } else {
        printf("An error occured...\n");
        exit(0);
      }
    }
  }

  /* Cleaup files */
  fclose(fr);

  /* Call this once to initialize the coat routine */
  // sendto_dbg_init(rate);

  return 0;
}
