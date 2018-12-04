
#ifndef MYTCP_H
#define MYTCP_H

#include <sys/socket.h>
// #include <sys/types.h>
#include <sys/time.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <cmath>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define SENDINGWINDOW_SIZE 60
#define RECV_BUFFER_SIZE 61440
// pthread_mutex_t mutex;

enum Event
{
  timeOut,
  duplicateACK, //不需要变量记录，存在三个重复Ack
  newAck
};

enum State
{
  slowStart,
  congestion_avoidance,
  fast_recovery
};

class congestion_control
{
private:
  State nowState;
public:
  int ssthresh; //对应的是当前轮发送包的所有字节数
  int cwnd;
  int dupACKcount;
  congestion_control();
  void reactToEvent(Event event, pthread_mutex_t *mutex);
  const char* getCurrentStateInStr();
  State getCurrentState();
};


struct tcpSeg
{
  int seq;
  int ack;
  int dataSize;
  int recvWindow;
  char sign;
  char buffer[BUFFER_SIZE]; //后期可改进的地方->非固定大小

  tcpSeg() {
    seq = 0;
    ack = 0;
    dataSize = 0;
    sign = 0;
    recvWindow = 0;
  }
};

struct sendingWindow
{
  int head;
  int tail;
  int sendBase;
  int nextseqnum;
  struct tcpSeg window[SENDINGWINDOW_SIZE];
  int timestamp[SENDINGWINDOW_SIZE];
  bool isRent[SENDINGWINDOW_SIZE];
  int ackNum[SENDINGWINDOW_SIZE];
  sendingWindow()
  {
    head = 0;
    tail = 0;
    sendBase = 0;
    nextseqnum = 0;
    for (int i = 0; i < SENDINGWINDOW_SIZE; i++)
    {
      isRent[i] = false;
    }
  }
};

struct gap
{
  int head;
  int tail;
  struct gap *nextGap;
  
  gap() {
    head = 0;
    tail = 0;
    nextGap = NULL;
  }

  gap(int h, int t, gap* next) {
    head = h;
    tail = t;
    nextGap = next;
  }
};

struct recvingWindow
{
  int recvBase;
  int emptyPos; //未被使用的区域
  struct gap *gapHead;
  char recvBuffer[RECV_BUFFER_SIZE];
  recvingWindow() {
    recvBase = 0;
    emptyPos = 0;
    gapHead = NULL;
  }
};

class mytcp {
  private:
    static bool recvFlag;
    static int idleCounter;
    static int TimeoutInterval;
    static congestion_control CC;
    double EstimatedRTT;
    double DevRTT;
    bool sendFinishFlag, recvFinishFlag;
    socklen_t len;
    void gapHandle();
    void eraseGap(gap *index);
    void insertGap(gap *newGap);
    void pushBackGap(gap *newGap);
    gap* getContainGap(int fseq, int eseq, int *flag);
  public:
    static int fd;
    static FILE *sFile, *rFile;
    static sockaddr_in send_addr, recv_addr;
    static sendingWindow swindow;
    static recvingWindow rwindow;
    static tcpSeg sendSeg, recvSeg;
    static pthread_mutex_t mutex;
    static void startTimer(int t);
    static void sendPkg(tcpSeg *seg);
    static void timeoutHandle(int signum);
    mytcp();
    ~mytcp();
    void accept(char &model, char *filename);
    void establish_socket_client();
    void establish_socket_server(int port);
    void establish_connection(char const *address, char const *filename, char const model);
    void sendFile();
    void socketFileSend();
    void socketFileAckRecv();
    void socketFileAckSend();
    void socketFileRecv(); 
    void getFile();
    void close_connection();
    void recvData();
    void writeData();
    void makeNextPkt(tcpSeg *seg);
    void messagePrint();
    void fileSendProgram(char const* filename);
    void fileRecvProgram(char const* filename);
};


#endif