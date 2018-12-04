#include "mytcp.h"
#include <iostream>
#include <vector>
#include <sys/wait.h>

struct connection_record
{
  struct sockaddr_in client_addr;
  int port;
  pid_t pid;
};

std::vector<connection_record> records;

void SIGCHLD_Handle(int signo)
{
  int status;
  int t;  
  while((t = waitpid(-1, &status, WNOHANG)) > 0) {
    for (int i = 0; i < records.size(); i++)
    {
      if (records[i].pid == t)
      {
        printf("exit process pid : %d port %d\n client %d\n", t, records[i].port, ntohl(records[i].client_addr.sin_addr.s_addr));
        records.erase(records.begin() + i);
        break;
      }
    }
  }
}

int getPortOfAddress(sockaddr_in& client_addr)
{
  
  for (int i = 0; i < records.size(); i++)
  {
    if (records[i].client_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr && records[i].client_addr.sin_family == client_addr.sin_family && records[i].client_addr.sin_port == client_addr.sin_port)
    {
      return records[i].port;
    }
  }
  return 0;
}

void getUseablePort(int &port) {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  int count;
  int sfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sfd < 0)
  {
    printf("create server socket failed!\n");
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // addr.sin_addr.s_addr = inet_addr("172.18.34.139");
  addr.sin_port = 0;
  if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    close(sfd);
    printf("bind addr failed\n");
    exit(-1);
  }
  
  if (getsockname(sfd, (struct sockaddr *)&addr, &len) == 0) {
    port = ntohs(addr.sin_port);
    printf("get port %d\n", port);
  }
  close(sfd);
}


int main(int argc, char const *argv[])
{
  int sfd, port;
  srand((unsigned)time(NULL));
  signal(SIGCHLD, SIGCHLD_Handle);

  struct sockaddr_in server_addr, client_addr;
  tcpSeg recvSeg, sendSeg;
  socklen_t len = sizeof(client_addr);
  int count;
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
  {
    printf("create server socket failed!\n");
    exit(-1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(8888);
  if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    close(fd);
    printf("bind addr failed\n");
    exit(-1);
  }

  while(true) {
    int count = recvfrom(fd, &recvSeg, sizeof(struct tcpSeg), 0, (struct sockaddr *)&client_addr, &len);
    if (count == -1)
    {
      printf("error : recv error\n");
      exit(-1);
    }
    printf("recv pkg\n");
    
    for(int i = 0; i < records.size(); i++)
    {
      printf("%d, %d, %d\n", ntohs(records[i].client_addr.sin_addr.s_addr), ntohs(records[i].client_addr.sin_port), records[i].client_addr.sin_family);
    }
    

    if(recvSeg.sign == 'H') {
    	printf("recv Hi pkg\n");
      port = getPortOfAddress(client_addr);
      if(port != 0) {
      	printf("Is acked client\n");
        sendSeg.seq = port;
        sendSeg.ack = port;
        sendSeg.dataSize = 0;
        sendSeg.sign = 'H';
        sendto(fd, &sendSeg, sizeof(tcpSeg), 0, (sockaddr *)&client_addr, sizeof(*(sockaddr *)&client_addr));
        printf("resend port %d, client %d", port, ntohl(client_addr.sin_addr.s_addr));
      }
      else {
        getUseablePort(port);
        sendSeg.seq = port;
        sendSeg.ack = port;
        sendSeg.dataSize = 0;
        sendSeg.sign = 'H';
        sendto(fd, &sendSeg, sizeof(tcpSeg), 0, (sockaddr *)&client_addr, sizeof(*(sockaddr *)&client_addr));
        pid_t fpid = fork();
        if (fpid < 0)
        {
          perror("fork error");
          exit(EXIT_FAILURE);
        }
        else if (fpid == 0)
        {
          printf("create process : pid %d port %d", fpid, port);
          char temp1[10], temp2[30];
          sprintf(temp1, "%d", port);
          sprintf(temp2, "%s", "./server");
          char *ARGV[3];
          char *ENVP[1];
          ARGV[0] = temp2;
          ARGV[1] = temp1;
          ARGV[2] = NULL;
          ENVP[0] = NULL; 
          execve("./server", ARGV, ENVP);
        }
        else {
	        connection_record temp;
	        temp.client_addr = client_addr;
	        temp.port = port;
	        temp.pid = fpid;
	        records.push_back(temp);
        }
      }
    }
  }

  close(fd);
  return 0;
}
