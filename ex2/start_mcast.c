#include "net_include.h"

int main() {
  struct sockaddr_in send_addr;
  int mcast_addr;
  unsigned char ttl_val;

  int ss;
  char send_buf[1];

  mcast_addr = 225 << 24 | 0 << 16 | 1 << 8 | 1;

  ss = socket(AF_INET, SOCK_DGRAM, 0);
  if (ss < 0) {
    perror("Mcast: socket");
    exit(1);
  }

  ttl_val = 1;
  if (setsockopt(ss, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl_val,
                 sizeof(ttl_val)) < 0) {
    printf("Mcast: problem in setsockopt of multicast ttl %d\n", ttl_val);
  }

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = htonl(mcast_addr);
  send_addr.sin_port = htons(PORT_MULTI_CAST);

  struct START_MSG start_msg;
  start_msg.msg.type = START_MCAST;
  memcpy(send_buf, &start_msg, sizeof(start_msg));
  sendto(ss, send_buf, strlen(send_buf), 0, (struct sockaddr *)&send_addr,
         sizeof(send_addr));
}
