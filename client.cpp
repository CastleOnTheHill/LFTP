#include "mytcp.h"


int main(int argc, char const *argv[])
{
	char model;
	if(argc != 4) {
		printf("USAGE : client send serverAddr filename OR client get serverAddr filename \n");
		exit(-1);
	}
	if(strcmp(argv[1], "send") == 0) {
		model = 'U';
	}
	else if(strcmp(argv[1], "get") == 0) {
		model = 'D';
	}
	else {
		printf("first parameter must be send OR get\n");
		exit(-1);
	}
	mytcp tcp;

	tcp.establish_socket_client();
	printf("%s, %s\n", argv[2], argv[3]);



	tcp.establish_connection(argv[2], argv[3], model); 

	if(model == 'D') {
		tcp.fileRecvProgram(argv[3]);
	}
	else {
		tcp.fileSendProgram(argv[3]);
	}

	tcp.close_connection();

	return 0;
}

