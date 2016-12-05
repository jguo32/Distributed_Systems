#include "sp.h"
#include "global.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>

#define MAX(x, y) (x > y ? x : y)
#define PORT "10580"
#define GLOBAL_GROUP_NAME "GLOBAL_SERVER_GROUP"

static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static mailbox Mbox;

int mkdir(const char *pathname, mode_t mode);

void print_index_matrix(int matrix[5][5]);
void print_email_list(struct EMAIL_MSG_NODE head);
void print_user_list(struct USER_NODE *user);
void print_update_msg_list();

void increment_index_matrix(int index_matrix[5][5], int group_members[5],
                            int server_index);
void update_index_matrix(int index_matrix[5][5], int group_members[5],
                         int server_index, int update_index);

void add_new_email(struct EMAIL_MSG new_email_msg,
                   struct USER_NODE *user_list_head);
struct SERVER_EMAIL_RES_MSG read_new_email(char *user_name, int email_index,
                                           int read_server_index,
                                           struct USER_NODE *user_list_head);
int delete_email(char *user_name, int email_index, int server_index,
                 struct USER_NODE *user_list_head);

void add_update_msg(struct UPDATE_MSG update_msg);
void delete_update_msg(int server_index, int update_index);
int check_time_index(int server_index1, int server_index2, int update_index_new,
                     int index_matrix[][5]);

void add_update_msg_lst(struct UPDATE_MSG_NODE *head, int server_index,
                        int update_index);
struct EMAIL_MSG_NODE *find_user_email_head(char *user_name,
                                            struct USER_NODE *user_list_head);
struct EMAIL_MSG_NODE *
find_user_email_node(struct EMAIL_MSG_NODE *user_email_head, int email_index,
                     int email_server_index);
struct EMAIL_MSG_NODE *find_email(struct USER_NODE *user_list_head,
                                  char *user_name, int email_index,
                                  int email_server_index);

struct UPDATE_MSG create_update_msg(char type, int time_stamp,
                                    int lamport_index, int server_index,
                                    int email_index, int email_server_index,
                                    char *user_name);

void write_index_matrix(int server_index, int index_matrix[5][5],
                        int time_stamp);
void read_index_matrix(int server_index, int mat[5][5], int *time_stamp);

void write_email(char *file_name, struct EMAIL_MSG email_msg);
struct EMAIL_MSG read_email(char *file_name);
void read_all_email(char *dir, struct USER_NODE *user_list_head);

void write_update_msg(int server_index);
void read_update_msg(int server_index);

void delete_email_on_disk(int server_index, char *file_name);

void testWrite();

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

  struct USER_NODE *user_list_head =
      (struct USER_NODE *)malloc(sizeof(struct USER_NODE));

  for (int i = 0; i < 5; i++) {
    update_msg_head[i] =
        (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
    update_msg_tail[i] =
        (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
    update_msg_tail[i]->next = NULL;

    update_msg_head[i]->next = update_msg_tail[i];
    update_msg_tail[i]->pre = update_msg_head[i];
    update_msg_head[i]->update_msg.update_index =
        update_msg_tail[i]->update_msg.update_index = -1;
  }

  if (argc != 2) {
    printf("Usage: server <server_index>.\n");
    exit(0);
  }
  server_index = argv[1];
  lamport_index = &index_matrix[atoi(server_index)][atoi(server_index)];

  mkdir("./data", 0700);
  char dir_root[DIR_LEN];
  char dir_emails[DIR_LEN];
  sprintf(dir_root, "./data/s%d", atoi(server_index));
  mkdir(dir_root, 0700);
  sprintf(dir_emails, "./data/s%d/emails", atoi(server_index));
  mkdir(dir_emails, 0700);

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
  for (int i = 0; i < 5; i++) {
    group_members[i] = 0;
  }
  group_members[atoi(server_index)] = 1;

  /*
    char group_check[1];
    ret =
    SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP_NAME, 0,
    1, group_check);

    ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups,
    &mess_type, &endian_mismatch, sizeof(mess), mess);

    for (int j = 0; j < num_groups; j++) {
    printf("Member: %s\n", target_groups[j]);
    char member_name[20];
    char *token;
    int existing_server_index;

    memcpy(member_name, &target_groups[j][1],
    strlen(target_groups[j]) - 1);
    token = strtok(member_name, "#");
    existing_server_index = token[strlen(token) - 1] - '0';
    group_members[existing_server_index] = 1;
    }
  */

  /* Initialize index matrix */
  memset(index_matrix, 0, sizeof(int) * 25);

  /* read data file from disk*/
  read_index_matrix(atoi(server_index), index_matrix, &time_stamp);
  read_all_email(dir_emails, user_list_head);
  read_update_msg(atoi(server_index));

  /* check */
  print_index_matrix(index_matrix);
  print_user_list(user_list_head);
  print_update_msg_list();

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

        /* Update all index value that the servers in same group */
        email_counter += 1;
        time_stamp += 1;

        /* Increment the corresponding position of the index matrix */
        increment_index_matrix(index_matrix, group_members, atoi(server_index));

        /* Create a new email msg */
        struct EMAIL_MSG email_msg;
        email_msg.email = send_email_msg.email;
        email_msg.email_index = email_counter;
        email_msg.server_index = atoi(server_index);
        email_msg.time_stamp = time_stamp;

        add_new_email(email_msg, user_list_head);

        // read_email(file_name);

        /* notify the user the email info has been changed */
          struct SERVER_INFO_CHANGE_MSG info_change_msg;
        info_change_msg.msg.type = INFO_CHANGE;

        ret = SP_multicast(Mbox, AGREED_MESS, send_email_msg.email.to, 0,
                           sizeof(info_change_msg), (char *)&info_change_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

        /*create update message, mutlicast to group*/
        struct UPDATE_MSG update_msg = create_update_msg(
            NEW_EMAIL, time_stamp, *lamport_index, atoi(server_index),
            email_counter, atoi(server_index), send_email_msg.receiver_name);

        struct NEW_EMAIL_MSG new_email_msg;

        new_email_msg.update_msg = update_msg;
        new_email_msg.email = send_email_msg.email;

        /* Append the update message to the list */
        add_update_msg(update_msg);

        /* write data to disk */
        write_index_matrix(atoi(server_index), index_matrix, time_stamp);
        write_update_msg(atoi(server_index));

        char file_name[FILENAME_LEN];
        sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                email_msg.email.to, email_msg.server_index,
                email_msg.email_index);
        write_email(file_name, email_msg);

        print_user_list(user_list_head);

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
        // print_user_list(user_list_head);
        // print_email_list(*user_email_head);

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
        // print_user_list(user_list_head);

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
        increment_index_matrix(index_matrix, group_members, atoi(server_index));

        /*create update message, and multicast to group*/
        struct UPDATE_MSG update_read_msg = create_update_msg(
            READ_EMAIL, time_stamp, *lamport_index, atoi(server_index),
            read_request.email_index, read_request.server_index,
            read_request.user_name);

        /* Append the update message to the list */
        add_update_msg(update_read_msg);

        /* Write date to disk */
        write_index_matrix(atoi(server_index), index_matrix, time_stamp);
        write_update_msg(atoi(server_index));

        char file_name[FILENAME_LEN];
        sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                read_request.user_name, read_request.server_index,
                read_request.email_index);

        struct EMAIL_MSG_NODE *email_msg_node =
            find_email(user_list_head, read_request.user_name,
                       read_request.email_index, read_request.server_index);

        write_email(file_name, email_msg_node->email_msg);

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
        increment_index_matrix(index_matrix, group_members, atoi(server_index));
        write_index_matrix(atoi(server_index), index_matrix, time_stamp);

        /* notify the user the email info has been changed */
        struct SERVER_INFO_CHANGE_MSG info_change_msg;
        info_change_msg.msg.type = INFO_CHANGE;

        ret = SP_multicast(Mbox, AGREED_MESS, delete_request.user_name, 0,
                           sizeof(info_change_msg), (char *)&info_change_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

        /* create update message, and multicast to group*/
        struct UPDATE_MSG update_delete_msg = create_update_msg(
            DELETE_EMAIL, time_stamp, *lamport_index, atoi(server_index),
            delete_request.email_index, delete_request.server_index,
            delete_request.user_name);
        /* Append the update message to the list */
        add_update_msg(update_delete_msg);

        /* Write date to disk */
        write_index_matrix(atoi(server_index), index_matrix, time_stamp);
        write_update_msg(atoi(server_index));

        char file_name[FILENAME_LEN];
        sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                delete_request.user_name, delete_request.server_index,
                delete_request.email_index);
        delete_email_on_disk(atoi(server_index), file_name);

        ret =
            SP_multicast(Mbox, AGREED_MESS, GLOBAL_GROUP_NAME, 0,
                         sizeof(update_delete_msg), (char *)&update_delete_msg);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

      } else if (client_msg.type == MEMBERSHIP_REQ) {

        struct SERVER_MEMBERSHIP_RES_MSG membership_response;
        membership_response.msg.type = MEMBERSHIP_RES;
        memcpy(membership_response.group_members, group_members,
               sizeof(int) * 5);

        ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                           sizeof(membership_response),
                           (char *)&membership_response);
        if (ret < 0) {
          SP_error(ret);
          printf("\nBye.\n");
          exit(0);
        }

      } else if (client_msg.type == MEMBER_CHECK_REQ) {

        printf("asdf\n");
        for (int i = 0; i < 5; i ++) {
          if (group_members[i] == 1) {
            printf("%d", i);
            if (i == atoi(server_index)) {
              printf("sent\n");
              struct SERVER_CHECK_MEMBER_RES_MSG check_member_res_msg;
              check_member_res_msg.msg.type = MEMBER_CHECK_RES;
              memcpy(check_member_res_msg.group_members, group_members, sizeof(int)*5);

              ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                                 sizeof(check_member_res_msg),
                                 (char *)&check_member_res_msg);
              if (ret < 0) {
                SP_error(ret);
                printf("\nBye.\n");
                exit(0);
              }
            }

            break;
          }
        }
      }

      // print_update_msg_list();
      // print_index_matrix(index_matrix);

      /* Process server request/updates */
    } else if (src.type == SERVER) {
      /* Unpack server update message */
      struct UPDATE_MSG update_msg;
      memcpy(&update_msg, mess, sizeof(update_msg));

      printf("\n\n~~~~~~~~~~~~~~~~\n");

      /* Append the update message to the list if it is not EXCHANGE */
      if (update_msg.type != EXCHANGE_INDEX_MATRIX &&
          check_time_index(atoi(server_index), update_msg.server_index,
                           update_msg.update_index, index_matrix)) {
        add_update_msg(update_msg);
        /* Update local Lamport timestamp first */
        time_stamp = MAX(time_stamp, update_msg.time_stamp);
      }

      if (update_msg.type == EXCHANGE_INDEX_MATRIX) {
        printf("Exchange msg!\n");
      }
      printf("index matrix before update:\n");
      print_index_matrix(index_matrix);

      printf("Received update msg: type: %c,  server_index: %d, update_index: "
             "%d, time_stamp: %d.\n",
             update_msg.type, update_msg.server_index, update_msg.update_index,
             update_msg.time_stamp);

      if (check_time_index(atoi(server_index), update_msg.server_index,
                           update_msg.update_index, index_matrix) ||
          update_msg.type == EXCHANGE_INDEX_MATRIX) {
	printf("Starting update...\n");

        if (update_msg.type == EXCHANGE_INDEX_MATRIX) {
          /* Form exchange message to update matrix */
          struct EXCHANGE_INDEX_MATRIX_MSG exchange_index_msg;
          memcpy(&exchange_index_msg, mess, sizeof(exchange_index_msg));

          int local_server_index = atoi(server_index);
          int incoming_server_index =
              exchange_index_msg.update_msg.server_index;
          int incoming_matrix[5][5];

          memcpy(incoming_matrix, exchange_index_msg.index_matrix,
                 sizeof(incoming_matrix));

          printf("incoming matrix\n");
          print_index_matrix(incoming_matrix);

          /*
          for (int i = 0; i < 5; i++) {
            if (i == local_server_index) {
              continue;
            }
            for (int j = 0; j < 5; j++) {
              index_matrix[i][j] =
                  MAX(index_matrix[i][j], incoming_matrix[i][j]);
            }
          }
          */

          /* Update other field of the matrix */
          for (int i = 0; i < 5; i++) {
            if (i == local_server_index) {
              continue;
            }
            for (int j = 0; j < 5; j++) {
              if (group_members[i] == 1) {
                index_matrix[i][j] =
                    MAX(index_matrix[i][j],
                        incoming_matrix[incoming_server_index][j]);
              } else {
                index_matrix[i][j] =
                    MAX(index_matrix[i][j], incoming_matrix[i][j]);
              }
            }
          }

          /* check the update_msg that needs to be sent out*/
          struct UPDATE_MSG_NODE *update_msg_head =
              (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
          update_msg_head->next = NULL;

          // Check each column of current server_index and incoming server index
          for (int j = 0; j < 5; j++) {
            if (index_matrix[local_server_index][j] >
                incoming_matrix[incoming_server_index][j]) {

              // send update message at [local_server_index][j]
              /**
                 for (int k = incoming_matrix[incoming_server_index][j] + 1;
                 k <= index_matrix[local_server_index][j]; k++) {
              */
              index_matrix[incoming_server_index][j] =
                  incoming_matrix[incoming_server_index][j];

              int update_start_point;
              update_start_point =
                  incoming_matrix[incoming_server_index][j] + 1;

              while (update_start_point <=
                     index_matrix[local_server_index][j]) {
                // index_matrix[incoming_server_index][j] += 1;
                add_update_msg_lst(update_msg_head, j, update_start_point);
                update_start_point += 1;
              }
            }
          }

          write_index_matrix(atoi(server_index), index_matrix, time_stamp);

          struct UPDATE_MSG_NODE *test = update_msg_head->next;

          while (test) {
            if (test->update_msg.update_index > 0) {
              printf("Update msg: type: %c, update_index: %d, server_index: "
                     "%d, email_index: %d.\n",
                     test->update_msg.type, test->update_msg.update_index,
                     test->update_msg.server_index,
                     test->update_msg.email_index);
            }

            test = test->next;
          }

          /*send out the update_msg*/
          while (update_msg_head->next) {

            // Sort the update messages by timestamp and send them one by one
            struct UPDATE_MSG_NODE *update_msg_next = update_msg_head->next;

            // printf("update msg next: update_msg index: %d.\n",
            // update_msg_next->update_msg.update_index);

            // multicast this update message to all other servers
            if (update_msg_next->update_msg.type == NEW_EMAIL) {

              struct EMAIL_MSG_NODE *email_msg_node = find_email(
                  user_list_head, update_msg_next->update_msg.user_name,
                  update_msg_next->update_msg.email_index,
                  update_msg_next->update_msg.email_server_index);

              struct NEW_EMAIL_MSG new_email_msg;
              new_email_msg.update_msg = update_msg_next->update_msg;
              new_email_msg.email = email_msg_node->email_msg.email;

              // multicast this update message to all other servers
              ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                                 sizeof(new_email_msg), (char *)&new_email_msg);
              if (ret < 0) {
                SP_error(ret);
                printf("\nBye.\n");
                exit(0);
              }

            } else {

              struct UPDATE_MSG update_msg = update_msg_next->update_msg;

              ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                                 sizeof(update_msg), (char *)&update_msg);
              if (ret < 0) {
                SP_error(ret);
                printf("\nBye.\n");
                exit(0);
              }
            }

            index_matrix[incoming_server_index][update_msg_next->update_msg
                                                    .server_index] =
                MAX(index_matrix[incoming_server_index]
                                [update_msg_next->update_msg.server_index],
                    update_msg_next->update_msg.update_index);

            update_msg_head->next = update_msg_next->next;

            free(update_msg_next);
            update_msg_next = NULL;
          }

          free(update_msg_head);
          update_msg_head = NULL;

          // TODO: notify the client to re-list mail when there is an update
        } else if (update_msg.type == NEW_EMAIL) {
          struct NEW_EMAIL_MSG new_email_msg;
          memcpy(&new_email_msg, mess, sizeof(new_email_msg));

          struct EMAIL_MSG email_msg;
          email_msg.server_index = new_email_msg.update_msg.email_server_index;
          email_msg.email_index = new_email_msg.update_msg.email_index;
          email_msg.email = new_email_msg.email;
          email_msg.time_stamp = new_email_msg.update_msg.time_stamp;

          add_new_email(email_msg, user_list_head);

          update_index_matrix(index_matrix, group_members,
                              update_msg.server_index, update_msg.update_index);

          /* Write data to disk */
          write_index_matrix(atoi(server_index), index_matrix, time_stamp);
          write_update_msg(atoi(server_index));

          char file_name[FILENAME_LEN];
          sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                  email_msg.email.to, email_msg.server_index,
                  email_msg.email_index);
          write_email(file_name, email_msg);

          print_user_list(user_list_head);
        } else if (update_msg.type == READ_EMAIL) {
          /* check */

          /**
             printf("username: %s, email_index: %d, email server index: %d.\n",
             update_msg.user_name, update_msg.email_index,
             update_msg.email_server_index);
          */
          read_new_email(update_msg.user_name, update_msg.email_index,
                         update_msg.email_server_index, user_list_head);
          update_index_matrix(index_matrix, group_members,
                              update_msg.server_index, update_msg.update_index);

          /* Write data to disk */
          write_index_matrix(atoi(server_index), index_matrix, time_stamp);
          write_update_msg(atoi(server_index));

          char file_name[FILENAME_LEN];
          sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                  update_msg.user_name, update_msg.email_server_index,
                  update_msg.email_index);

          struct EMAIL_MSG_NODE *email_msg_node =
              find_email(user_list_head, update_msg.user_name,
                         update_msg.email_index, update_msg.email_server_index);

          write_email(file_name, email_msg_node->email_msg);

          print_user_list(user_list_head);

        } else if (update_msg.type == DELETE_EMAIL) {

          delete_email(update_msg.user_name, update_msg.email_index,
                       update_msg.email_server_index, user_list_head);
          update_index_matrix(index_matrix, group_members,
                              update_msg.server_index, update_msg.update_index);

          /* Write data to disk */
          write_index_matrix(atoi(server_index), index_matrix, time_stamp);
          write_update_msg(atoi(server_index));

          char file_name[FILENAME_LEN];
          sprintf(file_name, "./data/s%d/emails/%s_s%d_%d", atoi(server_index),
                  update_msg.user_name, update_msg.email_server_index,
                  update_msg.email_index);
          delete_email_on_disk(atoi(server_index), file_name);

        } else {
        }
      }

      print_update_msg_list();

      /* discard unused update messages*/
      for (int j = 0; j < 5; j++) {
        int aru = index_matrix[0][j];
        for (int i = 1; i < 5; i++) {
          aru = (aru < index_matrix[i][j] ? aru : index_matrix[i][j]);
        }
        delete_update_msg(j, aru);
      }
      /* write update message into disk after delete*/
      write_update_msg(atoi(server_index));

      printf("After delete:\n");
      print_update_msg_list();
      printf("index_matrix after update from %d\n", update_msg.server_index);
      print_index_matrix(index_matrix);
    }

    for (int i = 0; i < num_groups; i++)
      printf("\t%s\n", &target_groups[i][0]);

    /* Actions when there is membership change */
    if (Is_membership_mess(service_type)) {
      ret = SP_get_memb_info(mess, service_type, &memb_info);
      if (ret < 0) {
        printf("BUG: membership message does not have valid body\n");
        SP_error(ret);
        exit(1);
      }

      if (strcmp(sender, GLOBAL_GROUP_NAME) == 0) {
        // Zero out the group member array
        for (int i = 0; i < 5; i++) {
          group_members[i] = 0;
        }

        // Fill the membership array by current members
        for (int j = 0; j < num_groups; j++) {
          printf("Member: %s\n", target_groups[j]);
          char member_name[20];
          char *token;
          int existing_server_index;

          memcpy(member_name, &target_groups[j][1],
                 strlen(target_groups[j]) - 1);
          token = strtok(member_name, "#");
          existing_server_index = token[strlen(token) - 1] - '0';
          group_members[existing_server_index] = 1;
        }
      }

      if (Is_reg_memb_mess(service_type)) {
        // Leave the group if it is a private group and current server is
        // the only member
        if (Is_caused_leave_mess(service_type) ||
            Is_caused_disconnect_mess(service_type)) {

          if (num_groups == 1 &&
              strncmp("_client", sender, strlen("_client")) == 0) {
            SP_leave(Mbox, sender);
          }
        } else if (Is_caused_network_mess(service_type)) {

          // Update index matrix and update list only if membership of
          // global group changes
          if (strcmp(sender, GLOBAL_GROUP_NAME) == 0) {

            // Fill the membership array by current members
            /*
            for (int j = 0; j < num_groups; j++) {
              printf("Member: %s\n", target_groups[j]);
              char member_name[20];
              char *token;
              int existing_server_index;

              memcpy(member_name, &target_groups[j][1],
                     strlen(target_groups[j]) - 1);
              token = strtok(member_name, "#");
              existing_server_index = token[strlen(token) - 1] - '0';
              group_members[existing_server_index] = 1;
            }
            */

            /**
               printf("Current membership: ");
               for (int i = 0; i < 5; i++) {
               printf("%d ", group_members[i]);
               }
               printf("\n");
            */

            struct EXCHANGE_INDEX_MATRIX_MSG exchange_msg;
            exchange_msg.update_msg.source.type = SERVER;
            exchange_msg.update_msg.server_index = atoi(server_index);
            exchange_msg.update_msg.type = EXCHANGE_INDEX_MATRIX;

            // exchange_msg.update_msg.update_index = -1;
            // exchange_msg.update_msg.time_stamp = -1;
            memcpy(exchange_msg.index_matrix, index_matrix,
                   sizeof(index_matrix));

            ret = SP_multicast(Mbox, AGREED_MESS, sender, 0,
                               sizeof(exchange_msg), (char *)&exchange_msg);
            if (ret < 0) {
              SP_error(ret);
              printf("\nBye.\n");
              exit(0);
            }
          }
        } else {
          printf("Received REGULAR membership for group %s with %d members, "
                 "where I am member %d:\n",
                 sender, num_groups, mess_type);

          for (int i = 0; i < num_groups; i++)
            printf("\t%s\n", &target_groups[i][0]);
        }
      }
    }
  }
}

// Tester methods
void print_index_matrix(int matrix[5][5]) {
  for (int i = 0; i < 5; i++) {
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
    printf("Mail (read:%d) with subject: %s, content %s, email_index %d, from "
           "server index: %d. \n",
           cur->email_msg.email.read, cur->email_msg.email.subject,
           cur->email_msg.email.content, cur->email_msg.email_index,
           cur->email_msg.server_index);
    cur = cur->next;
  }
}

void print_update_msg_list() {

  for (int i = 0; i < 5; i++) {
    printf("list no: %d\n", i);
    struct UPDATE_MSG_NODE *temp_msg = update_msg_head[i];
    while (temp_msg) {
      if (temp_msg->update_msg.update_index > 0) {
        printf("user name: %s ", temp_msg->update_msg.user_name);
        printf("type: %c ", temp_msg->update_msg.type);
        printf("time stamp: %d ", temp_msg->update_msg.time_stamp);
        printf("update index: %d ", temp_msg->update_msg.update_index);
        printf("server index: %d ", temp_msg->update_msg.server_index);
        printf("email index: %d ", temp_msg->update_msg.email_index);
        printf("email server index: %d ",
               temp_msg->update_msg.email_server_index);
        printf("\n");
      }
      temp_msg = temp_msg->next;
    }
  }
}

static void Bye() {
  printf("\nBye.\n");
  SP_disconnect(Mbox);
  exit(0);
}

void increment_index_matrix(int index_matrix[5][5], int group_members[5],
                            int server_index) {
  for (int i = 0; i < 5; i++) {
    if (group_members[i]) {
      index_matrix[i][server_index] += 1;
    }
  }
}

void update_index_matrix(int index_matrix[5][5], int group_members[5],
                         int server_index, int update_index) {
  for (int i = 0; i < 5; i++) {
    if (group_members[i]) {
      index_matrix[i][server_index] =
          MAX(index_matrix[i][server_index], update_index);
    }
  }
}

int check_time_index(int server_index1, int server_index2, int update_index_new,
                     int index_matrix[][5]) {

  if (server_index1 == server_index2)
    return 0;

  if (update_index_new <= index_matrix[server_index1][server_index2])
    return 0;

  return 1;
}

struct EMAIL_MSG_NODE *find_user_email_head(char *user_name,
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

struct EMAIL_MSG_NODE *
find_user_email_node(struct EMAIL_MSG_NODE *user_email_head, int email_index,
                     int email_server_index) {

  struct EMAIL_MSG_NODE *user_email_node = user_email_head->next;
  while (user_email_node) {
    if (user_email_node->email_msg.email_index == email_index &&
        user_email_node->email_msg.server_index == email_server_index) {
      return user_email_node;
    }
    user_email_node = user_email_node->next;
  }
  return NULL;
}

struct EMAIL_MSG_NODE *find_email(struct USER_NODE *user_list_head,
                                  char *user_name, int email_index,
                                  int email_server_index) {
  struct EMAIL_MSG_NODE *user_email_head =
      find_user_email_head(user_name, user_list_head);
  if (user_email_head) {

    return find_user_email_node(user_email_head, email_index,
                                email_server_index);
  }

  return NULL;
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
    struct EMAIL_MSG_NODE *current = user_email_head;
    int current_email_time;
    current_email_time = new_email_msg.time_stamp;

    // When the incoming email has the same timestamp with some of existing
    // email, put the one with larger server_index to the front

    while (current->next &&
           current->next->email_msg.time_stamp > current_email_time) {

      current = current->next;
    }

    while (current->next &&
           current->next->email_msg.time_stamp == current_email_time &&
           current->next->email_msg.server_index > new_email_msg.server_index) {
      current = current->next;
    }
    new_email_node->next = current->next;
    current->next = new_email_node;
    /**
        new_email_node->next = user_email_head->next;
        user_email_head->next = new_email_node;
        */
  }
}

struct SERVER_EMAIL_RES_MSG read_new_email(char *user_name, int email_index,
                                           int read_server_index,
                                           struct USER_NODE *user_list_head) {
  struct EMAIL_MSG_NODE *user_email_head = NULL;
  user_email_head = find_user_email_head(user_name, user_list_head);

  struct SERVER_EMAIL_RES_MSG read_response;
  read_response.msg.type = READ_EMAIL_RES;
  read_response.exist = 0;

  if (!user_email_head) {
    printf("User %s not found!\n", user_name);
  } else {
    struct EMAIL_MSG_NODE *user_email_node = user_email_head->next;
    while (user_email_node) {
      // printf("server_index: %d\n",
      // user_email_node->email_msg.server_index);
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
  user_email_head = find_user_email_head(user_name, user_list_head);

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

void add_update_msg(struct UPDATE_MSG update_msg) {

  int server_index;
  server_index = update_msg.server_index;

  struct UPDATE_MSG_NODE *update_msg_node =
      (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
  update_msg_node->update_msg = update_msg;
  update_msg_node->pre = update_msg_head[server_index];
  update_msg_node->next = update_msg_head[server_index]->next;
  update_msg_head[server_index]->next = update_msg_node;
  update_msg_node->next->pre = update_msg_node;
}

void delete_update_msg(int server_index, int update_index) {

  struct UPDATE_MSG_NODE *temp_msg = update_msg_tail[server_index]->pre;

  while (temp_msg->update_msg.update_index != -1 &&
         temp_msg->update_msg.update_index <= update_index) {

    temp_msg->pre->next = temp_msg->next;
    temp_msg->next->pre = temp_msg->pre;
    struct UPDATE_MSG_NODE *pre_msg = temp_msg->pre;
    free(temp_msg);
    temp_msg = pre_msg;
  }
}

void add_update_msg_lst(struct UPDATE_MSG_NODE *head, int server_index,
                        int update_index) {

  struct UPDATE_MSG_NODE *node = update_msg_head[server_index]->next;
  while (node) {
    if (node->update_msg.update_index == update_index) {
      struct UPDATE_MSG_NODE *pre = head;
      while (pre->next &&
             pre->next->update_msg.time_stamp < node->update_msg.time_stamp) {
        pre = pre->next;
      }

      // deep copy
      struct UPDATE_MSG_NODE *new_node =
          (struct UPDATE_MSG_NODE *)malloc(sizeof(struct UPDATE_MSG_NODE));
      memcpy(&new_node->update_msg, &node->update_msg,
             sizeof(node->update_msg));
      // printf("Update msg type %c, timestamp %d\n",
      // new_node->update_msg.type,
      //      new_node->update_msg.time_stamp);

      new_node->next = pre->next;
      new_node->pre = pre;
      pre->next = new_node;
    }
    node = node->next;
  }
}

struct UPDATE_MSG create_update_msg(char type, int time_stamp,
                                    int lamport_index, int server_index,
                                    int email_index, int email_server_index,
                                    char *user_name) {
  struct UPDATE_MSG update_msg;
  update_msg.source.type = SERVER;
  update_msg.type = type;
  update_msg.time_stamp = time_stamp;
  update_msg.update_index = lamport_index;
  update_msg.server_index = server_index;
  update_msg.email_index = email_index;
  update_msg.email_server_index = email_server_index;
  memcpy(update_msg.user_name, user_name, sizeof(update_msg.user_name));
  return update_msg;
}

void write_index_matrix(int server_index, int mat[5][5], int time_stamp) {

  return;

  char file_name[FILENAME_LEN];
  sprintf(file_name, "./data/s%d/mat", server_index);

  FILE *fw;
  if ((fw = fopen(file_name, "w")) == NULL) {
    perror("fopen error");
    // exit(0);
  }

  fprintf(fw, "%d\n", time_stamp);

  // printf("write index matrix\n");
  for (int i = 0; i < 5; i++)
    fprintf(fw, "%d %d %d %d %d\n", mat[i][0], mat[i][1], mat[i][2], mat[i][3],
            mat[i][4]);

  fclose(fw);
}

void read_index_matrix(int server_index, int mat[5][5], int *time_stamp) {

  char file_name[FILENAME_LEN];
  sprintf(file_name, "./data/s%d/mat", server_index);

  if (access(file_name, F_OK) == -1)
    return;

  FILE *fr;
  if ((fr = fopen(file_name, "r")) == NULL) {
    perror("fopen error");
    // exit(0);
  }

  int num;
  int i = 0;
  int *pointer = &mat[0][0];

  fscanf(fr, "%d", time_stamp);

  while (fscanf(fr, "%d", &num) == 1 && i < 25) {
    //    printf("%d ", num);
    pointer[i++] = num;
  }

  /*
    if (feof(fr)) {
    // hit end of file
    }
    else {
    // some other error interrupted the read
    }
  */

  fclose(fr);
}

void write_email(char *file_name, struct EMAIL_MSG email_msg) {

  return;

  FILE *fw;
  if ((fw = fopen(file_name, "w")) == NULL) {
    perror("fopen error");
    // exit(0);
  }

  printf("write email\n");
  fwrite(&email_msg, sizeof(email_msg), 1, fw);

  fclose(fw);
}

void read_all_email(char *dir, struct USER_NODE *user_list_head) {

  DIR *dp;
  struct dirent *ep;

  dp = opendir(dir);
  if (dp != NULL) {

    while ((ep = readdir(dp))) {
      // puts (ep->d_name);

      char *file_name = ep->d_name;

      if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
        continue;

      char file_dir[FILENAME_LEN];
      sprintf(file_dir, "%s/%s", dir, file_name);
      printf("%s\n", file_dir);
      struct EMAIL_MSG email_msg = read_email(file_dir);

      add_new_email(email_msg, user_list_head);
    }
    (void)closedir(dp);
  }
}

struct EMAIL_MSG read_email(char *file_name) {

  /*
    if(access(file_name, F_OK ) == -1)
    return;*/

  struct EMAIL_MSG email_msg;
  FILE *fr;
  if ((fr = fopen(file_name, "r")) == NULL) {
    perror("fopen error");
    // exit(0);
  }
  fread(&email_msg, sizeof(email_msg), 1, fr);

  /*
    printf("read email\n");
    printf("read: %d\n", email.read);
    printf("to: %s\n", email.to);
    printf("from: %s\n", email.from);
    printf("subject: %s\n", email.subject);
    printf("content: %s\n", email.content);
  */

  fclose(fr);

  return email_msg;
}

void write_update_msg(int server_index) {

  return;
  for (int i = 0; i < 5; i++) {
    // printf("list no: %d\n", i);
    FILE *fw;
    char file_name[FILENAME_LEN];
    sprintf(file_name, "./data/s%d/msg_s%d", server_index, i);

    if ((fw = fopen(file_name, "w")) == NULL) {
      perror("fopen error");
      // exit(0);
    }
    struct UPDATE_MSG_NODE *temp_msg = update_msg_tail[i];
    while (temp_msg) {
      if (temp_msg->update_msg.update_index != -1)
        fwrite(&temp_msg->update_msg, sizeof(temp_msg->update_msg), 1, fw);

      temp_msg = temp_msg->pre;
    }
    fclose(fw);
  }
}

void read_update_msg(int server_index) {

  for (int i = 0; i < 5; i++) {
    FILE *fr;
    char file_name[FILENAME_LEN];
    sprintf(file_name, "./data/s%d/msg_s%d", server_index, i);

    if (access(file_name, F_OK) == -1)
      continue;

    if ((fr = fopen(file_name, "r")) == NULL) {
      perror("fopen error");
      // exit(0);
    }

    struct UPDATE_MSG update_msg;
    while (fread(&update_msg, sizeof(update_msg), 1, fr) == 1) {
      add_update_msg(update_msg);
    }

    fclose(fr);
  }
}

void delete_email_on_disk(int server_index, char *file_name) {

  return;
  int ret;
  ret = remove(file_name);

  if (ret == 0) {
    printf("File deleted successfully");
  } else {
    printf("Error: unable to delete the file");
  }
}

void testWrite() {
  int num;
  FILE *fptr;
  fptr = fopen("test", "w");

  if (fptr == NULL) {
    printf("Error!");
    exit(1);
  }

  printf("Enter num: ");
  scanf("%d", &num);

  fprintf(fptr, "%d", num);
  fclose(fptr);
}
