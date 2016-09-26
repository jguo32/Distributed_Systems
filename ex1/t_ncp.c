#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "net_include.h"
#include "sendto_dbg.h"

int main(int argc, char **argv) {
  /* Variables for tcp transmission */
  struct sockaddr_in host;
  struct hostent h_ent, *p_h_ent;

  char host_name[80];
  char *file_name;
  char *dest_file_name;
  char my_name[NAME_LENGTH] = {'\0'};

  int s;
  int ret;
  int bytes;
  int mess_len;
  char mess_buf[READ_BUF_SIZE];
  char *neto_mess_ptr = &mess_buf[sizeof(mess_len)];

  /* For file reading */
  FILE *fr;

  /* Other parameters */
  int loss_rate;
  char *token;

  if (argc != 4) {
    printf("Usage: t_ncp <loss_rate_percent> <source_file_name> "
           "<dest_file_name>@<comp_name>\n");
    exit(0);
  }

  /* Parse the commandline parameters */
  loss_rate = atoi(argv[1]);
  file_name = argv[2];

  token = strtok(argv[3], "@");
  dest_file_name = (char *)malloc(strlen(token));
  strcpy(dest_file_name, token);
  dest_file_name[strlen(token)] = '\0';

  token = strtok(NULL, "@");
  if (strlen(token) > NAME_LENGTH) {
    printf("t_ncp: host name is too long!\n");
    exit(1);
  }
  strcpy(host_name, token);

  gethostname(my_name, NAME_LENGTH);
  p_h_ent = gethostbyname(host_name);
  if (p_h_ent == NULL) {
    printf("t_ncp: gethostbyname error.\n");
    exit(1);
  }

  printf("Sending file from %s to %s @ %s.\n", my_name, dest_file_name,
         host_name);

  s = socket(AF_INET, SOCK_STREAM, 0); /* Create a socket (TCP) */
  if (s < 0) {
    perror("Net_client: socket error");
    exit(1);
  }

  memcpy(&h_ent, p_h_ent, sizeof(h_ent));
  memcpy(&host.sin_addr, h_ent.h_addr_list[0], sizeof(host.sin_addr));

  ret = connect(s, (struct sockaddr *)&host, sizeof(host)); /* Connect! */
  if (ret < 0) {
    perror("Net_client: could not connect to server");
    exit(1);
  }

    /* Open the source file for reading */
    if((fr = fopen(argv[1], "r")) == NULL) {
	        perror("fopen");
		    exit(0);
		      }

  for (;;) {
    bytes = fread(neto_mess_ptr, 1, sizeof(mess_buf) - sizeof(mess_len), fr);
    neto_mess_ptr[bytes] = 0;
    mess_len = strlen(neto_mess_ptr) + sizeof(mess_len);

    if (bytes > 0) {
      ret = send( s, mess_buf, mess_len, 0);
      if (ret != mess_len) {
        perror("t_ncp: error in sending");
	exit(1);
      }  
    }
  }
  return 0;
}
