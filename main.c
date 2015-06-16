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
	fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) | O_NONBLOCK);

	size_t bufsize = CHUNK_SIZE;
	char *buf = (char*)malloc(bufsize);

	struct timeval begin;
	gettimeofday(&begin, NULL);

	size_t size = 0;
	while(1){
		struct timeval now;
		gettimeofday(&now, NULL);
		double timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
	
		if((size > 0 && timediff > RECV_TIMEOUT) || timediff > RECV_TIMEOUT * 2){
			printf("Time out on connection");
			return -1;
		}

		char data[CHUNK_SIZE];
		int status = recv(fd, data, CHUNK_SIZE, 0);
		if(status < 0){
			usleep(10000);
		}else if(status == 0){
			break;
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

	// Open socket as write file
	FILE *tcpwrite = fdopen(fd, "w");
	if(strncmp(buf, "OPTIONS", 4) == 0){
		fprintf(tcpwrite, "RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n\r\n", seq);
		printf("Send back response to OPTIONS\n");
	}else if(strncmp(buf, "DESCRIBE", 4) == 0){
		fprintf(tcpwrite, "RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-Type: application/sdp\r\nContent-Length: %d\r\n\r\n", seq, 0);
	}
	fflush(tcpwrite);
	fclose(tcpwrite);

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

	signal(SIGINT, stop);

	while(1){
		struct sockaddr_in cliaddr;
		unsigned int clilen = sizeof(cliaddr);
		childfd = accept(fd, (struct sockaddr*)&cliaddr, &clilen);
		if(childfd < 0){
			goto error;
		}

		parseInput(childfd);

		close(childfd);
	}

	close(fd);

	return 0;

error:
	perror(NULL);

	close(fd);
	close(childfd);
	return 1;
}
