#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 100
#define CHUNK_SIZE 256
#define SEND_TIMEOUT 10

#define RTSP_STATUS_200 "RTPS/1.0 200 OK\r\n"
#define RTSP_STATUS_CSEQ "CSEQ: %d\r\n"
#define RTSP_STATUS_OPTIONS "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"

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

		fcntl(childfd, F_SETFL, O_NONBLOCK);
		while(1){
			char data[CHUNK_SIZE];
			int status = recv(childfd, data, CHUNK_SIZE, 0);
			if(status < 0){
				usleep(10000);
			}else if(status > 0){
				data[status] = '\0';
				printf("%s", data);

				if(status < CHUNK_SIZE){
					// Received full message
					char options[1024];
					int optsize = sprintf(options, RTSP_STATUS_200 RTSP_STATUS_CSEQ RTSP_STATUS_OPTIONS, 1);
					if(write(childfd, options, optsize) != optsize){
						goto error;
					}
					
					break;
				}
			}
		}

		fcntl(childfd, F_SETFL, fcntl(childfd, F_GETFL) & ~O_NONBLOCK);

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
