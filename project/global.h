#include "sp.h"

#define MAX_MEMBERS 100
#define MAX_MESSLEN 102400

#define CLIENT 'c'
#define SERVER 's'

#define USERNAME_LEN 80
#define SUBJECT_LEN 200
#define CONTENT_LEN 1000
#define GROUPNAME_LEN MAX_GROUP_NAME
// char user_name[USERNAME_LEN];

/* client status */
#define INIT 'i'
#define LOGIN 'g'
#define CONNECT 'c'

/* msg type between client with server */
#define PRIVATE_GROUP_REQ 'p'
#define PRIVATE_GROUP_RES 'r'
#define SEND_EMAIL 'e'

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
  int server_id;
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

/* check msg from server or client */
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

struct CLIENT_SEND_EMAIL_MSG {
  struct CLIENT_MSG msg;
  char receiver_name[USERNAME_LEN];
  struct EMAIL email;
};

/* msg from server to client */
struct SERVER_MSG {
  char type;
};

struct SERVER_PRIVATE_GROUP_RES_MSG {
  struct SERVER_MSG msg;
  char group_name[GROUPNAME_LEN];
};
