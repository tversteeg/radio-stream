#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 100
#define CHUNK_SIZE 256
#define SEND_TIMEOUT 10
#define RECV_TIMEOUT 3

int parseInput(int fd)
{
	size_t bufsize = CHUNK_SIZE;
	char *buf = (char*)malloc(bufsize);

	struct timeval begin;
	gettimeofday(&begin, NULL);

	// Receive the send message and store it
	size_t size = 0;
	while(1){
		struct timeval now;
		gettimeofday(&now, NULL);
		double timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
	
		if((size > 0 && timediff > RECV_TIMEOUT) || timediff > RECV_TIMEOUT * 2){
			fprintf(stderr, "Time out on connection!\n");
			return -1;
		}

		char data[CHUNK_SIZE];
		int status = recv(fd, data, CHUNK_SIZE, 0);
		if(status <= 0){
			usleep(10000);
		}else{
			if(size + status > bufsize){
				bufsize <<= 1;
				buf = (char*)realloc(buf, bufsize);
			}
			memcpy(buf + size, data, status);
			size += status;

			gettimeofday(&begin, NULL);

			if(status < CHUNK_SIZE){
				break;
			}
		}
	}

	// Find sequence ID
	int pos = 0;
	while(buf[++pos] != '\n');
	int seq;
	sscanf(buf + pos + sizeof("Cseq:"), "%d", &seq);

#define SEND_MESSAGE(_m, _a...) \
	char _b[sizeof(#_m) + 256];\
	sprintf(_b, _m, ##_a); \
	printf(_m, ##_a); \
	write(fd, _b, strlen(_b));
	
	printf("%s\n", buf);
	// Send reply
	if(strncmp(buf, "OPTIONS", 4) == 0){
		SEND_MESSAGE("RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n", seq);
	}else if(strncmp(buf, "DESCRIBE", 4) == 0){
		const char description[] = "v=0\r\n"
			"i=Test server\r\n"
			"m=audio 0 RTP/AVP 97\r\n"
			"a=control:streamid=1\r\n"
			"a=range:-1\r\n"
			"a=length:-1";

		SEND_MESSAGE("RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-Type: application/sdp\r\nContent-Length: %d\r\n\r\n%s", seq, (int)strlen(description), description);
	}else if(strncmp(buf, "SETUP", 4) == 0){
		SEND_MESSAGE("RTSP/10. 200 OK\r\nCseq: %d\r\nTransport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257\r\n\r\n", seq);
	}

	return 0;
}

int port, fd, childfd;

void stop(int sig)
{
	printf("\n%d - Closing server\n", sig);

	close(fd);
	close(childfd);
	exit(0);
}

int main(int argc, char *argv[])
{
	port = 554;

	int i;
	for(i = 1; i < argc; i++){
		if(argv[i][0] == '-'){
			switch(argv[i][1]){
				case 'p':
					sscanf(argv[i + 1], "%d", &port);
					i++;
				break;
			}
		}
	}

	printf("Port: %d\n", port);

	childfd = 0;
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(fd < 0){
		perror(NULL);
		close(fd);
		return 1;
	}

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int yes = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0){
		goto error;
	}

	if(bind(fd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in)) < 0){
		goto error;
	}

	if(listen(fd, MAX_CONNECTIONS) < 0){
		goto error;
	}

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

	signal(SIGINT, stop);

	while(1){
		struct sockaddr_in cliaddr;
		unsigned int clilen = sizeof(cliaddr);
		childfd = accept(fd, (struct sockaddr*)&cliaddr, &clilen);
		if(childfd > 0){
			fcntl(childfd, F_SETFL, fcntl(childfd, F_GETFL) | O_NONBLOCK);

			while(parseInput(childfd) != -1);

			close(childfd);
		}
	}

	close(fd);

	return 0;

error:
	printf("Error!\n");
	perror(NULL);

	close(fd);
	close(childfd);
	return 1;
}
