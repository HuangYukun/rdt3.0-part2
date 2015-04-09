/*
** selectserver.c -- a cheezy multiperson chat server
**
** A sample program downloaded from the Beej's Guide 
** to Network Programming Web site:
** http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
**
** To connect to this server, use telnet as a client.
**
** I have amended the program to include the timeout feature -- atctam
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define PORT "49994"   // port we're listening on  - changed by atctam

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{   // the following three variables are related to the use of select() -- added by atctam
	fd_set master;    // master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select()
	int fdmax;        // maximum file descriptor number

	int sockfd;     // listening socket descriptor
	int newfd;        // newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	char buf[256];    // buffer for client data
	int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes=1;        // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;

	struct addrinfo hints, *ai, *p;
	
	struct timeval timer;  // for demonstration of timeout -- added by atctam
	int status;

	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	/*
	** This program uses a more intelligence method to assign/bind 
	** an IP address used by this machine to the new socket as a  
	** machine may have more than one IP addresses.
	** Please read slide 34 in 03-SocketProgramming for a brief
	** introduction of the use of getaddrinfo() -- added by atctam
	*/
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}

	/*
	** As the machine might have more than one IP addresses returned by
	** getaddrinfo(), we just select the first one in the list that suits
	** for our use -- added by atctam
	*/
	for(p = ai; p != NULL; p = p->ai_next) {
    	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0) { 
			continue;
		}
		
		// lose the pesky "address already in use" error message
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
			close(sockfd);
			continue;
		}

		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen
	if (listen(sockfd, 10) == -1) {
		perror("listen");
		exit(3);
	}

	// add the sockfd to the master set
	// Was initially empty -- added by atctam
	FD_SET(sockfd, &master);

	// keep track of the biggest file descriptor
	fdmax = sockfd; // so far, it's this one

	// main loop
	for(;;) {
		/* read_fds will be changed by select(); so we have to copy the 
		** master set to it before calling select() -- added by atctam
		*/
		read_fds = master; // copy it
		/*
		** set the timeval structure
		** to "unblock" select() call if nothing happens in 5 second
		** -- added by atctam to demonstrate how to set timeout
		*/
		timer.tv_sec = 5;
		timer.tv_usec = 0;
		if ((status = select(fdmax+1, &read_fds, NULL, NULL, &timer)) == -1) {
			perror("select");
			exit(4);
		} else if (status == 0) {  // added by atctam
			printf("selectserver: Nothing happened in 5 seconds\n");
			continue;
		}
		
		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == sockfd) {
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(sockfd,
						(struct sockaddr *)&remoteaddr,
						&addrlen);

					if (newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) {    // keep track of the max
							fdmax = newfd;
						}
						printf("selectserver: new connection from %s on "
							"socket %d\n",
							inet_ntop(remoteaddr.ss_family,
								get_in_addr((struct sockaddr*)&remoteaddr),
								remoteIP, INET6_ADDRSTRLEN),
							newfd);
					}
				} else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("selectserver: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					} else {
						// we got some data from a client
						for(j = 0; j <= fdmax; j++) {
							// send to everyone!
							if (FD_ISSET(j, &master)) {
								// except the sockfd and ourselves
								if (j != sockfd && j != i) {
									if (send(j, buf, nbytes, 0) == -1) {
										perror("send");
									}
								}
							}
						}
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
    
	return 0;
}

