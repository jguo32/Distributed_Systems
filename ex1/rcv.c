#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "net_include.h"
#include "sendto_dbg.h"

#define MIN(x, y) (x > y ? y : x)

int main(int argc, char **argv) {
  /* Variable for writing files */
  char *file_name; // not used
  FILE *fw;
  int nwritten;
  int bytes;
  int num;
  int status; //(-1:free; 0: connection,send ack to sender; 1:file transfer,
  // send packet ack; 2: )
  char mess_buf[MAX_MESS_LEN];

  /* Variables for UDP file transfer */
  struct sockaddr_in name;
  struct sockaddr_in send_addr;
  struct sockaddr_in from_addr;
  socklen_t from_len;

  struct hostent h_ent;
  struct hostent *p_h_ent;
  char my_name[NAME_LENGTH] = {'\0'};
  char host_name[NAME_LENGTH] = {'\0'}; // Host to send to
  int host_num;
  int from_ip;
  int sr;
  struct SENDER_NODE *head = malloc(sizeof(
      struct SENDER_NODE)); // Use a list of addr to maintain the sender list
  struct SENDER_NODE *curr = head;

  fd_set mask;
  fd_set dummy_mask, temp_mask;

  struct timeval timeout;

  /* Other parameters */
  int loss_rate;

  if (argc != 2) {
    printf("Usage: rcv <loss_rate_percent>\n");
    exit(0);
  }

  loss_rate = atoi(argv[1]);

  gethostname(my_name, NAME_LENGTH);
  printf("My host name is %s, ready to receive files...\n", my_name);

  /* Initialize the socket for receiving files */
  sr = socket(AF_INET, SOCK_DGRAM, 0);
  if (sr < 0) {
    perror("ncp: socket");
    exit(1);
  }

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = htons(PORT);

  if (bind(sr, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("ncp: bind");
    exit(1);
  }

  FD_ZERO(&mask);
  FD_ZERO(&dummy_mask);
  FD_SET(sr, &mask);
  status = RECEIVER_FREE;

LOOP:
  while (1) {
    printf("~~~~~~~~~~ recv ~~~~~~~~~~~\n");
    printf("status: %d\n", status);

    temp_mask = mask;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    num = select(FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);

    if (num > 0) {
      if (FD_ISSET(sr, &temp_mask)) {
        from_len = sizeof(from_addr);
        bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
                         (struct sockaddr *)&from_addr, &from_len);
        from_ip = from_addr.sin_addr.s_addr;

        /* check msg type */
        struct MSG recv_msg;
        memcpy(&recv_msg, mess_buf, sizeof(recv_msg));
        printf("message type: %c\n", recv_msg.type);

        if (recv_msg.type == STOR_START_CONN) {
          if (status == RECEIVER_FREE) {
            status = RECEIVER_START_CONN; // change status, start connection
            /* Open or create the destination file for writing */
	    struct OPEN_CONN_MSG open_conn_msg;
	    memcpy(&open_conn_msg, mess_buf, sizeof(open_conn_msg));
	    printf("File name is %s\n", open_conn_msg.filename);
            if ((fw = fopen(open_conn_msg.filename, "w")) == NULL) {
              perror("fopen");
              exit(0);
            }
          } else {
            unsigned long current_ip = head->next->from_addr.sin_addr.s_addr;
            if (from_ip != current_ip) {
              struct WAIT_MSG wait_msg;
              wait_msg.msg.type = RTOS_WAIT_CONN;
              char wait_buf[sizeof(wait_msg)];
              memcpy(wait_buf, &wait_msg, sizeof(wait_msg));
              sendto(sr, wait_buf, sizeof(wait_buf), 0,
                     (struct sockaddr *)&from_addr, sizeof(from_addr));
            }
          }

          // Check if the incoming sender is already in the list
          struct SENDER_NODE *temp = head->next;
          while (temp != NULL) {
            if (temp->from_addr.sin_addr.s_addr == from_ip) {
              goto LOOP;
            }
            temp = temp->next;
          }

          // Append the incoming address to the end of list
          struct SENDER_NODE *ptr =
              (struct SENDER_NODE *)malloc(sizeof(struct SENDER_NODE));
          ptr->from_addr = from_addr;
          curr->next = ptr;
          curr = ptr;
          // Print out new sender's information
          printf("Received transfer request from (%d.%d.%d.%d), "
                 "destination "
                 "file name: %s\n",
                 (htonl(from_ip) & 0xff000000) >> 24,
                 (htonl(from_ip) & 0x00ff0000) >> 16,
                 (htonl(from_ip) & 0x0000ff00) >> 8,
                 (htonl(from_ip) & 0x000000ff), mess_buf + sizeof(char));

        } else if (recv_msg.type == STOR_CLOSE_CONN) {
          // TODO: handle the case of closing message
          if (status == RECEIVER_DATA_TRANSFER) {
            printf("Receive file completely transfered msg, prepare to close "
                   "file writter. \n");
            fclose(fw);

            // TODO: Remove current sender (head) from the list
            // and send ack to the next sender (if there is one)
            struct SENDER_NODE *next_sender = head->next;
            head->next = next_sender->next;
            free(next_sender);
            if (head->next == NULL) {
              status = RECEIVER_FREE;
              curr = head;
            } else {
              from_addr = head->from_addr;
              status = RECEIVER_WAIT_NEXT;
            }
          }

          /*
          while (head != NULL) {
            from_ip = head->from_addr.sin_addr.s_addr;
            printf("current node: %d.%d.%d.%d \n",
                 (htonl(from_ip) & 0xff000000) >> 24,
                 (htonl(from_ip) & 0x00ff0000) >> 16,
                 (htonl(from_ip) & 0x0000ff00) >> 8,
                 (htonl(from_ip) & 0x000000ff));
            head = head->next;
          }
          */
          struct CLOSE_CONN_MSG close_msg;
          close_msg.msg.type = RTOS_CLOSE_CONN;
          char close_buf[sizeof(close_msg)];
          memcpy(close_buf, &close_msg, sizeof(close_msg));
          sendto(sr, close_buf, sizeof(close_buf), 0,
                 (struct sockaddr *)&from_addr,
                 sizeof(from_addr));

        } else if (recv_msg.type == STOR_PACKET_COMES) {
          if (status == RECEIVER_DATA_TRANSFER) {

            struct STOR_MSG recv_pack;
            memcpy(&recv_pack, mess_buf, sizeof(recv_pack));

            printf("Receive package (no: %d)\n", recv_pack.packageNo);
            printf("size %d\n", sizeof(recv_pack.data));
            // printf("len %d\n", strlen(recv_pack.data));
            // printf("data: %s\n", recv_pack.data);

            nwritten =
                fwrite(recv_pack.data, 1,
                       MIN(sizeof(recv_pack.data), strlen(recv_pack.data)), fw);

            struct RTOS_MSG send_pack;
            send_pack.msg.type = RTOS_ACK_COMES;
            send_pack.ackNo = recv_pack.packageNo;

            char send_buf[sizeof(send_pack)];
            memcpy(send_buf, &send_pack, sizeof(send_pack));

            /*TO-DO, pack all the ack together and send out */
            sendto(sr, send_buf, sizeof(send_buf), 0,
                   (struct sockaddr *)&(head->next->from_addr),
                   sizeof(head->next->from_addr));

            // break; // for test
            /*
            if (bytes > 0) {
              nwritten = fwrite(mess_buf + sizeof(char), 1, bytes -
            sizeof(char), fw);
              if (nwritten != (bytes - sizeof(char))) {
                perror("fwrite");
                exit(0);
              }
            }

            // Why this line??
            mess_buf[bytes] = 0;
            if (bytes < BUF_SIZE) {
              break;
            }*/
          }
        } else if (recv_msg.type == STOR_COMFIRM_CONN) {
          // receive connection ack from sender, start file transfer
          printf(
              "Received connection ack from sender, start file transfer...\n");
          status = RECEIVER_DATA_TRANSFER;
        } else {
          perror("Packet error.\n");
          exit(0);
        }
      }
    }

    printf("~~~~~~~~~~ send ~~~~~~~~~~~\n");
    printf("status: %d\n", status);
    // send
    if (status == RECEIVER_START_CONN) { // send connection ack
      printf("Send out connection ack.\n");
      struct START_CONN_MSG conn_msg;
      conn_msg.msg.type = RTOS_START_CONN;

      char conn_buf[sizeof(conn_msg)];
      memcpy(conn_buf, &conn_msg, sizeof(conn_msg));
      sendto(sr, conn_buf, strlen(conn_buf), 0,
             (struct sockaddr *)&(head->next->from_addr),
             sizeof(head->next->from_addr));
    } else if (status == RECEIVER_WAIT_NEXT) {
      struct AWAKE_MSG awake_msg;
      awake_msg.msg.type = RTOS_AWAKE;
      char awake_buf[sizeof(awake_msg)];
      memcpy(awake_buf, &awake_msg, sizeof(awake_msg));
      sendto(sr, awake_buf, strlen(awake_buf), 0,
             (struct sockaddr *)&(head->next->from_addr),
             sizeof(head->next->from_addr));
    } else {
    }
  }
  //  fclose(fw);

  return 0;
}
