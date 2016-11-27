#include "sp.h"
#include "global.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT "10580"
#define GLOBAL_GROUP "GLOBAL"

/**
struct EMAIL_MSG_NODE {
  struct EMAIL_MSG email_msg;
  struct EMAIL_MSG_NODE *next;
};

struct EMAIL_MSG {
  int server_id;
  int email_index;
  struct EMAIL email;
};

struct USER_NODE {
  char user_name[80];
  struct EMAIL_MSG_NODE email_node;
  struct USER_NODE *next;
};
*/

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;

void print_user_list(struct USER_NODE *user);
static void print_menu();
static void read_message();
static void Bye();

int main(int argc, char *argv[]) {
  static int index_matrix[5][5]; // Lamport index matrix
  struct USER_NODE *user_list_header = (struct USER_NODE*) malloc(sizeof(struct USER_NODE));
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

  // Test code
  /**
  char send_buf[80];
  strcpy(send_buf, "hello world....");
  ret = SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP, 0, sizeof(send_buf),
                     send_buf);
  **/

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

        // Join the private group
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
      } else if (client_msg.type == SEND_EMAIL) {
        struct CLIENT_SEND_EMAIL_MSG send_email_msg;
        memcpy(&send_email_msg, mess, sizeof(send_email_msg));
        // TODO: might have to use memcpy here
        char *user_name = send_email_msg.receiver_name;

        // Check if the receiver is in the server's user list
        struct EMAIL_MSG_NODE *user_email_head = NULL;
        struct USER_NODE *user_list_node = user_list_header;
        while (user_list_node->next) {
          user_list_node = user_list_node->next;
          if (strcmp(user_list_node->user_name, user_name) == 0) {
            user_email_head = &user_list_node->email_node;
            break;
          }
        }

        // Create a new mail node
        // TODO: add server_id and index to email_node, wrap with EMAIL_MSG
        struct EMAIL_MSG_NODE *new_email_node = (struct EMAIL_MSG_NODE*) malloc(sizeof(struct EMAIL_MSG_NODE));
        new_email_node->email_msg.email = send_email_msg.email;

        // Add a new user node if it is not in the list
        if (!user_email_head) {
          struct USER_NODE *new_user = (struct USER_NODE*) malloc(sizeof(struct EMAIL_MSG_NODE));
          strcpy(new_user->user_name, user_name);

          // Create a new header mail node for the new user
          struct EMAIL_MSG_NODE *new_email_head = (struct EMAIL_MSG_NODE*) malloc(sizeof(struct EMAIL_MSG_NODE));
          new_email_head->next = new_email_node;

          new_user->email_node = *new_email_head;
          user_list_node->next = new_user;

        } else {
          // Append the new mail to the end of the existing mail list
	  new_email_node->next = user_email_head->next;
          user_email_head->next = new_email_node;
        }

	print_user_list(user_list_header);
      }
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

static void print_menu() { printf("\n"); }

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}
