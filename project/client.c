#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSLEN 102400

#define PORT "10580"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;
static char username[80];
static char server_index[10];

static void user_command();
static void print_menu();
static void read_message();
static void Bye();

int main(int argc, char *argv[]) {
  char *client_index;
  char user_name[80];
  
  int ret;

  if (argc != 2) {
    printf("Usage: client <client_index>.\n");
    exit(0);
  }
  client_index = argv[1];

  sprintf(User, "client_");
  strcat(User, client_index);
  sprintf(Spread_name, PORT);

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret != ACCEPT_SESSION) {
    SP_error(ret);
    Bye();
  }

  printf("User: connected to %s with private group %s\n", Spread_name,
         Private_group);

  E_init();

  E_attach_fd(0, READ_FD, user_command, 0, NULL, LOW_PRIORITY);

  E_attach_fd(Mbox, READ_FD, read_message, 0, NULL, HIGH_PRIORITY);

  print_menu();

  printf("\nUser> ");
  fflush(stdout);

  E_handle_events();

  return (0);
}

static void user_command() {
  char command[130];
  char mess[MAX_MESSLEN];
  //char group[80];
  char groups[10][MAX_GROUP_NAME];
  int num_groups;
  unsigned int mess_len;
  int ret;
  int i;

  for (i = 0; i < sizeof(command); i++) {
    command[i] = 0;
  }

  if (fgets(command, 130, stdin) == NULL) {
    Bye();
  }

  switch(command[0]) {
    case 'u':
      ret = sscanf( &command[2], "%s", username );
      if (ret < 1) {
        printf("Invalid username.\n");
	break;
      }
      printf("User logged in as: %s\n", username);
      break;
    case 'c':
      ret = sscanf( &command[2], "%s", server_index );
      int index = atoi(server_index);
      if (ret < 1 || !(index <= 5 && index >= 1)) {
        printf("Invalid server index.\n");
        break;	
      }
      
      // Join the public group of the designated server
      char public_group[80];
      strcpy(public_group, "public_group_");
      strcat(public_group, server_index);
      ret = SP_join(Mbox, public_group);
      if (ret < 0) {
        SP_error(ret);
	Bye();
      }
      
      // TODO: Get response from the server
      

      // TODO: Join the private group set by the server


      printf("Successfully connected to server #%d\n", index);
      break;
    case 'l':
      break;
    case 'q':
      Bye();
      break;

    default:
      printf("\nUnknown command.\n");
      print_menu();
      break;

  }
  printf("\nUser> ");
  fflush(stdout);
}

static void read_message() {}

static void print_menu() {
  printf("\n");
  printf("==========\n");
  printf("User Menu:\n");
  printf("----------\n");
  printf("\n");

  printf("\tu <name>: login with a user name.\n");
  printf("\tc <server>: connect to a specific server(1 - 5).\n");
  printf("\tl: list the headers of received mails.\n");
  printf("\tm: mail a message to a user.\n");
  printf("\td <mail_index>: delete a mail message.\n");
  printf("\tr <mail_index>: read a received mail message.\n");
  printf("\tv: print the membership of the mail servers.\n");
  printf("\n");
  printf("\tq: quit.\n");
  fflush(stdout);
}

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}
