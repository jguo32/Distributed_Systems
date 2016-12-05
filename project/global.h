#include "sp.h"

#define MAX_MEMBERS 100
#define MAX_MESSLEN 102400

#define DIR_LEN      100
#define FILENAME_LEN 50
#define USERNAME_LEN 80
#define SUBJECT_LEN  200
#define CONTENT_LEN  1000
#define GROUPNAME_LEN MAX_GROUP_NAME
#define EMAIL_LIST_MAX_LEN 50

#define MAX_VSSETS 10

// char user_name[USERNAME_LEN];

/* client status */
#define INIT    'i'
#define LOGIN   'g'
#define CONNECT 'c'

/* msg type between client with server */
#define CLIENT  'c'
#define SERVER  's'

#define PRIVATE_GROUP_REQ     'a'
#define PRIVATE_GROUP_RES     'b'
#define SEND_EMAIL            'c'
#define EMAIL_LIST_REQ        'd'
#define EMAIL_LIST_RES        'e'
#define READ_EMAIL_REQ        'f'
#define READ_EMAIL_RES        'g'
#define DELETE_EMAIL_REQ      'h'
#define DELETE_EMAIL_RES      'i'
#define MEMBERSHIP_REQ        'j'
#define MEMBERSHIP_RES        'k'
#define INFO_CHANGE           'l'
#define MEMBER_CHECK_REQ      'm'
#define MEMBER_CHECK_RES      'n'

#define EXCHANGE_INDEX_MATRIX 'o'
#define NEW_EMAIL             'p'
#define READ_EMAIL            'q'
#define DELETE_EMAIL          'r'

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
  int time_stamp;
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

/* sturcts for the server to maintain updates messages */
struct UPDATE_MSG {
  struct SOURCE source;
  char type;
  int time_stamp;
  int update_index;
  int server_index;
  int email_index;              /* for create/read/delete email */
  int email_server_index;
  char user_name[USERNAME_LEN];
};

struct UPDATE_MSG_NODE {
  struct UPDATE_MSG_NODE *next;
  struct UPDATE_MSG_NODE *pre;
  struct UPDATE_MSG update_msg;
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
  char receiver_name[USERNAME_LEN]; /*deprecate*/
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

struct CLIENT_MEMBERSHIP_MSG {
  struct CLIENT_MSG msg;
};

struct CLIENT_CHECK_MEMBER_REQ_MSG {
  struct CLIENT_MSG msg;
};

/* msg from server to client */
struct SERVER_MSG {
  char type;
};

struct SERVER_INFO_CHANGE_MSG {
  struct SERVER_MSG msg;
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

struct SERVER_MEMBERSHIP_RES_MSG {
  struct SERVER_MSG msg;
  int group_members[5];
};

struct SERVER_CHECK_MEMBER_RES_MSG {
  struct SERVER_MSG msg;
  int group_members[5];
};

/* message bewteen servers */
struct EXCHANGE_INDEX_MATRIX_MSG {
  struct UPDATE_MSG update_msg;
  int index_matrix[5][5];
};

struct NEW_EMAIL_MSG {
  struct UPDATE_MSG update_msg;
  struct EMAIL email;
};
