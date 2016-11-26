#include "sp.h"

#define MAX_MEMBERS        100
#define MAX_MESSLEN        102400

#define CLIENT 'c'
#define SERVER 's'

#define USERNAME_LEN       80
#define GROUPNAME_LEN      MAX_GROUP_NAME
// char user_name[USERNAME_LEN];

/* client status */
#define INIT             'i'
#define LOGIN            'g'
#define CONNECT          'c'

/* msg type between client with server */
#define PRIVATE_GROUP_REQ       'p'
#define PRIVATE_GROUP_RES       'r'
#define SEND_EMAIL              'e'



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

/* msg from server to client */
struct SERVER_MSG {
  char type;
};

struct SERVER_PRIVATE_GROUP_RES_MSG {
  struct SERVER_MSG msg;
  char group_name[GROUPNAME_LEN];
};
  


