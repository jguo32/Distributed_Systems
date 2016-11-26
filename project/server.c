#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSLEN 102400
#define MAX_MEMBERS 100

#define PORT "10580"
#define GLOBAL_GROUP "GLOBAL"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;

static void print_menu();
static void read_message();
static void Bye();

int main(int argc, char *argv[]) {
  int num_servers = 5;
  char *server_index;

  static char mess[MAX_MESSLEN];
  char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
  char sender[MAX_GROUP_NAME];
  int ret;
  int service_type;
  int num_groups;
  int16 mess_type;
  int endian_mismatch;

  if (argc != 2) {
    printf("Usage: server <server_index>.\n");
    exit(0);
  }
  server_index = argv[1];

  sprintf(User, "server_");
  // strcpy(User, "server_");
  strcat(User, server_index);
  sprintf(Spread_name, PORT);

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret != ACCEPT_SESSION) {
    SP_error(ret);
    Bye();
  }

  // Join global group with all servers in it
  ret = SP_join(Mbox, GLOBAL_GROUP);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }

  // Join public group with only current server in it
  char public_group[80];
  strcpy(public_group, "public_group_");
  strcat(public_group, server_index);
  ret = SP_join(Mbox, public_group);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }

  printf("Server #%s successfully launched, public group name: %s.\n",
         server_index, public_group);

  // Test code
  char send_buf[80];
  strcpy(send_buf, "hello world....");
  ret = SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP, 0, sizeof(send_buf),
                     send_buf);

  // May use Spread event handler
  membership_info memb_info;
  while (1) {
    ret =
        SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups,
                   &mess_type, &endian_mismatch, sizeof(mess), mess);
    if (ret < 0) {
      SP_error(ret);
      printf("\nBye.\n");
      exit(0);
    }

    if (Is_membership_mess(service_type)) {
      ret = SP_get_memb_info(mess, service_type, &memb_info);
      if (ret < 0) {
        printf("BUG: membership message does not have valid body\n");
        SP_error(ret);
        exit(1);
      }
    }

  }

  // Event handler skeleton
  E_init();

  E_attach_fd(Mbox, READ_FD, read_message, 0, NULL, HIGH_PRIORITY);

  E_handle_events();

  return (0);
}
static void read_message() {}

static void print_menu() { printf("\n"); }

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}
