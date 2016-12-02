#include "sp.h"
#include "global.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX(x, y) (x > y ? x : y)
#define PORT "10580"
#define GLOBAL_GROUP_NAME "GLOBAL_SERVER_GROUP"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;

void print_index_matrix(int matrix[][5], int n);
void print_email_list(struct EMAIL_MSG_NODE head);
void print_user_list(struct USER_NODE *user);


void update_index_matrix(int index_matrix[5][5], int group_members[5],
                         int server_index); 
void add_new_email(struct EMAIL_MSG new_email_msg,
                   struct USER_NODE *user_list_head);
struct SERVER_EMAIL_RES_MSG read_new_email(char *user_name, int email_index,
                                           int read_server_index,
                                           struct USER_NODE *user_list_head);
int delete_email(char *user_name, int email_index, int server_index,
                 struct USER_NODE *user_list_head);

void add_update_msg(struct UPDATE_MSG update_msg, int server_index);
void delete_update_msg(int server_index, int update_index);
int check_time_index(int server_index1, int server_index2, int update_index_new,
                     int index_matrix[][5]);

// TODO
struct EMAIL_MSG lookup_email(int server_index, char *user_name);

struct UPDATE_MSG_NODE *update_msg_head[5] = {NULL};
struct UPDATE_MSG_NODE *update_msg_tail[5] = {NULL};

static void Bye();

int main(int argc, char *argv[]) {
  char *server_index;

  static char mess[MAX_MESSLEN];
  char target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
  char sender[MAX_GROUP_NAME];
  int ret;
  int service_type;
  int num_groups;
  int16 mess_type;
  int endian_mismatch;

  int index_matrix[5][5];
  int email_counter = 0;
  int time_stamp = 0;
  int *lamport_index;
  int group_members[5];

  /* Variables for partition */
  vs_set_info vssets[MAX_VSSETS];
  unsigned int my_vsset_index;
  int num_vs_sets;

  struct USER_NODE *user_list_head =
      (struct USER_NODE *)malloc(sizeof(struct USER_NODE));

  for (int i = 0; i < 5; i++) {
    update_msg_head[i] =
        (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
    update_msg_tail[i] =
        (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
    update_msg_head[i]->next = update_msg_tail[i];
    update_msg_tail[i]->pre = update_msg_head[i];
  }

  if (argc != 2) {
    printf("Usage: server <server_index>.\n");
    exit(0);
  }
  server_index = argv[1];
  lamport_index = &index_matrix[atoi(server_index)][atoi(server_index)];

  sprintf(User, "server_");
  strcat(User, server_index);
  sprintf(Spread_name, PORT);

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret != ACCEPT_SESSION) {
    SP_error(ret);
    Bye();
  }

  // Join global group with all servers in it
  ret = SP_join(Mbox, GLOBAL_GROUP_NAME);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }
  printf("Successfully joined global group.\n");

  // Join public group with only current server in it
  char public_group[80];
  strcpy(public_group, "public_group_");
  strcat(public_group, server_index);
  ret = SP_join(Mbox, public_group);
  if (ret < 0) {
    SP_error(ret);
    Bye();
  }

  /* Initialize group members, all servers are in the same group at beginning */
  memset(group_members, 1, sizeof(int) * 5);

  /* Initialize index matrix */
  memset(index_matrix, 0, sizeof(int) * 25);

  printf("Server #%s successfully launched, public group name: %s.\n",
         server_index, public_group);

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
        char private_group[80];
        strcpy(private_group, sender);
        strcat(private_group, "#");
        strcat(private_group, User);
        printf("private group name: %s\n", private_group);

        /* Replace '#' by '_' in the group name */
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

        // TODO: write email/updates into disks

        /* Update all index value that the servers in same group */
        email_counter += 1;
        time_stamp += 1;

	update_index_matrix(index_matrix, group_members, atoi(server_index));
	// TODO: append the update message to the list

        /* Create a new email msg */
        struct EMAIL_MSG email_msg;
        email_msg.email = send_email_msg.email;
        email_msg.email_index = email_counter;
        email_msg.server_index = atoi(server_index);

        add_new_email(email_msg, user_list_head);

        print_user_list(user_list_head);

        /*create update message, mutlicast to group*/
        struct NEW_EMAIL_MSG new_email_msg;
        new_email_msg.update_msg.source.type = SERVER;
        new_email_msg.update_msg.type = NEW_EMAIL;
        new_email_msg.update_msg.time_stamp = time_stamp;
        new_email_msg.update_msg.update_index = *lamport_index;
        new_email_msg.update_msg.server_index = atoi(server_index);
        new_email_msg.update_msg.email_index = email_counter;
        new_email_msg.email = send_email_msg.email;
        memcpy(new_email_msg.update_msg.user_name, send_email_msg.receiver_name,
               sizeof(send_email_msg.receiver_name));

        // multicast this update message to all other servers
        ret = SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP_NAME, 0,
                           sizeof(new_email_msg), (char *)&new_email_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

        /* Process list email request */
      } else if (client_msg.type == EMAIL_LIST_REQ) {
        struct CLIENT_EMAIL_LIST_REQ_MSG list_req;
        memcpy(&list_req, mess, sizeof(list_req));

        char *user_name = list_req.receiver_name;

        /* Look up the user in the list */
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

        /* Form & send the response message */
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
        print_user_list(user_list_head);

      } else if (client_msg.type == READ_EMAIL_REQ) {

        struct CLIENT_READ_EMAIL_MSG read_request;
        memcpy(&read_request, mess, sizeof(read_request));

        struct SERVER_EMAIL_RES_MSG read_response;
        read_response =
            read_new_email(read_request.user_name, read_request.email_index,
                           read_request.server_index, user_list_head);

        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0, sizeof(read_response),
                           (char *)&read_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

        time_stamp += 1;
        //*lamport_index += 1;
	update_index_matrix(index_matrix, group_members, atoi(server_index));

        /*create update message, and multicast to group*/
        struct UPDATE_MSG update_read_msg;
        update_read_msg.source.type = SERVER;
        update_read_msg.type = READ_EMAIL;
        update_read_msg.time_stamp = time_stamp;
        update_read_msg.update_index = *lamport_index;
        update_read_msg.server_index = atoi(server_index);
        update_read_msg.email_index = read_request.email_index;
        update_read_msg.email_server_index = read_request.server_index;
        memcpy(update_read_msg.user_name, read_request.user_name,
               sizeof(read_request.user_name));

        ret = SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP_NAME, 0,
                           sizeof(update_read_msg), (char *)&update_read_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

      } else if (client_msg.type == DELETE_EMAIL_REQ) {
        struct CLIENT_DELETE_EMAIL_MSG delete_request;
        memcpy(&delete_request, mess, sizeof(delete_request));

        struct SERVER_DELETE_RES_MSG delete_response;
        delete_response.msg.type = DELETE_EMAIL_RES;
        delete_response.success =
            delete_email(delete_request.user_name, delete_request.email_index,
                         delete_request.server_index, user_list_head);

        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                           sizeof(delete_response), (char *)&delete_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

        time_stamp += 1;
        //lamport_index += 1;
	update_index_matrix(index_matrix, group_members, atoi(server_index));

        /* create update message, and multicast to group*/
        struct UPDATE_MSG update_delete_msg;
        update_delete_msg.source.type = SERVER;
        update_delete_msg.type = DELETE_EMAIL;
        update_delete_msg.time_stamp = time_stamp;
        update_delete_msg.update_index = *lamport_index;
        update_delete_msg.server_index = atoi(server_index);
        update_delete_msg.email_index = delete_request.email_index;
        update_delete_msg.email_server_index = delete_request.server_index;
        memcpy(update_delete_msg.user_name, delete_request.user_name,
               sizeof(delete_request.user_name));

        ret =
            SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP_NAME, 0,
                         sizeof(update_delete_msg), (char *)&update_delete_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

      } else {
      }

      /* Process server request/updates */
    } else if (src.type == SERVER) {
      /* Unpack server update message */
      struct UPDATE_MSG update_msg;
      memcpy(&update_msg, mess, sizeof(update_msg));

      /* Update local Lamport timestamp first */
      time_stamp = MAX(time_stamp, update_msg.time_stamp);

      if (update_msg.type == EXCHANGE_INDEX_MATRIX) {
        /* Form exchange message to update matrix */
        struct EXCHANGE_INDEX_MATRIX_MSG exchange_index_msg;
        memcpy(&exchange_index_msg, mess, sizeof(exchange_index_msg));

        int local_server_index = atoi(server_index);
        int incoming_server_index = exchange_index_msg.update_msg.server_index;
        int incoming_matrix[5][5];
        memcpy(&incoming_matrix, exchange_index_msg.index_matrix,
               sizeof(incoming_matrix));
        incoming_matrix = exchange_index_msg.index_matrix;

        for (int i = 0; i < 5; i++) {
          if (index_matrix[local_server_index][i] >
              incoming_matrix[incoming_server_index][i]) {
            // TODO: send update message at [local_server_index][i]
            // Sort the update messages by timestamp and send them one by one
          }
        }

        /* tester */

        /* Update other field of the matrix */
        for (int i = 0; i < 5; i++) {
          if (i == local_server_index) {
            continue;
          }
          for (int j = 0; j < 5; j++) {
            index_matrix[i][j] = MAX(index_matrix[i][j], incoming_matrix[i][j]);
          }
        }

      } else if (update_msg.type == NEW_EMAIL) {
        struct NEW_EMAIL_MSG new_email_msg;
        memcpy(&new_email_msg, mess, sizeof(new_email_msg));

        /* check time_stamp & update_index */
        if (check_time_index(atoi(server_index), update_msg.server_index,
                             update_msg.update_index, index_matrix)) {

          struct EMAIL_MSG email_msg;
          email_msg.server_index = new_email_msg.update_msg.server_index;
          email_msg.email_index = new_email_msg.update_msg.email_index;
          email_msg.email = new_email_msg.email;

          add_new_email(email_msg, user_list_head);

          print_user_list(user_list_head);
        }
      } else if (update_msg.type == READ_EMAIL) {

        /* check */
        if (check_time_index(atoi(server_index), update_msg.server_index,
                             update_msg.update_index, index_matrix)) {

          read_new_email(update_msg.user_name, update_msg.email_index,
                         update_msg.email_server_index, user_list_head);

          print_user_list(user_list_head);
        }

      } else if (update_msg.type == DELETE_EMAIL) {

        if (check_time_index(atoi(server_index), update_msg.server_index,
                             update_msg.update_index, index_matrix)) {

          // printf("delete email\n");

          delete_email(update_msg.user_name, update_msg.email_index,
                       update_msg.email_server_index, user_list_head);
        }

        // printf("matrix after update:\n");
        // print_index_matrix(index_matrix, 5);
      }
    }

    /* Actions when there is membership change */
    if (Is_membership_mess(service_type)) {
      ret = SP_get_memb_info(mess, service_type, &memb_info);
      if (ret < 0) {
        printf("BUG: membership message does not have valid body\n");
        SP_error(ret);
        exit(1);
      }

      if (Is_reg_memb_mess(service_type)) {
        /**
           printf("Received REGULAR membership for group %s with %d members, "
           "where I am member %d:\n",
           sender, num_groups, mess_type);

           for (int i = 0; i < num_groups; i++)
           printf("\t%s\n", &target_groups[i][0]);
        */

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
        } else if (Is_caused_network_mess(service_type)) {
          printf("Due to NETWORK change with %u VS sets\n",
                 memb_info.num_vs_sets);
          num_vs_sets = SP_get_vs_sets_info(mess, &vssets[0], MAX_VSSETS,
                                            &my_vsset_index);
          if (num_vs_sets < 0) {
            printf("BUG: membership message has more then %d vs sets. "
                   "Recompile with larger MAX_VSSETS\n",
                   MAX_VSSETS);
            SP_error(num_vs_sets);
            exit(1);
          }

          /* Multicast index_matrix to current group */
          struct EXCHANGE_INDEX_MATRIX_MSG exchange_msg;
          exchange_msg.update_msg.source.type = SERVER;
          exchange_msg.update_msg.type = EXCHANGE_INDEX_MATRIX;
          memcpy(&exchange_msg.index_matrix, index_matrix,
                 sizeof(index_matrix));
          ret = SP_multicast(Mbox, AGREED_MESS, sender, 0, sizeof(exchange_msg),
                             (char *)&exchange_msg);
          if (ret < 0) {
            SP_error(ret);
            printf("\nBye.\n");
            exit(0);
          }
        }
      }
    }
  }
}

// Tester methods
void print_index_matrix(int matrix[][5], int m) {
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < 5; j++) {
      printf("%d ", matrix[i][j]);
    }
    printf("\n");
  }
}

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
    printf("Mail(read:%d) with content %s \n", cur->email_msg.email.read,
           cur->email_msg.email.content);
    cur = cur->next;
  }
}

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}

void update_index_matrix(int index_matrix[5][5], int group_members[5],
                         int server_index) {
  for (int i = 0; i < 5; i++) {
    if (group_members[i] == 1) {
      index_matrix[i][server_index] += 1;
    }
  }
}

int check_time_index(int server_index1, int server_index2, int update_index_new,
                     int index_matrix[][5]) {

  printf("~~~~~~~~~~~~\n");
  //  printf("server index1 %d\n", server_index1);
  //  printf("server index2 %d\n", server_index2);
  //  printf("time cur %d\n", time_stamp_cur);
  //  printf("time new %d\n", time_stamp_new);

  /*
  printf("%d, %d, %d, %d, %d, %d\n", server_index1, server_index2,
  time_stamp_new,
         time_stamp_cur, update_index_new,
         index_matrix[server_index1][server_index2]);*/

  if (server_index1 == server_index2)
    return 0;

  // if (update_index_new <= index_matrix[server_index1][server_index2])
  // return 0;

  return 1;
}

struct EMAIL_MSG_NODE *find_user_email_node(char *user_name,
                                            struct USER_NODE *user_list_head) {
  struct EMAIL_MSG_NODE *user_email_head = NULL;
  struct USER_NODE *user_list_node = user_list_head;
  while (user_list_node->next) {
    user_list_node = user_list_node->next;
    if (strcmp(user_list_node->user_name, user_name) == 0) {
      user_email_head = &user_list_node->email_node;
      break;
    }
  }
  return user_email_head;
}

void add_new_email(struct EMAIL_MSG new_email_msg,
                   struct USER_NODE *user_list_head) {

  // Check if the receiver is in the server's user list
  char *user_name = new_email_msg.email.to;
  struct EMAIL_MSG_NODE *user_email_head = NULL;
  struct USER_NODE *user_list_node = user_list_head;
  while (user_list_node->next) {
    user_list_node = user_list_node->next;
    if (strcmp(user_list_node->user_name, user_name) == 0) {
      user_email_head = &user_list_node->email_node;
      break;
    }
  }
  // Create a new email node
  struct EMAIL_MSG_NODE *new_email_node =
      (struct EMAIL_MSG_NODE *)malloc(sizeof(struct EMAIL_MSG_NODE));
  new_email_node->email_msg = new_email_msg;

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
}

struct SERVER_EMAIL_RES_MSG read_new_email(char *user_name, int email_index,
                                           int read_server_index,
                                           struct USER_NODE *user_list_head) {
  struct EMAIL_MSG_NODE *user_email_head = NULL;
  user_email_head = find_user_email_node(user_name, user_list_head);

  struct SERVER_EMAIL_RES_MSG read_response;
  read_response.msg.type = READ_EMAIL_RES;
  read_response.exist = 0;

  if (!user_email_head) {
    printf("User %s not found!\n", user_name);
  } else {
    struct EMAIL_MSG_NODE *user_email_node = user_email_head->next;
    while (user_email_node) {
      // printf("server_index: %d\n", user_email_node->email_msg.server_index);
      // printf("email_index: %d\n",  user_email_node->email_msg.email_index);
      if (user_email_node->email_msg.server_index == read_server_index &&
          user_email_node->email_msg.email_index == email_index) {
        printf("read - found email \n");
        user_email_node->email_msg.email.read = 1;
        read_response.exist = 1;
        read_response.email = user_email_node->email_msg.email;
        break;
      }
      user_email_node = user_email_node->next;
    }
  }
  return read_response;
}

int delete_email(char *user_name, int delete_email_index,
                 int delete_server_index, struct USER_NODE *user_list_head) {
  int res = 0;
  struct EMAIL_MSG_NODE *user_email_head = NULL;
  user_email_head = find_user_email_node(user_name, user_list_head);

  if (user_email_head) {
    struct EMAIL_MSG_NODE *user_email_node = user_email_head;
    while (user_email_node->next) {
      struct EMAIL_MSG cur_email_msg = user_email_node->next->email_msg;
      if (cur_email_msg.server_index == delete_server_index &&
          cur_email_msg.email_index == delete_email_index) {
        user_email_node->next = user_email_node->next->next;
        res = 1;
        break;
      }
      user_email_node = user_email_node->next;
    }
  }

  return res;
}

void add_update_msg(struct UPDATE_MSG update_msg, int server_index) {

  struct UPDATE_MSG_NODE *update_msg_node =
      (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
  update_msg_node->update_msg = update_msg;
  update_msg_node->pre = update_msg_head[server_index];
  update_msg_node->next = update_msg_head[server_index]->next;
  update_msg_head[server_index]->next = update_msg_node;
  update_msg_node->next->pre = update_msg_node;
}

void delete_update_msg(int server_index, int update_index) {
  int find = 0;
  struct UPDATE_MSG_NODE *temp_msg = update_msg_tail[server_index]->pre;
  while (temp_msg->update_msg.update_index != update_index) {
    temp_msg = temp_msg->pre;
    find = 1;
  }
  if (find == 1) {
    temp_msg->pre->next = temp_msg->next;
    temp_msg->next->pre = temp_msg->pre;
    free(temp_msg);
  }
}
