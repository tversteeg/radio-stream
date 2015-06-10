#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 100
#define CHUNK_SIZE 256
#define SEND_TIMEOUT 10

int port;

int main(int argc, char *argv[])
{
	port = 9403;

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

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(fd < 0){
		perror(NULL);
		return 1;
	}

	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int yes = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0){
		perror(NULL);
		return 1;
	}

	if(bind(fd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in)) < 0){
		perror(NULL);
		return 1;
	}

	if(listen(fd, MAX_CONNECTIONS) < 0){
		perror(NULL);
		return 1;
	}

	while(1){
		struct sockaddr_in cliaddr;
		unsigned int clilen = sizeof(cliaddr);
		int newfd = accept(fd, (struct sockaddr*)&cliaddr, &clilen);
		if(newfd < 0){
			perror(NULL);
			return 1;
		}

		fcntl(newfd, F_SETFL, O_NONBLOCK);
		while(1){
			char data[CHUNK_SIZE];
			int status = recv(newfd, data, CHUNK_SIZE, 0);
			if(status < 0){
				usleep(10000);
			}else if(status > 0){
				data[status] = '\0';
				printf("%s", data);
				if(status < CHUNK_SIZE){
					close(fd);
					return 0;
				}
			}
		}

		fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL) & ~O_NONBLOCK);

		close(newfd);
	}

	close(fd);

	return 0;
}
