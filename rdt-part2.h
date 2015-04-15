/**************************************************************
rdt-part2.h
Student name: Huang Yukun
Student No. : 3035030808
Date and version: 4/13/2015, v1.0
Development platform: Ubuntu 12.04
Development language: C
Compilation:
	Can be compiled with g++
*****************************************************************/

#ifndef RDT2_H
#define RDT2_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>

#define PAYLOAD 1000		//size of data payload of the RDT layer
#define TIMEOUT 50000		//50 milliseconds
#define TWAIT 10*TIMEOUT	//Each peer keeps an eye on the receiving  
							//end for TWAIT time units before closing
							//For retransmission of missing last ACK

//----- Type defines ----------------------------------------------------------
typedef unsigned char		u8b_t;    	// a char
typedef unsigned short		u16b_t;  	// 16-bit word
typedef unsigned int		u32b_t;		// 32-bit word 

extern float LOSS_RATE, ERR_RATE;


/* this function is for simulating packet loss or corruption in an unreliable channel */
/***
Assume we have registered the target peer address with the UDP socket by the connect()
function, udt_send() uses send() function (instead of sendto() function) to send 
a UDP datagram.
***/
int udt_send(int fd, void * pkt, int pktLen, unsigned int flags) {
	double randomNum = 0.0;

	/* simulate packet loss */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < LOSS_RATE){
		//simulate packet loss of unreliable send
		printf("WARNING: udt_send: Packet lost in unreliable layer!!!!!!\n");
		return pktLen;
	}

	/* simulate packet corruption */
	//randomly generate a number between 0 and 1
	randomNum = (double)rand() / RAND_MAX;
	if (randomNum < ERR_RATE){
		//clone the packet
		u8b_t errmsg[pktLen];
		memcpy(errmsg, pkt, pktLen);
		//change a char of the packet
		int position = rand() % pktLen;
		if (errmsg[position] > 1) errmsg[position] -= 2;
		else errmsg[position] = 254;
		printf("WARNING: udt_send: Packet corrupted in unreliable layer!!!!!!\n");
		return send(fd, errmsg, pktLen, 0);
	} else 	// transmit original packet
		return send(fd, pkt, pktLen, 0);
}

/* this function is for calculating the 16-bit checksum of a message */
/***
Source: UNIX Network Programming, Vol 1 (by W.R. Stevens et. al)
***/
u16b_t checksum(u8b_t *msg, u16b_t bytecount)
{
	u32b_t sum = 0;
	u16b_t * addr = (u16b_t *)msg;
	u16b_t word = 0;
	
	// add 16-bit by 16-bit
	while(bytecount > 1)
	{
		sum += *addr++;
		bytecount -= 2;
	}
	
	// Add left-over byte, if any
	if (bytecount > 0) {
		*(u8b_t *)(&word) = *(u8b_t *)addr;
		sum += word;
	}
	
	// Fold 32-bit sum to 16 bits
	while (sum>>16) 
		sum = (sum & 0xFFFF) + (sum >> 16);
	
	word = ~sum;
	
	return word;
}

//----- Type defines ----------------------------------------------------------

// define your data structures and global variables in here
typedef u8b_t Packet[PAYLOAD+4];
typedef u8b_t ACK[4];

//global variables
//expected seq num
u8b_t expectedseqnum = '0'; //receiver use
//last acknum
u8b_t last_acknum = '1';


int client_sender_already_to_receiver = 0;
int client_sender_already_back_to_sender = 0;
int server_fd = 0;
int client_fd = 0;

int rdt_socket();
int rdt_bind(int fd, u16b_t port);
int rdt_target(int fd, char * peer_name, u16b_t peer_port);
int rdt_send(int fd, char * msg, int length);
int rdt_recv(int fd, char * msg, int length);
int rdt_close(int fd);

/* Application process calls this function to create the RDT socket.
   return	-> the socket descriptor on success, -1 on error 
*/
int rdt_socket() {
//same as part 1
	return socket(AF_INET, SOCK_DGRAM, 0);
}

/* Application process calls this function to specify the IP address
   and port number used by itself and assigns them to the RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_bind(int fd, u16b_t port){
//same as part 1
   struct sockaddr_in myaddr;
   myaddr.sin_family = AF_INET;
   myaddr.sin_port = htons(port);
   myaddr.sin_addr.s_addr = INADDR_ANY;
   return bind(fd, (struct sockaddr*) &myaddr, sizeof myaddr);
}

/* Application process calls this function to specify the IP address
   and port number used by remote process and associates them to the 
   RDT socket.
   return	-> 0 on success, -1 on error
*/
int rdt_target(int fd, char * peer_name, u16b_t peer_port){
//same as part 1
   struct addrinfo hints, *res;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_DGRAM;

   char port[sizeof peer_port];
   sprintf(port,"%d" ,peer_port);
   getaddrinfo(peer_name, port, &hints, &res);

   return connect(fd, res->ai_addr, res->ai_addrlen);
}

/* Application process calls this function to transmit a message to
   target (rdt_target) remote process through RDT socket.
   msg		-> pointer to the application's send buffer
   length	-> length of application message
   return	-> size of data sent on success, -1 on error
*/
int rdt_send(int fd, char * msg, int length){
//implement the Stop-and-Wait ARQ (rdt3.0) logic
  if (server_fd == 0)
  	server_fd = fd; //set up only once
  if (client_sender_already_to_receiver == 1)
  	client_sender_already_back_to_sender = 1;
  Packet pkt;
  //control info
  pkt[0] = '1'; // type of packet
  if (last_acknum == '1'){
  	pkt[1] = '0';
  }else{
  	pkt[1] = '1';
  }
  // copy application data to payload field
  memcpy(&pkt[4], msg, length); // length of data
  //set checksum field to zero
  pkt[2] = '0';
  pkt[3] = '0';
  //calculate checksum for whole packet
  u16b_t ckm = checksum(pkt, length+4);
  //set checksum in the header
  memcpy(&pkt[2], (unsigned char*)&ckm, 2);
  int send;
  if((send = udt_send(fd, pkt, length+4, 0)) == -1){
  	perror("send");
  }
  // printf("rdt_send msg of size%d\n", length+4);
  // printf("packet of seq # = %c\n", pkt[1]);

  struct timeval timer;
  //setting description set
  fd_set read_fds;
  FD_ZERO(&read_fds);

  FD_SET(fd, &read_fds);

  
  u8b_t buf[4]; //header size

  //do
  for(;;) {
	  //repeat until received expected ACK
	  //setting timeout
	  timer.tv_sec = 0;
	  timer.tv_usec = TIMEOUT;
	  // <0 => error
	  // ==0 => timeout
	  // >0 => receive a packet
	  int status;
	  status = select(fd+1, &read_fds, NULL, NULL, &timer);
	  if (status == -1){
	  	perror("select ");
	  	exit(4);
	  } else if(status == 0){
	  	//timeout happens
	  	//retransmit the packet
	  	int send;
	  	if((send = udt_send(fd, pkt, length+4, 0)) == -1){
	  		perror("send");
	  	}
	  	printf("Retrans msg of size%d, seq#=%c\n", length+4, pkt[1]);
	  	FD_SET(fd, &read_fds);
	  	continue;
	  }
	  for(;;){
	  	if (FD_ISSET(fd, &read_fds)){
	  		int nbytes;
	  		nbytes = recv(fd, buf, sizeof buf, 0);
	  		u16b_t ACK_check = checksum(buf, 4);
	  		u8b_t checksum_in_char[2];
			memcpy(&checksum_in_char[0], (unsigned char*)&ACK_check, 2);
			// printf("checksum_in_char: %u, %u\n", checksum_in_char[0], checksum_in_char[1]);
	  		if (nbytes <= 0) {
				// got error or connection closed by client
				if (nbytes == 0) {
				// connection closed
					printf("selectserver: socket %d hung up\n", fd);
				} else {
					perror("recv");
				}
			}else {
				if (checksum_in_char[0]!='0' || checksum_in_char[1]!='0'){
					//not necessarily corrupted
					if (buf[0] == '0'){
						perror("corrupted");
						break;
					}
					else{
						//not a very good scheme for detecting data, the first bit can be corrupted
						//data
						ACK ack;
						ack[0] = '0'; //packet type, 0 is ACK
						ack[2] = '0';
						ack[3] = '0';
						ack[1] = '0';//it can only 0 based on the sequence
						//so resend the ACK if it's only if it's "old"
						if (fd == client_fd && client_sender_already_back_to_sender != 1){
							//ignore the new data msg
							ack[1] = '1';
							u16b_t ckm = checksum(ack, 4);
							memcpy(&ack[2], (unsigned char*)&ckm, 2);
							udt_send(fd, ack, 4, 0);
							printf("UNIQUE ACK1 sent\n");
							FD_SET(fd, &read_fds);
							break;
						}
						else if (fd == server_fd && client_sender_already_to_receiver == 1){
							u16b_t ckm = checksum(ack, 4);
							memcpy(&ack[2], (unsigned char*)&ckm, 2);
							if (udt_send(fd, ack, 4, 0) == -1){
								perror("send");
							}
							else{
								printf("UNIQUE ACK0 sent\n");
								FD_SET(fd, &read_fds);
								break;
							}
						}
						else{
							//ignore
							break;
						}
					}
				}
				else{
					//if is ACK
					if (buf[0] == '0'){
						//by this we assume that checksum has guaranteed no error will occur
						if (last_acknum == buf[1]){
							//not the expected ACK
							printf("not expected ACK\n");
							send = udt_send(fd, pkt, length+4, 0);
							// printf("Retrans msg of size%d, seq#=%c\n", length+4, pkt[1]);
							FD_SET(fd, &read_fds);
							break;
						}
						else{
							if (last_acknum == '1'){
								last_acknum = '0';
								printf("receive ACK0\n");
								return length+4;
							}
							else{
								last_acknum = '1';
								printf("receive ACK1\n");
								return length+4;
							}
						}
					}
					else {
						// //if is data
						// printf("SENDER receive Dataaaaaaaaaaaaaaaaaaaa\n");
						// //this is a sender, how can we receive it
						// //ignore it
					}
				}
			}
	  	}
	  }
  }
}

/* Application process calls this function to wait for a message 
   from the remote process; the caller will be blocked waiting for
   the arrival of the message.
   msg		-> pointer to the receiving buffer
   length	-> length of receiving buffer
   return	-> size of data received on success, -1 on error
*/
int rdt_recv(int fd, char * msg, int length){
//implement the Stop-and-Wait ARQ (rdt3.0) logic
	if (client_fd == 0)
		client_fd = fd;
	if (fd == server_fd)
		client_sender_already_to_receiver = 1;
	for(;;) {
		int receiveBytes = recv(fd, msg, length+4, 0);
		printf("receive msg of size %d\n", receiveBytes);
		if (receiveBytes <= 0){
			if (receiveBytes == 0) {
				// connection closed
					printf("selectserver: socket %d hung up\n", fd);
				} else {
					perror("recv");
				}
		}
		u8b_t* checksum_msg = (u8b_t*)msg;
		u16b_t ckm = checksum(checksum_msg, receiveBytes);
		u8b_t checksum_in_char[2];
		memcpy(&checksum_in_char[0], (unsigned char*)&ckm, 2);
		//make ACK packet
		ACK ack;
		ack[0] = '0'; //packet type, 0 is ACK
		ack[2] = '0';
		ack[3] = '0';
		if (checksum_in_char[0] != '0' || checksum_in_char[1] != '0'){
			//corrupted
			printf("recv corrupted\n");
			if (expectedseqnum == '0')
				ack[1] = '1';
			else
				ack[1] = '0';
			//calculate checksum for ACK
			u16b_t ckm = checksum(ack, 4);
			// printf("unsigned short ckm: %hu\n", ckm);
			//set checksum in the header
			memcpy(&ack[2], (unsigned char*)&ckm, 2);
			//so resend the ACK
			if (udt_send(fd, ack, 4, 0) == -1){
				perror("send");
			}
			else{
				printf("resend ACK%c\n", ack[1]);
			}
		}else{
			if (msg[0] == '1'){
				//is DATA
				if (msg[1] == expectedseqnum){
					if (msg[1] == '1'){
						ack[1] = '1';
						u16b_t ckm = checksum(ack, 4);
						// printf("unsigned short ckm: %hu\n", ckm);
						memcpy(&ack[2], (unsigned char*)&ckm, 2);
						if (udt_send(fd, ack, 4, 0) == -1){
							perror("send");
						}
						else{
							printf("ACK1 sent\n");
							expectedseqnum = '0';
							for (int i=0; i < receiveBytes-4; i++){
								msg[i] = msg[i+4];
							}
							msg[receiveBytes-4] = '\0';
							return receiveBytes-4;
						}
					}
					else{
						ack[1] = '0';
						u16b_t ckm = checksum(ack, 4);
						// printf("unsigned short ckm: %hu\n", ckm);
						memcpy(&ack[2], (unsigned char*)&ckm, 2);
						if (udt_send(fd, ack, 4, 0) == -1){
							perror("send");
						}
						else{
							printf("ACK0 sent\n");
							expectedseqnum = '1';
							for (int i=0; i < receiveBytes-4; i++){
								msg[i] = msg[i+4];
							}
							msg[receiveBytes-4] = '\0';
							return receiveBytes-4;
						}
					}
				}else{
					//not expected data, resend last ACK
					if (expectedseqnum == '0'){
						ack[1] = '1';
					}
					else{
						ack[1] = '0';
					}
					//calculate checksum for ACK
					u16b_t ckm = checksum(ack, 4);
					//set checksum in the header
					memcpy(&ack[2], (unsigned char*)&ckm, 2);
					//so resend the ACK
					if (udt_send(fd, ack, 4, 0) == -1){
						perror("send");
					}
					else{
						printf("resend ACK%c\n", ack[1]);
					}
				}
			}else{
				//is ACK
				ACK ack;
				ack[0] = '0'; //packet type, 0 is ACK
				ack[2] = '0';
				ack[3] = '0';
				ack[1] = '1';
				u16b_t ckm = checksum(ack, 4);
				// printf("unsigned short ckm: %hu\n", ckm);
				memcpy(&ack[2], (unsigned char*)&ckm, 2);
				if (udt_send(fd, ack, 4, 0) == -1){
					perror("send");
				}
				else{
					printf("ACK1 sent, for replying sender\n");
				}
			}
		}
	}
}

/* Application process calls this function to close the RDT socket.
*/
int rdt_close(int fd){
//implement the Stop-and-Wait ARQ (rdt3.0) logic
	struct timeval timer;
	//setting description set
	fd_set read_fds;
	FD_ZERO(&read_fds);

	FD_SET(fd, &read_fds);
	int status;
	u8b_t buf[4];
	for (;;){
		//setting timeout
		timer.tv_sec = 0;
		timer.tv_usec = TWAIT;
		status = select(fd+1, &read_fds, NULL, NULL, &timer);
		if (status == -1){
		  	perror("select ");
		  	exit(4);
		} else if(status == 0){
		  	//timeout happens
		  	return close(fd);
		}
		else{
		  	if (FD_ISSET(fd, &read_fds)){
		  		recv(fd, buf, sizeof buf, 0);
		  		u16b_t ACK_check = checksum(buf, 4);
		  		u8b_t checksum_in_char[2];
				memcpy(&checksum_in_char[0], (unsigned char*)&ACK_check, 2);
				if (checksum_in_char[0] =='0' && checksum_in_char[1] =='0'){
					//ACK
					printf("get ACK, terminate\n");
				}
				else{
					ACK ack;
					ack[0] = '0';
					ack[2] = '0';
					ack[3] = '0';
					ack[1] = expectedseqnum;//need to be modified/checked
					u16b_t ckm = checksum(ack, 4);
					memcpy(&ack[2], (char*)&ckm, 2);
					if (udt_send(fd, ack, 4, 0) == -1){
						perror("send");
					}
					else{
						if (expectedseqnum == '0')
							expectedseqnum = '1';
						else
							expectedseqnum = '0';
					}
				}
			}
		}
	}
}

#endif
