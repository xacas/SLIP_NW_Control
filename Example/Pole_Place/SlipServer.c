#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>

// M系列の更新周期
#define MSEQ_WIDTH 120
// Quantize gain
#define Q_GAIN 25.0

void server(int sockfd);

int main(void){

	int sockfd;
	int new_sockfd;
	int writer_len;
	struct sockaddr_in reader_addr;
	struct sockaddr_in writer_addr;

	/* make socket */
	if((sockfd = socket(PF_INET,SOCK_STREAM,0)) < 0){
		perror("fail at make reader socket");
		exit(1);
	}
	char *opt;
	opt = "sl0";
	if(setsockopt(sockfd,SOL_SOCKET,SO_BINDTODEVICE,opt,3) < 0){
		perror("fail at setsocket opt");
		exit(1);
	}

	/* Setting Communication Port, address  */
	bzero((char *) &reader_addr,sizeof(reader_addr));
	reader_addr.sin_family=PF_INET;
	reader_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	//reader_addr.sin_addr.s_addr=inet_addr("192.168.5.1");
	reader_addr.sin_port=htons(8000);

	/* bind addres to socket  */
	if(bind(sockfd, (struct sockaddr *)&reader_addr, sizeof(reader_addr)) < 0){
		perror("fail at bind socket");
		exit(1);
	}

	/* setting conect request */
	if(listen(sockfd,5)<0){
		perror("fail at listen");
		close(sockfd);
		exit(1);
	}

	while(1){
		/* waiting conect request */
		if((new_sockfd = accept(sockfd,(struct sockaddr *)&writer_addr,&writer_len)) < 0){
			perror("fail at accept");
			exit(1);
		}
		server(new_sockfd);
	}
	close(sockfd);
	return 0;
}

char quantizer(float sig){
    return (char)(sig*Q_GAIN);
}

float dequantizer(char sig){
    return (float)(sig/Q_GAIN);
}

float control(float Vo, float V1){
	return -2.9145*Vo - 0.0043*V1;
}

void server(int sockfd){
	char buf[8]={};
	char write_buf[8]={};
	float Vo,V1,Vi;
	FILE *fp;

	read(sockfd,buf,2);
	Vo=dequantizer(buf[0]);
	V1=dequantizer(buf[1]);
	Vi=control(Vo,V1);

	printf("%f,%f,%f\n",Vo,V1,Vi);

	write_buf[0]=quantizer(Vi);
	write(sockfd,write_buf,1);

	if((fp=fopen("data.csv","a")) != NULL){
		fprintf(fp,"%f,%f\n",Vo,V1);
		fclose(fp);
	}

	close(sockfd);
}

