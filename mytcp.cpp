#include "mytcp.h"

int mytcp::fd = 0;
FILE* mytcp::sFile = NULL, * mytcp::rFile = NULL;
sockaddr_in mytcp::send_addr, mytcp::recv_addr;
sendingWindow mytcp::swindow;
recvingWindow mytcp::rwindow;
tcpSeg mytcp::sendSeg, mytcp::recvSeg;
bool mytcp::recvFlag = false;
int mytcp::idleCounter = 0;
int mytcp::TimeoutInterval = 500;
congestion_control mytcp::CC;
pthread_mutex_t mytcp::mutex;

const char *congestion_control::getCurrentStateInStr()
{
  if(nowState == slowStart) {
    return "slow start";
  }
  else if(nowState == congestion_avoidance) {
    return "congestion avoidance";
  }
  else {
    return "fast recovery";
  }
}

State congestion_control::getCurrentState() {
  return nowState;
}


void congestion_control::reactToEvent(Event event, pthread_mutex_t* mutex) {
  
  int i = pthread_mutex_trylock(mutex);
  if(i != 0) {
    printf("lock failed \n");
    return;
  }
  
  printf("nowState : %s event : %d\n", getCurrentStateInStr(), event);
  switch (nowState)
  {
    case slowStart:
      switch(event) 
      {
        case timeOut :
          ssthresh = cwnd / 2;
          cwnd = BUFFER_SIZE;
          break;
        case duplicateACK:
          dupACKcount += 1;
          if(dupACKcount == 3) {
            ssthresh = cwnd / 2;
            cwnd = ssthresh + 3 * BUFFER_SIZE;
            nowState = fast_recovery;
          }
          break;
        case newAck :
          cwnd += BUFFER_SIZE;
          dupACKcount = 0;
          if(cwnd >= ssthresh / 2) {
            nowState = congestion_avoidance;
          }
          break;
      }
      break;
    case congestion_avoidance :
      switch (event)
      {
        case timeOut:
          ssthresh = cwnd / 2;
          cwnd = BUFFER_SIZE;
          nowState = slowStart;
          break;
        case duplicateACK:
          dupACKcount += 1;
          if(dupACKcount == 3) {
            ssthresh = cwnd / 2;
            cwnd = ssthresh + 3 * BUFFER_SIZE;
            nowState = fast_recovery;
          }
          break;
        case newAck:
          dupACKcount = 0;
          cwnd += BUFFER_SIZE * BUFFER_SIZE / cwnd;
          break;
      }
      break;
    case fast_recovery :
      switch (event)
      {
        case timeOut:
          ssthresh = cwnd / 2;
          cwnd = BUFFER_SIZE;
          nowState = slowStart;
          break;
        case duplicateACK:
          cwnd += BUFFER_SIZE;
          break;
        case newAck:
          dupACKcount = 0;
          cwnd = ssthresh;
          nowState = congestion_avoidance;
          break;
      }      
      break;
  }
  pthread_mutex_unlock(mutex);
}

congestion_control::congestion_control()
{
  nowState = slowStart;
  ssthresh = 64 * BUFFER_SIZE;
  cwnd = 1 * BUFFER_SIZE;
  dupACKcount = 0;
}

// 辅助函数
int getTimeMS()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

mytcp::mytcp() {
  sendFinishFlag = false;
  recvFinishFlag = false;
  EstimatedRTT = 1;
  DevRTT = 0;
  srand((unsigned)time(NULL));
  signal(SIGALRM, mytcp::timeoutHandle);
}

mytcp::~mytcp() {
  // 存在中间中断情况，在析构函数中统一CLOSE
  if(rFile != NULL) {
    fclose(rFile);
  }
  if(sFile != NULL) {
    fclose(sFile);
  }
  if(rwindow.gapHead != NULL) {
    gap* temp;
    while(rwindow.gapHead != NULL) {
      temp = rwindow.gapHead->nextGap;
      free(rwindow.gapHead);
      rwindow.gapHead = temp;
    }
  }
  close(fd);
}

void mytcp::establish_socket_client()
{
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
  {
    printf("socket establish failed!\n");
    exit(-1);
  }
  int n = 1000 * sizeof(tcpSeg);
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

void mytcp::establish_socket_server(int port) {
  struct sockaddr_in server_addr, client_addr;
  socklen_t len = sizeof(client_addr);
  int count;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
  {
    printf("create server socket failed!\n");
    exit(-1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // server_addr.sin_addr.s_addr = inet_addr("172.18.34.139");
  server_addr.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    close(fd);
    printf("bind addr failed\n");
    exit(-1);
  }
  int n = 1000 * sizeof(tcpSeg);
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

void mytcp::establish_connection(char const *address, char const *filename, char const model)
{
  memset(&send_addr, 0, sizeof(struct sockaddr_in));
  send_addr.sin_family = AF_INET;
  // send_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  send_addr.sin_addr.s_addr = inet_addr(address);
  send_addr.sin_port = htons(8888);
  
  // 第一个H包的发送
  // swindow.window[swindow.tail].seq = 0;
  // swindow.window[swindow.tail].sign = 'H';
  // swindow.window[swindow.tail].dataSize = 0;
  // swindow.window[swindow.tail].recvWindow = RECV_BUFFER_SIZE;
  // printf("client send H pkg\n");
  // sendPkg(&(swindow.window[swindow.tail]));
  // startTimer(TimeoutInterval);
  // swindow.tail = (swindow.tail + 1) % SENDINGWINDOW_SIZE;

  // while (true)
  // {
  //   int count = recv(fd, &recvSeg, sizeof(tcpSeg), 0);
  //   recvFlag = true;
  //   if (count == -1)
  //   {
  //     printf("error : recv error\n");
  //     exit(-1);
  //   }
  //   if (recvSeg.sign == 'H')
  //   {
  //     send_addr.sin_port = htons(recvSeg.seq);
  //     printf("recv H new port %d", recvSeg.seq);
  //     swindow.head = swindow.tail;
  //     break;
  //   }
  // }

  // 第一个S包的发送
  swindow.window[swindow.tail].seq = 0;
  swindow.window[swindow.tail].sign = 'S';
  swindow.window[swindow.tail].dataSize = 0;
  swindow.window[swindow.tail].recvWindow = RECV_BUFFER_SIZE;
  printf("client send 1th S pkg\n");
  sendPkg(&(swindow.window[swindow.tail]));
  startTimer(TimeoutInterval);
  swindow.tail = (swindow.tail + 1) % SENDINGWINDOW_SIZE;   

  while(true) {
    int count = recv(fd, &recvSeg, sizeof(tcpSeg), 0);
    recvFlag = true;
    if(count == -1) {
      printf("error : recv error\n");
      exit(-1);
    }
    if(recvSeg.ack == swindow.window[swindow.tail - 1].seq + 1 && recvSeg.sign == 'S') {
       swindow.head = swindow.tail;
       break;
    }
  }

  // 模式包发送
  swindow.window[swindow.tail].seq = recvSeg.ack;
  swindow.window[swindow.tail].sign = model;
  swindow.window[swindow.tail].ack = recvSeg.seq + 1;  // 最后一次握手阶段还是 +1
  swindow.window[swindow.tail].dataSize = strlen(filename);
  swindow.window[swindow.tail].recvWindow = RECV_BUFFER_SIZE;
  strncpy(swindow.window[swindow.tail].buffer, filename, swindow.window[swindow.tail].dataSize);
  printf("client send %c pkg\n", model);
  sendPkg(&(swindow.window[swindow.tail]));
  startTimer(TimeoutInterval);
  swindow.tail = (swindow.tail + 1) % SENDINGWINDOW_SIZE;  
  // 设置好sendBase 和 recvBase
  if(model == 'D') {
    rwindow.recvBase = recvSeg.seq + 1;
  }
  else {
    swindow.sendBase = swindow.window[swindow.tail - 1].seq + swindow.window[swindow.tail - 1].dataSize;
    swindow.nextseqnum = swindow.sendBase;
    printf("here ! nextseqnum %d\n", swindow.nextseqnum);
  }

  while(true) {
    int count = recv(fd, &recvSeg, sizeof(tcpSeg), 0);
    recvFlag = true;
    if(count == -1) {
      printf("error : recv error\n");
      exit(-1);
    }
    if(recvSeg.ack == swindow.window[swindow.tail - 1].seq + swindow.window[swindow.tail - 1].dataSize && recvSeg.sign == 'O') {
       swindow.head = swindow.tail;
       startTimer(0);
       printf("last server info seg %d ack %d datasize %d\n", recvSeg.seq, recvSeg.ack, recvSeg.dataSize);
       printf("recv correct, handshake over\n");
       break;
    }
    else if(recvSeg.sign != 'S' && recvSeg.sign != 'O' && recvSeg.dataSize > 0 && rwindow.recvBase <= recvSeg.seq) {
      // O 包丢失处理
      printf("detect O pkg lost, recv seq %d akc %d\n", recvSeg.seq, recvSeg.ack);
      swindow.head = swindow.tail;
      startTimer(0);
      recvData();
      break;
    }
    // 其他情况不理
  }

}

void mytcp::accept(char &model, char *filename) {
  startTimer(1000 * 10);
  int round = 0;
  while(round < 2) {
    int count = recvfrom(fd, &recvSeg, sizeof(struct tcpSeg), 0, (struct sockaddr *)&send_addr, &len);
    recvFlag = true;
    // 不能有多个客户端同时请求连接
    if(count == -1) {
      printf("error : recv error\n");
      exit(-1);
    }
    switch(round) {
      case 0:
        if(recvSeg.sign == 'S') {
          sendSeg.seq = 0;
          sendSeg.ack = recvSeg.seq + 1;
          sendSeg.sign = 'S';
          sendSeg.recvWindow = RECV_BUFFER_SIZE;
          printf("server send 2th S pkg\n");
          sendPkg(&sendSeg);          
          round += 1;
        }
        break;
      case 1:
        if(recvSeg.sign == 'D' || recvSeg.sign == 'U') {
          strncpy(filename, recvSeg.buffer, recvSeg.dataSize);
          filename[recvSeg.dataSize] = '\0';
          /* 保持意义上的统一 */
          if (recvSeg.sign == 'D') {
            model = 'U';
          }
          else {
            model = 'D';
          }
          sendSeg.seq = recvSeg.ack;
          sendSeg.ack = recvSeg.seq + recvSeg.dataSize;
          sendSeg.sign = 'O';
          sendSeg.recvWindow = RECV_BUFFER_SIZE;
          printf("server send O pkg\n");
          sendPkg(&sendSeg);
          // 设置好sendBase 和 recvBase
          if(model == 'U') {
            swindow.sendBase = recvSeg.ack;
            swindow.nextseqnum = recvSeg.ack;
          }
          else {
            rwindow.recvBase = sendSeg.ack;
          }
          round += 1;
        }
        else if(recvSeg.sign == 'S') {
          // 向前兼容 应对之前的包丢失
          sendSeg.seq = 0;
          sendSeg.ack = recvSeg.seq + 1;
          sendSeg.sign = 'S';
          sendSeg.recvWindow = RECV_BUFFER_SIZE;
          printf("server send 2th S pkg again \n");
          sendPkg(&sendSeg);
        }
        break;
    }
  }

}


void mytcp::fileSendProgram(char const* filename) {
  if (pthread_mutex_init(&mutex, NULL) != 0){
    printf("锁初始化失败\n");
    exit(-1);
  }
  sFile = fopen(filename, "rb");
  if(sFile == NULL) {
    printf("File open failed\n");
    exit(-1);
  }

  while (sendFinishFlag == false)
  {
    sendFile();
    socketFileAckRecv();
  }

  while (swindow.head != swindow.tail) {
    socketFileAckRecv();
  }
  printf("file send complete !\n");
  pthread_mutex_destroy(&mutex);

}
void mytcp::fileRecvProgram(char const* filename) {
  rFile = fopen("recvfile.pdf", "wb");
  // rFile = fopen(filename, "wb");
  if(rFile == NULL) {
    printf("File open failed\n");
    exit(-1);
  }
  startTimer(TimeoutInterval);
  getFile();
  startTimer(0);
}
void mytcp::sendFile()
{ 
  printf("unacked Pkg : %d\n", swindow.nextseqnum - swindow.sendBase);
  printf("send round start : state %s cwnd %d ssthresh %d dupACK %d\n", CC.getCurrentStateInStr(), CC.cwnd, CC.ssthresh, CC.dupACKcount);
  while ((swindow.tail + 1) % SENDINGWINDOW_SIZE != swindow.head && sendFinishFlag == false && recvSeg.recvWindow > BUFFER_SIZE && swindow.nextseqnum - swindow.sendBase < CC.cwnd) {
    socketFileSend();
  }
}
void mytcp::getFile()
{
  rwindow.emptyPos = 0;
  rwindow.gapHead = NULL;
  while (recvFinishFlag != true) 
  {
    socketFileRecv();
  }

  while (rwindow.gapHead != NULL) // 还要没有gap
  {
    socketFileRecv();
  }
  printf("file recv complete!\n");
  writeData();
}

void mytcp::close_connection()
{

}

void mytcp::startTimer(int t) //毫秒
{
  struct itimerval val;
  val.it_interval.tv_sec = 0;
  val.it_interval.tv_usec = 0;
  val.it_value.tv_sec = t / 1000;
  val.it_value.tv_usec = (t % 1000) * 1000;
  setitimer(ITIMER_REAL, &val, NULL);
  printf("Start timer : %d\n", t);
}

void mytcp::socketFileSend()
{
  if ((swindow.tail + 1) % SENDINGWINDOW_SIZE == swindow.head)
  {
    printf("sending window is full\n");
    return;
  }
  makeNextPkt(&(swindow.window[swindow.tail]));
  if (swindow.tail == swindow.head)
  {
    swindow.sendBase = swindow.window[swindow.tail].seq;
    startTimer(TimeoutInterval);
  }
  swindow.nextseqnum = swindow.window[swindow.tail].seq + swindow.window[swindow.tail].dataSize;
  swindow.isRent[swindow.tail] = false;
  swindow.timestamp[swindow.tail] = getTimeMS();
  swindow.ackNum[swindow.tail] = 0;
  printf("send pkg : seq %d ack %d dataSize %d sign %c\n", swindow.window[swindow.tail].seq, swindow.window[swindow.tail].ack, swindow.window[swindow.tail].dataSize, swindow.window[swindow.tail].sign);
  sendPkg(&(swindow.window[swindow.tail]));
  swindow.tail = (swindow.tail + 1) % SENDINGWINDOW_SIZE;
}

void mytcp::socketFileAckRecv() {
  int count = recv(fd, &recvSeg, sizeof(struct tcpSeg), 0);
  recvFlag = true;
  printf("recv ack : ack %d recvWindow %d\n", recvSeg.ack, recvSeg.recvWindow);
  int h = swindow.head;

  if(swindow.window[h].seq == recvSeg.ack) {
    CC.reactToEvent(duplicateACK, &mutex);
    swindow.ackNum[h] += 1;
    // if(CC.dupACKcount == 3 && CC.getCurrentState() != fast_recovery) {
    if(swindow.ackNum[h] == 3) {
      printf("Fast Retransmit : resend seq %d", swindow.window[h].seq);
      sendPkg(&swindow.window[h]); // 快速重传
      swindow.isRent[h] = true;
    }
  }
  else {
    while (h % SENDINGWINDOW_SIZE != swindow.tail)
    {
      if (swindow.window[h].seq + swindow.window[h].dataSize == recvSeg.ack)
      {
        if(h == swindow.head && !swindow.isRent[h]) {
          int sampleRTT = getTimeMS() - swindow.timestamp[h];
          EstimatedRTT = 0.875 * EstimatedRTT + 0.125 * sampleRTT;
          DevRTT = 0.75 * DevRTT + 0.25 * fabs(EstimatedRTT - sampleRTT);
          TimeoutInterval = EstimatedRTT + 4 * DevRTT;
          printf("update timeout : timeout %d sampleRTT %d EstimatedRTT %3.1f DevRTT %3.1f", TimeoutInterval, sampleRTT, EstimatedRTT, DevRTT);
        }
        CC.reactToEvent(newAck, &mutex);
        CC.dupACKcount = 0;
        /* 估计RTT */
        swindow.sendBase = recvSeg.ack;
        swindow.head = (h + 1) % SENDINGWINDOW_SIZE; //累计ACK
        if (swindow.head == swindow.tail && sendFinishFlag == true) // 发完所有数据包 且 所有数据包以收到 才停止时钟
        {
          startTimer(0); //stop timer
        }
        else
        {
          startTimer(TimeoutInterval);
        }
        break;
      }
      h = (h + 1) % SENDINGWINDOW_SIZE;
    }
  }
}

void mytcp::socketFileAckSend() {
  sendSeg.seq = recvSeg.ack;
  if(rwindow.gapHead == NULL) {
    sendSeg.ack = rwindow.recvBase + rwindow.emptyPos;
  }
  else {
    sendSeg.ack = rwindow.gapHead->head;
  }
  sendSeg.recvWindow = RECV_BUFFER_SIZE - rwindow.emptyPos;
  printf("send ack : ack %d windowSize : %d\n", sendSeg.ack, sendSeg.recvWindow);
  sendPkg(&sendSeg);
}

void mytcp::socketFileRecv() {
  int count = recv(fd, &recvSeg, sizeof(struct tcpSeg), 0);
  recvFlag = true;
  if(count == -1) {
    printf("error : recv error\n");
    exit(-1);
  }
  // 接到一个包重启定时器
  startTimer(TimeoutInterval);
  printf("recv pkg : seq %d dataSize %d\n", recvSeg.seq, recvSeg.dataSize);

  // 对于'O'包丢失处理
  if (recvSeg.sign == 'U')
  {
    sendSeg.seq = recvSeg.ack;
    sendSeg.ack = recvSeg.seq + recvSeg.dataSize;
    sendSeg.sign = 'O';
    sendSeg.dataSize = 0;
    sendSeg.recvWindow = RECV_BUFFER_SIZE;
    printf("resend O pkg\n");
    sendPkg(&sendSeg);
    return;
  }

  if(recvSeg.sign == 'F') {
    recvFinishFlag = true;
    printf("recv last pkg\n");
  }
  assert(recvSeg.dataSize > 0);
  recvData();
}

void mytcp::gapHandle()
{
  if(rwindow.recvBase + RECV_BUFFER_SIZE < recvSeg.seq + recvSeg.dataSize) {
    if(recvSeg.sign == 'F') { 
      //如果收到了，但是drop掉，还是未完成
      recvFinishFlag = false;
    }
    printf("out of recv buffer, drop pkg seq %d\n", recvSeg.seq);
    return;
  }

  if (rwindow.recvBase + rwindow.emptyPos < recvSeg.seq)
  {
    struct gap *newGap = (struct gap *)malloc(sizeof(struct gap));
    newGap->head = rwindow.recvBase + rwindow.emptyPos;
    newGap->tail = recvSeg.seq - 1;
    pushBackGap(newGap);
    rwindow.emptyPos = recvSeg.seq - rwindow.recvBase;
    for (int i = 0; i < recvSeg.dataSize; ++i)
    {
      rwindow.recvBuffer[rwindow.emptyPos] = recvSeg.buffer[i];
      rwindow.emptyPos += 1;
    }
  }
  else
  {
    if(rwindow.gapHead == NULL) {
      printf("recv duplicated pkg seq %d\n", recvSeg.seq);
      return;
    }
    int flag; /*flag -1代表刚好包括 0代表部分内含  1代表完全内含 2代表已收到*/
    struct gap *p = getContainGap(recvSeg.seq, recvSeg.seq + recvSeg.dataSize - 1, &flag);
    if(flag != 2) {
      int base = recvSeg.seq - rwindow.recvBase;
      for (int i = 0; i < recvSeg.dataSize; ++i)
      {
        rwindow.recvBuffer[base + i] = recvSeg.buffer[i];
      }
    }

    if(flag == 2) {
      printf("recv duplicated pkg seq %d\n", recvSeg.seq);
    }
    else if (flag == -1)
    {
      // 刚好包括
      eraseGap(p);
      if(rwindow.gapHead == NULL) {
        printf("all gaps is erased write start!\n");
        writeData();
      }
    }
    else if (flag == 0)
    {
      // updataGap
      if (p->head == recvSeg.seq)
      {
        p->head = recvSeg.seq + recvSeg.dataSize;
      }
      else
      {
        p->tail = recvSeg.seq - 1;
      }
    }
    else
    {
      struct gap *newGap = (struct gap *)malloc(sizeof(struct gap));
      newGap->head = recvSeg.seq + recvSeg.dataSize;
      newGap->tail = p->tail;
      p->tail = recvSeg.seq - 1;
      insertGap(newGap);
    }
  }
}

void mytcp::writeData()
{
  assert(rwindow.gapHead == NULL);
  assert(rFile != NULL);
  if (fwrite(rwindow.recvBuffer, sizeof(char), rwindow.emptyPos, rFile) != rwindow.emptyPos)
  {
    printf("error : write file error\n");
    exit(-1);
  }
  rwindow.recvBase = rwindow.recvBase + rwindow.emptyPos;
  rwindow.emptyPos = 0;
}

void mytcp::makeNextPkt(struct tcpSeg *seg)
{
  assert(sFile != NULL);
  // 准备发送带数据的数据包而不是ack
  seg->seq = swindow.nextseqnum;
  seg->ack = recvSeg.seq + recvSeg.dataSize; // 过程中不变
  seg->sign = 0;
  seg->recvWindow = RECV_BUFFER_SIZE - rwindow.emptyPos;
  seg->dataSize = BUFFER_SIZE;
  int num = fread(seg->buffer, sizeof(char), seg->dataSize, sFile);
  if (num != seg->dataSize)
  {
    if (ferror(sFile))
    {
      printf("error : read file failed\n");
      exit(-1);
    }
    else if (feof(sFile))
    {
      printf("file transport complete !\n");
      sendFinishFlag = true;
      seg->dataSize = num;
      seg->sign = 'F';
    }
  }
}

void mytcp::eraseGap(struct gap *index)
{
  if (rwindow.gapHead == index)
  {
    rwindow.gapHead = index->nextGap;
    free(index);
    return;
  }
  struct gap *now = rwindow.gapHead, *before = rwindow.gapHead;
  while (now != index)
  {
    before = now;
    now = now->nextGap;
  }
  before->nextGap = now->nextGap;
  free(now);
}

void mytcp::insertGap(struct gap *newGap)
{
  if (rwindow.gapHead->head > newGap->tail)
  {
    newGap->nextGap = rwindow.gapHead;
    rwindow.gapHead = newGap;
    return;
  }
  struct gap *now = rwindow.gapHead->nextGap, *before = rwindow.gapHead->nextGap;

  while (now->head < newGap->tail)
  {
    before = now;
    now = now->nextGap;
  }
  before->nextGap = newGap;
  newGap->nextGap = now;
}

void mytcp::pushBackGap(struct gap *newGap)
{
  if (rwindow.gapHead == NULL)
  {
    rwindow.gapHead = newGap;
    newGap->nextGap = NULL;
  }
  else
  {
    struct gap *p = rwindow.gapHead;
    while (p->nextGap != NULL)
    {
      p = p->nextGap;
    }
    p->nextGap = newGap;
    newGap->nextGap = NULL;
  }
}

gap* mytcp::getContainGap(int fseq, int eseq, int *flag)
{
  assert(rwindow.gapHead != NULL);
  struct gap *p = rwindow.gapHead;
  while (p != NULL)
  {
    if (p->head == fseq && p->tail == eseq)
    {
      *flag = -1;
      return p;
    }
    else if (p->head < fseq && p->tail == eseq || p->head == fseq && p->tail > eseq)
    {
      *flag = 0;
      return p;
    }
    else if (p->head < fseq && p->tail > eseq)
    {
      *flag = 1;
      return p;
    }
    p = p->nextGap;
  }
  *flag = 2;
  return NULL;
}

void mytcp::recvData()
{
  if (rwindow.recvBase + rwindow.emptyPos == recvSeg.seq && rwindow.emptyPos < RECV_BUFFER_SIZE)
  {
    for (int i = 0; i < recvSeg.dataSize; ++i)
    {
      rwindow.recvBuffer[rwindow.emptyPos] = recvSeg.buffer[i];
      rwindow.emptyPos += 1;
    }
    assert(rwindow.emptyPos <= RECV_BUFFER_SIZE);
    // double contain = (double)rwindow.emptyPos / RECV_BUFFER_SIZE;
    if (rwindow.gapHead == NULL)
    {
      // printf("Recv buffer is almost full write start !\n");
      /* 一起写比一个一个写效率更高，但又不能因为后面过多的gap，导致没有位置，一直rwindow大小一直为0 */
      writeData();
    }
  }
  else
  {
    // assert(recvSeg.seq > rwindow.recvBase); //error : 收到序号小于recvBase的包！
    if (recvSeg.seq < rwindow.recvBase) {
      printf("recv duplicate package !\n");
    }
    else {
      gapHandle();
    }
  }
  socketFileAckSend();
}

void mytcp::messagePrint() {
  int tail = swindow.tail - 1;
  printf("---------------------------------\n");
  if(swindow.head == swindow.tail) {
    printf("Sending Window is Empty!\n");
  }
  else {
    printf("Sending Window : seq %d ack %d dataSize %d sign %c recvWindow %d\n", swindow.window[tail].seq, swindow.window[tail].ack, swindow.window[tail].dataSize, swindow.window[tail].sign, swindow.window[tail].recvWindow);
  }
  printf("send head : %d  send tail : %d\n", swindow.head, swindow.tail);
  printf("---------------------------------\n\n");
}

void mytcp::timeoutHandle(int signum)
{
  if(swindow.sendBase > BUFFER_SIZE) {
    CC.reactToEvent(timeOut, &mutex);
  }
  if(recvFlag == false) {
    idleCounter += 1;
  }
  else {
    recvFlag = false;
    idleCounter = 0;
  }
  if(idleCounter > 10) {
    printf("error : connection failed, please try later. \n");
    exit(0);
  }
  if(swindow.head != swindow.tail) {
    int head = swindow.head;
    sendto(fd, &(swindow.window[head]), sizeof(tcpSeg), 0, (sockaddr *)&send_addr, sizeof(*(sockaddr *)&send_addr));
    printf("timeout : sending seq %d again\n", swindow.window[head].seq);
    // while (head != swindow.tail)
    // {
    //   swindow.isRent[head] = true;
    //   head = (head + 1) % SENDINGWINDOW_SIZE;
    // }
    swindow.isRent[head] = true;
  }
  startTimer(TimeoutInterval);
};

void mytcp::sendPkg(tcpSeg *seg)
{
  // int r = rand() % 6;
  // if(r < 5) {
  //   // 1/2 的丢包率
  //   sendto(fd, seg, sizeof(tcpSeg), 0, (sockaddr *)&send_addr, sizeof(*(sockaddr *)&send_addr));
  // }
  // else {
  //   printf("TEST : actually not send pkg : seq %d ack %d\n", seg->seq, seg->ack);
  // }
  sendto(fd, seg, sizeof(tcpSeg), 0, (sockaddr *)&send_addr, sizeof(*(sockaddr *)&send_addr));
}

