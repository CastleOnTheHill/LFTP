# LFTP

计算机网络课程个人项目，仅使用UDP完成TCP的特性并实现一个简易的FTP，支持多用户同时上传下载文件。

## 设计文档

### 1.   结构体/类

#### 1. `TCP`数据包结构体：`tcpSeg`

```c++
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
```

#### 2. 拥塞控制类`congestion_control`

```c++
enum Event
{
  timeOut,
  duplicateACK, 
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
```

枚举变量`State`分别对应慢启动，拥塞避免，和快速恢复。

枚举变量`Event`分别对应事件超时，收到冗余的数据包，接收到了新的ack。

函数`reactToEvent`的功能是对应发生的事件，将拥塞避免类中的变量和状态改变。

#### 3. 发送窗口结构体：`sendingWindow` 

```c++
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
```

记录发送窗口的`sendBase`，`nextseqnum`信息。

使用循环队列来存储已发送的数据包。当收到ACK的时候，将对应包和对应包之前的数据包全部移出队列。

#### 4. 接收窗口结构体：`recvingWindow `

```c++
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
```

记录下接收窗口的`recvBase`，可用缓存等信息。 

#### 5. 记录接收buffer中失序的包的结构体：`gap`

```c++
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
```

记录好未收到的包的开始序号，结束序号，并使用一个单向链表将它们串连起来。 

#### 6. TCP类：`mytcp` 

```c++
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
```

实现一对一TCP客户端和服务端的所有功能。

### 2. 实现思路

#### 1. 百分百可靠数据传输

- 发送方方面：发送方将已发送的数据包缓存在发送窗口中并开始计时，收到接收方的`ACK`就将对应的数据包移出发送窗口并重新开始计时。如果计时结束，则将发送窗口中的第一个数据包重传。 
- 接收方方面：将失序的包存储下来，并记录中间未到达的数据包的`seqNum`，每次都`ACK seqNum`，出发服务端的快速重传，直到有一大段收到的的`seqNum`，将其传递给应用层，并更新`recvBase`，`emptyPos`的数值。

#### 2. 流控制功能 

发送方将接收窗口的剩余缓存信息写入到数据包结构体的`tcpSeg recvWindow`字段，当数值为0时，发送方停止发送数据，直到收到数据包的`recvWindow`字段值大于0。

#### 3. 拥塞控制功能

每次检测到超时、接收重复数据包、接收新的数据包的事件时，将对应事件作为参数发送到拥塞控制类`congestion_control`的`reactToEvent`函数，函数通过当前的状态和事件，决定下一个状态和`ssthresh`，`cwnd`， `dupACKcount`的值。

发送时限制已发送并但没有收到ack的包的字节数总数不大于`cwnd`。

#### 4. 服务端同时服务多客户端

先构建好一对一的服务端`server`和客户端`client`功能，然后构建一个`mutiServer`。客户端先与`mutiServer`发送标记为`H`的数据包，`mutiServer`记录下客户端的IP和端口，如果客户端IP和端口的组合是之前没有出现过的，随机生成一个可用端口Port返回给客户端，并生成一个监听在port的server子进程；如果客户端IP和端口的组合是之前出现过的，就返回之前生成的端口Port。

当通信结束后，`server`会结束进程，`mutiServer`通过`waitpid`来处理结束了的子进程，防止其变成僵尸进程，也把之前记录的客户端的信息删除。

### 运行截图

#### 1. 握手阶段：确定客户端想要使用的功能 send/recv

客户端：

![shake1](images\client_handshake.png)

服务器端：

![shake1](images\server_handshake.png)

#### 2. 收发数据阶段

客户端：

![recv](G:\study\大三上\code\myTCP_cpp\images\client_recv.png)

服务器端：

![send](images\server_send.png)

#### 3. 结束阶段

客户端：

![close1](images\client_close.png)

服务器端：

![close2](images\server_close.png)