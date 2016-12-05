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
static char user_name[USERNAME_LEN];
static char server_index[10];
static char email_no[10];

static void user_command();
static void print_menu();
static void read_message();
static void Bye();

char status = INIT;
char private_group_name[GROUPNAME_LEN];
struct EMAIL_MSG email_list[EMAIL_LIST_MAX_LEN];
int email_num = -1;
int cur_server_index = -1;

int last_connect_index = -1;

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
  // char mess[MAX_MESSLEN];
  // char group[80];
  // char groups[10][MAX_GROUP_NAME];
  // int num_groups;
  // unsigned int mess_len;
  int ret;
  int i;

  for (i = 0; i < sizeof(command); i++) {
    command[i] = 0;
  }

  if (fgets(command, 130, stdin) == NULL) {
    Bye();
  }

  switch (command[0]) {
  case 'u': {

    if (status != INIT)
      ret = SP_leave(Mbox, user_name);

    ret = sscanf(&command[2], "%s", user_name);
    if (ret < 1) {
      printf("Invalid user name.\n");
      break;
    }
    printf("User logged in as: %s\n", user_name);

    ret = SP_join(Mbox, user_name);

    status = LOGIN;
    cur_server_index = -1;

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    break;
  }

  case 'c': {
    ret = sscanf(&command[2], "%s", server_index);
    int index = atoi(server_index);
    if (ret < 1 || !(index <= 4 && index >= 0)) {
      printf("Invalid server index.\n");
      break;
    }

    if (cur_server_index != -1 && cur_server_index == index) {
      printf("you are already connect with this server.\n");
      break;
    }

    if (status == CONNECT) {
      ret = SP_leave(Mbox, private_group_name);
      printf("disconnect with server #%d\n", cur_server_index);
      if (ret < 0) {
        SP_error(ret);
        Bye();
      }
      status = LOGIN;
    }

    printf("connecting to server #%d\n", index);
    last_connect_index = index;

    struct CLIENT_CHECK_MEMBER_REQ_MSG check_member_req_msg;
    check_member_req_msg.msg.type = MEMBER_CHECK_REQ;

    // check_member_req_msg.msg.type = MEMBERSHIP_REQ;
    check_member_req_msg.msg.source.type = CLIENT;
    ret = SP_multicast(Mbox, AGREED_MESS, "GLOBAL_SERVER_GROUP", 0,
                       sizeof(check_member_req_msg),
                       (char *)&check_member_req_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    break;
  }

  case 'm': {
    if (status != CONNECT) {
      printf("you should connect server first!\n");
      break;
    }

    struct EMAIL email;
    email.read = 0;
    memcpy(email.from, user_name, USERNAME_LEN);

    /* get recipient's name */
    printf("\nTo:> ");
    while (fgets(email.to, USERNAME_LEN, stdin) == NULL ||
           (strlen(email.to) == 1 && email.to[0] == '\n')) {
      printf("please enter the recipient's name\n");
      printf("\nTo:> ");
    }

    email.to[strlen(email.to) - 1] = 0; // remove enter
    printf("recipient name: %s\n", email.to);

    /* get email subject */
    printf("\nSubject:> ");
    while (fgets(email.subject, SUBJECT_LEN, stdin) == NULL ||
           (strlen(email.subject) == 1 && email.subject[0] == '\n')) {
      printf("please enter the subject\n");
      printf("\nSubject:> ");
    }

    email.subject[strlen(email.subject) - 1] = 0; // remove enter
    printf("subject: %s\n", email.subject);

    /* get email content */
    printf("\nContent:> ");
    while (fgets(email.content, CONTENT_LEN, stdin) == NULL ||
           (strlen(email.content) == 1 && email.content[0] == '\n')) {
      printf("please enter the content\n");
      printf("\nContent:> ");
    }

    email.content[strlen(email.content) - 1] = 0; // remove enter
    printf("content: %s\n", email.content);

    struct CLIENT_SEND_EMAIL_MSG send_email_msg;
    send_email_msg.msg.source.type = CLIENT;
    send_email_msg.msg.type = SEND_EMAIL;
    memcpy(send_email_msg.receiver_name, email.to, sizeof(email.to));
    send_email_msg.email = email;

    ret = SP_multicast(Mbox, AGREED_MESS, private_group_name, 0,
                       sizeof(send_email_msg), (char *)&send_email_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    break;
  }

  case 'l': {
    struct CLIENT_EMAIL_LIST_REQ_MSG email_list_req_msg;
    email_list_req_msg.msg.source.type = CLIENT;
    email_list_req_msg.msg.type = EMAIL_LIST_REQ;
    memcpy(email_list_req_msg.receiver_name, user_name, USERNAME_LEN);
    ret = SP_multicast(Mbox, AGREED_MESS, private_group_name, 0,
                       sizeof(email_list_req_msg), (char *)&email_list_req_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }

    break;
  }

  case 'r': {

    if (email_num < 0) {
      printf("you should request email list first.\n");
      break;
    }

    ret = sscanf(&command[2], "%s", email_no);
    int no = atoi(email_no) - 1;
    if (no < 0 || no >= email_num) {
      printf("invalid email number\n");
      break;
    }

    struct CLIENT_READ_EMAIL_MSG read_email_msg;
    read_email_msg.msg.type = READ_EMAIL_REQ;
    read_email_msg.msg.source.type = CLIENT;
    read_email_msg.server_index = email_list[no].server_index;
    read_email_msg.email_index = email_list[no].email_index;
    memcpy(read_email_msg.user_name, user_name, USERNAME_LEN);

    ret = SP_multicast(Mbox, AGREED_MESS, private_group_name, 0,
                       sizeof(read_email_msg), (char *)&read_email_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }
    break;
  }

  case 'd': {
    if (email_num < 0) {
      printf("you should request email list first.\n");
      break;
    }

    ret = sscanf(&command[2], "%s", email_no);
    int no = atoi(email_no) - 1;
    if (no < 0 || no >= email_num) {
      printf("invalid email number\n");
      break;
    }

    struct CLIENT_DELETE_EMAIL_MSG delete_email_msg;
    delete_email_msg.msg.type = DELETE_EMAIL_REQ;
    delete_email_msg.msg.source.type = CLIENT;
    delete_email_msg.server_index = email_list[no].server_index;
    delete_email_msg.email_index = email_list[no].email_index;
    memcpy(delete_email_msg.user_name, user_name, USERNAME_LEN);

    ret = SP_multicast(Mbox, AGREED_MESS, private_group_name, 0,
                       sizeof(delete_email_msg), (char *)&delete_email_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }
    break;
  }

  case 'v': {
    struct CLIENT_MEMBERSHIP_MSG membership_msg;
    membership_msg.msg.source.type = CLIENT;
    membership_msg.msg.type = MEMBERSHIP_REQ;

    ret = SP_multicast(Mbox, AGREED_MESS, private_group_name, 0,
                       sizeof(membership_msg), (char *)&membership_msg);

    if (ret < 0) {
      SP_error(ret);
      Bye();
    }
    break;
  }

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
  // TODO: check invalid server

  int service_type;
  char sender[MAX_GROUP_NAME];
  int num_groups;
  char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
  int16 mess_type;
  int endian_mismatch;
  char mess[MAX_MESSLEN];
  int ret;

  ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups,
                   &mess_type, &endian_mismatch, sizeof(mess), mess);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }

  if (Is_reg_memb_mess(service_type)) {
    // Leave the group when server is down
    if (Is_caused_leave_mess(service_type) ||
        Is_caused_disconnect_mess(service_type) ||
        Is_caused_network_mess(service_type)) {

      /*TODO check server or client */
      if (strcmp(user_name, sender) == 0)
        return;

      SP_leave(Mbox, sender);
      status = LOGIN;
      email_num = -1;
      cur_server_index = -1;

      printf("disconnect from server, please reconnect!\n");
      printf("\nUser> ");
      fflush(stdout);
      return;

    }
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

    status = CONNECT;

    email_num = -1;
    cur_server_index = atoi(server_index);

    printf("\nsuccessfully connected to server #%s\n", server_index);
    printf("\nUser> ");
    fflush(stdout);

  } else if (msg.type == EMAIL_LIST_RES) {

    struct SERVER_EMAIL_LIST_RES_MSG email_list_res_msg;
    memcpy(&email_list_res_msg, mess, sizeof(email_list_res_msg));
    email_num = email_list_res_msg.email_num;
    memcpy(email_list, email_list_res_msg.email_list,
           email_num * sizeof(struct EMAIL_MSG));

    printf("\nuser: %s, server index: %s\n", user_name, server_index);
    printf("%-5s %-10s %-12s %s\n", "no", "status", "from", "subject");

    for (int i = 0; i < email_num; i++) {
      char *read = "unread";
      if (email_list[i].email.read == 1)
        read = "read";

      printf("%-5d %-10s %-12s %-10s ",
             i+1, read,
             email_list[i].email.from,
             email_list[i].email.subject);
      printf("%-5d %-5d", email_list[i].server_index,
             email_list[i].email_index);

      printf("\n");
    }
    printf("\nUser> ");
    fflush(stdout);

  } else if (msg.type == READ_EMAIL_RES) {

    /*TODO check if has to update lst*/
    struct SERVER_EMAIL_RES_MSG email_msg;
    memcpy(&email_msg, mess, sizeof(email_msg));
    if (email_msg.exist == 0) {
      printf("\nthe email has already been deleted by other machine");
    } else {
      printf("\nto:      %s\nsubject: %s\n%s\n", email_msg.email.to,
             email_msg.email.subject, email_msg.email.content);
    }
    printf("\nUser> ");
    fflush(stdout);

  } else if (msg.type == DELETE_EMAIL_RES) {

    struct SERVER_DELETE_RES_MSG delete_email_res_msg;
    memcpy(&delete_email_res_msg, mess, sizeof(delete_email_res_msg));
    if (delete_email_res_msg.success == 0)
      printf("\nit's already been removed!\n");
    else
      printf("\nsuccessfully removed!\n");

    printf("\nUser> ");
    fflush(stdout);
  } else if (msg.type == MEMBERSHIP_RES) {
    struct SERVER_MEMBERSHIP_RES_MSG membership_res_msg;
    memcpy(&membership_res_msg, mess, sizeof(membership_res_msg));
    printf("\ncurrent member in group: ");
    for (int i = 0; i < 5; i++) {
      if (membership_res_msg.group_members[i] == 1) {
        printf("#%d ", i);
      }
    }
    printf("\n");
    printf("\nUser> ");
    fflush(stdout);
  } else if (msg.type == INFO_CHANGE) {
    email_num = -1;
    //printf("\nemail num(come) %d\n", email_num);
    //printf("\nUser> ");
    //fflush(stdout);
  } else if (msg.type == MEMBER_CHECK_RES) {

    struct SERVER_CHECK_MEMBER_RES_MSG check_member_res_msg;
    memcpy(&check_member_res_msg, mess, sizeof(check_member_res_msg));

    if (check_member_res_msg.group_members[last_connect_index] == 1) {

      // Join the public group of the designated server
      char public_group[80];
      strcpy(public_group, "public_group_");
      strcat(public_group, server_index);

      struct CLIENT_PRIVATE_GROUP_REQ_MSG private_group_req_msg;
      private_group_req_msg.msg.source.type = CLIENT;
      private_group_req_msg.msg.type = PRIVATE_GROUP_REQ;

      ret = SP_multicast(Mbox, AGREED_MESS, public_group, 0,
                         sizeof(private_group_req_msg),
                         (char *)&private_group_req_msg);

      if (ret < 0) {
        SP_error(ret);
        Bye();
      }
      email_num = -1;
      cur_server_index = last_connect_index;

    } else {
      printf("\nserver #%d can't be connected right now, you can try: ", last_connect_index);
      for (int i = 0; i < 5; i++) {
        if (check_member_res_msg.group_members[i] == 1) {
          printf("#%d ", i);
        }
      }
      printf("\n");
      printf("\nUser> ");
      fflush(stdout);
    }
  }

}

static void print_menu() {
  printf("\n");
  printf("==========\n");
  printf("User Menu:\n");
  printf("----------\n");
  printf("\n");

  printf("\tu <name>: login with a user name.\n");
  printf("\tc <server>: connect to a specific server(0 - 4).\n");
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
