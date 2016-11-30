#include "sp.h"

#define MAX_MEMBERS 100
#define MAX_MESSLEN 102400

#define USERNAME_LEN 80
#define SUBJECT_LEN 200
#define CONTENT_LEN 1000
#define GROUPNAME_LEN MAX_GROUP_NAME
#define EMAIL_LIST_MAX_LEN 50

#define MAX_VSSETS 10

// char user_name[USERNAME_LEN];

/* client status */
#define INIT 'i'
#define LOGIN 'g'
#define CONNECT 'c'

/* msg type between client with server */
#define CLIENT 'c'
#define SERVER 's'

#define PRIVATE_GROUP_REQ 'a'
#define PRIVATE_GROUP_RES 'b'
#define SEND_EMAIL 'c'
#define EMAIL_LIST_REQ 'd'
#define EMAIL_LIST_RES 'e'
#define READ_EMAIL_REQ 'f'
#define READ_EMAIL_RES 'g'
#define DELETE_EMAIL_REQ 'h'
#define DELETE_EMAIL_RES 'i'

#define EXCHANGE_INDEX_MATRIX 'j'

/* email struct */
struct EMAIL {
  int read;
  char to[USERNAME_LEN];
  char from[USERNAME_LEN];
  char subject[SUBJECT_LEN];
  char content[CONTENT_LEN];
};

/* structs for the server to maintain user/email list */
struct EMAIL_MSG {
  int server_index;
  int email_index;
  struct EMAIL email;
};

struct EMAIL_MSG_NODE {
  struct EMAIL_MSG email_msg;
  struct EMAIL_MSG_NODE *next;
};

struct USER_NODE {
  char user_name[80];
  struct EMAIL_MSG_NODE email_node;
  struct USER_NODE *next;
};

/* msg to server, flag msg check from server or client */
struct SOURCE {
  char type;
};

/* msg from client to server */
struct CLIENT_MSG {
  struct SOURCE source;
  char type;
};

struct CLIENT_PRIVATE_GROUP_REQ_MSG {
  struct CLIENT_MSG msg;
};

struct CLIENT_EMAIL_LIST_REQ_MSG {
  struct CLIENT_MSG msg;
  char receiver_name[USERNAME_LEN];
};

struct CLIENT_SEND_EMAIL_MSG {
  struct CLIENT_MSG msg;
  char receiver_name[USERNAME_LEN];
  struct EMAIL email;
};

struct CLIENT_READ_EMAIL_MSG {
  struct CLIENT_MSG msg;
  int server_index;
  int email_index;
  char user_name[USERNAME_LEN];
};

struct CLIENT_DELETE_EMAIL_MSG {
  struct CLIENT_MSG msg;
  int server_index;
  int email_index;
  char user_name[USERNAME_LEN];
};

/* msg from server to client */
struct SERVER_MSG {
  char type;
};

struct SERVER_PRIVATE_GROUP_RES_MSG {
  struct SERVER_MSG msg;
  char group_name[GROUPNAME_LEN];
};

struct SERVER_EMAIL_LIST_RES_MSG {
  struct SERVER_MSG msg;
  int email_num;
  struct EMAIL_MSG email_list[EMAIL_LIST_MAX_LEN];
};

struct SERVER_EMAIL_RES_MSG {
  struct SERVER_MSG msg;
  int exist;
  struct EMAIL email;
};

struct SERVER_DELETE_RES_MSG {
  struct SERVER_MSG msg;
  int success;
};

struct UPDATE_MSG {
  struct SOURCE source;
  char type;
  int update_index;
  int server_index;
  int email_index;
};

struct EXCHANGE_INDEX_MATRIX_MSG {
  struct UPDATE_MSG update_msg;
  int index_matrix[5][5];
};
