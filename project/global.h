#include "sp.h"


#define USERNAME_LEN 80
#define GROUPNAME_LEN MAX_GROUP_NAME
// char user_name[USERNAME_LEN];

/* msg from client to server */

struct CLIENT_MSG {
  char type;
};

struct CLIENT_SEND_EMAIL_MSG {
  CLIENT_MSG msg;
};

/* msg from server to client */

struct SERVER_MSG {
  char type;
};

struct SERVER_PRIVATE_GROUP_MSG {
  SERVER_MSG msg;
  char group_name[GROUPNAME_LEN];
};
  



  

