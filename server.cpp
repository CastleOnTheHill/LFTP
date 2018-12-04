#include "mytcp.h"

// struct tcpSeg handShakeSegSend, handShakeSegRecv;
int main(int argc, char const *argv[])
{
	mytcp tcp;
	char model;
	char filename[50];
	
	if(argc != 2) {
		printf("worng call\n");
		exit(-1);
	}

	tcp.establish_socket_server(atoi(argv[1]));

	tcp.accept(model, filename);
	/* 保持意义上的统一 */
	if(model == 'U') {
		tcp.fileSendProgram(filename);
	}
	else {
		tcp.fileRecvProgram(filename);
	}

	tcp.close_connection();
	
	return 0;
}
