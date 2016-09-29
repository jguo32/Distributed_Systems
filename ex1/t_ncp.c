#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "net_include.h"

int main(int argc, char **argv) {
  /* Variables for file read */
  char *file_name;
  char *dest_file_name;
  FILE *fr;
  
  // char send_buf[sizeof(send_package[0])]; //send data buffer to receiver
  char read_buf[PACKET_DATA_SIZE+1]; // read data buffer from file
  int nread;      // the actual length of data read from file

  /* Variables for TCP transfer */
  struct sockaddr_in host;
  struct hostent h_ent;
  struct hostent *p_h_ent;
  char send_buf[PACKET_DATA_SIZE+2];
  
  char my_name[NAME_LENGTH] = {'\0'};
  char host_name[NAME_LENGTH] = {'\0'}; // Host to send to

  int ss;
  int ret;
  int finishedRead;

  char *token;

  if (argc != 3) {
    printf("Usage: t_ncp <source_file_name> "
           "<dest_file_name>@<comp_name>\n");
    exit(0);
  }

  /* Parse the commandline parameters */
  file_name = argv[1];

  token = strtok(argv[2], "@");
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

  /* Initialize the socket for sending files */
  ss = socket(AF_INET, SOCK_STREAM, 0);
  if (ss < 0) {
    perror("ncp: socket");
    exit(1);
  }

  host.sin_family = AF_INET;  
  host.sin_port = htons(PORT);
    
  memcpy(&h_ent, p_h_ent, sizeof(h_ent));
  memcpy(&host.sin_addr, h_ent.h_addr_list[0], sizeof(host.sin_addr));

  ret = connect(ss, (struct sockaddr *)&host, sizeof(host) ); /* Connect!*/
  if ( ret < 0 ) {
    perror( "Net_client: could not connect to server");
    exit(1);
  }

  /* Open the source file for reading */
  if ((fr = fopen(file_name, "r")) == NULL) {
    perror("fopen");
    exit(0);
  }
  printf("Opened %s for reading...\n", file_name);

  /* Init finished reading file */
  finishedRead = -1;

  /* Send the file name */

  int len = strlen(dest_file_name);
  memcpy(send_buf+sizeof(len), dest_file_name, len);
  char *neto = &read_buf[sizeof(len)];
  neto[len] = 0;
  len = len + sizeof(len);
  memcpy(send_buf, &len, sizeof(len));
  ret = send(ss, send_buf, len, 0);  

  
  while (1) {
    len = 0;
    
    memset(read_buf, 0, PACKET_DATA_SIZE+1);
    nread = fread(read_buf, 1, PACKET_DATA_SIZE, fr);

    // printf("nread: %d\n", nread);
    /* fread returns a short count either at EOF or when an error occurred
     */
    read_buf[nread] = '\0';
    if (nread < PACKET_DATA_SIZE) {
      if (feof(fr)) {
	printf("nread: %d\n", nread);
	printf("Finished reading data from files.\n");
	printf("last time of read, data size: %d\n", nread);
	finishedRead = 1;  
      } else {
	printf("An error occured...\n");
	exit(0);
      }
    }

    len = nread;
    memcpy(send_buf+sizeof(len), read_buf, len);
    // printf("len : %d\n", len);

    neto[len] = 0;
    len = len + sizeof(len);
    memcpy(send_buf, &len, sizeof(len));

    ret = send(ss, send_buf, len, 0);  
	
    if (finishedRead == 1) {
      // tell the receiver that the file was complelely read
      break;
    }
  }
  /* Cleaup files */
  fclose(fr);

  return 0;
}
