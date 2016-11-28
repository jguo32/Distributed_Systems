#include "sp.h"
#include "global.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "10580"
#define GLOBAL_GROUP "GLOBAL"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;

void print_email_list(struct EMAIL_MSG_NODE head);
void print_user_list(struct USER_NODE *user);
static void read_message();
static void Bye();

int main(int argc, char *argv[]) {
  static int index_matrix[5][5]; // Lamport index matrix
  static int email_counter = 0;

  struct USER_NODE *user_list_head =
      (struct USER_NODE *)malloc(sizeof(struct USER_NODE));
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

    struct SOURCE src;
    memcpy(&src, mess, sizeof(src));
    if (src.type == CLIENT) {
      struct CLIENT_MSG client_msg;
      memcpy(&client_msg, mess, sizeof(client_msg));

      /* Establish connection with client */
      if (client_msg.type == PRIVATE_GROUP_REQ) {
        // Build the private group name
        char private_group[80];
        strcpy(private_group, sender);
        strcat(private_group, "#");
        strcat(private_group, User);
        printf("private group name: %s\n", private_group);

        // Replace '#' by '_' in the group name
        for (int i = 0; i < 80 && private_group[i] != '\0'; i++) {
          if (private_group[i] == '#') {
            private_group[i] = '_';
          }
        }

        SP_join(Mbox, private_group);

        // Return the private group name to the client
        struct SERVER_PRIVATE_GROUP_RES_MSG server_response;
        server_response.msg.type = PRIVATE_GROUP_RES;
        strcpy(server_response.group_name, private_group);
        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                           sizeof(server_response), (char *)&server_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }
        /* Process client send email request */
      } else if (client_msg.type == SEND_EMAIL) {
        struct CLIENT_SEND_EMAIL_MSG send_email_msg;
        memcpy(&send_email_msg, mess, sizeof(send_email_msg));
        char *user_name = send_email_msg.receiver_name;

        // Check if the receiver is in the server's user list
        struct EMAIL_MSG_NODE *user_email_head = NULL;
        struct USER_NODE *user_list_node = user_list_head;
        while (user_list_node->next) {
          user_list_node = user_list_node->next;
          if (strcmp(user_list_node->user_name, user_name) == 0) {
            user_email_head = &user_list_node->email_node;
            break;
          }
        }

        // Create a new mail node
        // TODO: add email index to email_node, wrap with EMAIL_MSG
        struct EMAIL_MSG_NODE *new_email_node =
            (struct EMAIL_MSG_NODE *)malloc(sizeof(struct EMAIL_MSG_NODE));
        new_email_node->email_msg.email = send_email_msg.email;
        new_email_node->email_msg.email_index = ++email_counter;
        new_email_node->email_msg.server_index = atoi(server_index);

        // Add a new user node if it is not in the list
        if (!user_email_head) {
          struct USER_NODE *new_user =
              (struct USER_NODE *)malloc(sizeof(struct EMAIL_MSG_NODE));
          strcpy(new_user->user_name, user_name);

          // Create a new head mail node for the new user
          struct EMAIL_MSG_NODE *new_email_head =
              (struct EMAIL_MSG_NODE *)malloc(sizeof(struct EMAIL_MSG_NODE));
          new_email_head->next = new_email_node;

          new_user->email_node = *new_email_head;
          user_list_node->next = new_user;

        } else {
          // Append the new mail to the end of the existing mail list
          new_email_node->next = user_email_head->next;
          user_email_head->next = new_email_node;
        }

        // TODO: multicast to all other servers
        // TODO: write email/updates into disks

        print_user_list(user_list_head);

        /* Process list email request */
      } else if (client_msg.type == EMAIL_LIST_REQ) {
        struct CLIENT_EMAIL_LIST_REQ_MSG list_req;
        memcpy(&list_req, mess, sizeof(list_req));

        char *user_name = list_req.receiver_name;

        // Look up the user in the list
        struct EMAIL_MSG_NODE *user_email_head = NULL;
        struct USER_NODE *user_list_node = user_list_head;

        while (user_list_node) {
          if (strcmp(user_list_node->user_name, user_name) == 0) {
            user_email_head = &user_list_node->email_node;
            break;
          }
          user_list_node = user_list_node->next;
        }

        struct EMAIL_MSG email_list[EMAIL_LIST_MAX_LEN];
        int email_num = 0;

        if (!user_email_head) {
          printf("User %s not found!\n", user_name);
        } else {
          struct EMAIL_MSG_NODE *user_email_node = user_email_head->next;
          while (user_email_node) {
            email_list[email_num++] = user_email_node->email_msg;
            user_email_node = user_email_node->next;
          }
        }

        // Form & send the response message
        struct SERVER_EMAIL_LIST_RES_MSG list_response;
        list_response.msg.type = EMAIL_LIST_RES;
        memcpy(&list_response.email_list, email_list, sizeof(email_list));
        list_response.email_num = email_num;

        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0, sizeof(list_response),
                           (char *)&list_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }
        /* Process read request */
      } else if (client_msg.type == READ_EMAIL_REQ) {

        struct CLIENT_READ_EMAIL_MSG read_request;
        memcpy(&read_request, mess, sizeof(read_request));

        char *user_name = read_request.user_name;
        int email_index = read_request.email_index;
        int read_server_index = read_request.server_index;

        // Look up the user in the list
        struct EMAIL_MSG_NODE *user_email_head = NULL;
        struct USER_NODE *user_list_node = user_list_head;

        while (user_list_node) {
          if (strcmp(user_list_node->user_name, user_name) == 0) {
            user_email_head = &user_list_node->email_node;
            break;
          }
          user_list_node = user_list_node->next;
        }

        struct SERVER_EMAIL_RES_MSG read_response;
        read_response.msg.type = READ_EMAIL_RES;
        read_response.exist = 0;

        if (!user_email_head) {
          printf("User %s not found!\n", user_name);
        } else {
          struct EMAIL_MSG_NODE *user_email_node = user_email_head->next;
          while (user_email_node) {
            if (user_email_node->email_msg.server_index == read_server_index &&
                user_email_node->email_msg.email_index == email_index) {
              read_response.exist = 1;
              read_response.email = user_email_node->email_msg.email;
              break;
            }
            user_email_node = user_email_node->next;
          }
        }
        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0, sizeof(read_response),
                           (char *)&read_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

      } else if (client_msg.type == DELETE_EMAIL_REQ) {

      } else {
      }

      /* Process server request/updates */
    } else if (src.type == SERVER) {
      // Process update messages from other servers
    }

    if (Is_membership_mess(service_type)) {
      ret = SP_get_memb_info(mess, service_type, &memb_info);
      if (ret < 0) {
        printf("BUG: membership message does not have valid body\n");
        SP_error(ret);
        exit(1);
      }

      if (Is_reg_memb_mess(service_type)) {
        printf("Received REGULAR membership for group %s with %d members, "
               "where I am member %d:\n",
               sender, num_groups, mess_type);

        for (int i = 0; i < num_groups; i++)
          printf("\t%s\n", &target_groups[i][0]);

        /*
        printf("grp id is %d %d %d\n", memb_info.gid.id[0],
               memb_info.gid.id[1], memb_info.gid.id[2]);
        **/

        // Leave the group if it is a private group and current server is
        // the only member
        if (Is_caused_leave_mess(service_type) ||
            Is_caused_disconnect_mess(service_type)) {
          if (num_groups == 1 &&
              strncmp("_client", sender, strlen("_client")) == 0) {
            SP_leave(Mbox, sender);
            // printf("Left private group %s\n", sender);
          }
        }
      }
    }
  }
}

// Tester methods
void print_user_list(struct USER_NODE *user) {
  user = user->next;
  while (user) {
    printf("User %s in the list.\n", user->user_name);
    print_email_list(user->email_node);
    user = user->next;
  }
}

void print_email_list(struct EMAIL_MSG_NODE head) {
  struct EMAIL_MSG_NODE *cur = head.next;
  while (cur) {
    printf("Mail with content %s \n", cur->email_msg.email.content);
    cur = cur->next;
  }
}

static void read_message() {}

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}
