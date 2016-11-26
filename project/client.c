#include "sp.h"

#include "global.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "10580"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;
static char user_name[80];
static char server_index[10];

static void user_command();
static void print_menu();
static void read_message();
static void Bye();

int in_private_group = 0;      //check if the client is connecting with one server
                               //(in one group)
char private_group_name[GROUPNAME_LEN];

int main(int argc, char *argv[]) {
  char *client_index;
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
    ret = sscanf( &command[2], "%s", user_name );
    if (ret < 1) {
      printf("Invalid user name.\n");
      break;
    }
    printf("User logged in as: %s\n", user_name);
    break;

  case 'c':
    ret = sscanf( &command[2], "%s", server_index );
    int index = atoi(server_index);
    if (ret < 1 || !(index <= 5 && index >= 1)) {
      printf("Invalid server index.\n");
      break;	
    }

    if (in_private_group == 1) {
      ret = SP_leave(Mbox, private_group_name);
      if (ret < 0) {
	SP_error(ret);
	Bye();
      }
    }

    in_private_group = 1;
    // Join the public group of the designated server
    char public_group[80];
    strcpy(public_group, "public_group_");
    strcat(public_group, server_index);

    struct CLIENT_PRIVATE_GROUP_REQ_MSG private_group_req_msg;
    private_group_req_msg.msg.type = PRIVATE_GROUP_REQ;
    
    ret = SP_multicast(Mbox, AGREED_MESS, public_group, 0, sizeof(private_group_req_msg), (char *)&private_group_req_msg);
  
    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    printf("successfully connected to server #%d\n", index);
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

static void read_message() {
  // TODO: Get response from the server
  // TODO: Join the private group set by the server

  int             service_type;
  char            sender[MAX_GROUP_NAME];
  int             num_groups;
  char            target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
  int16           mess_type;
  int             endian_mismatch;
  char            mess[MAX_MESSLEN];
  int             ret;
  
  ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups, 
		   &mess_type, &endian_mismatch, sizeof(mess), mess);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }
  
  struct SERVER_MSG msg;
  memcpy(&msg, mess, sizeof(msg));
  
  if (msg.type == PRIVATE_GROUP_RES) {
    struct SERVER_PRIVATE_GROUP_RES_MSG private_group_res_msg;
    memcpy(&private_group_res_msg, mess, sizeof(private_group_res_msg));
    
    ret = SP_join(Mbox, private_group_res_msg.group_name);

    memcpy(private_group_name, private_group_res_msg.group_name,
	   sizeof(private_group_name));
    
    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    printf("successfully join the private group: %s\n",
	   private_group_res_msg.group_name);
  }
  
}

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
