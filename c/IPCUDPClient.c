#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/time.h> /* select() */ 
#include <stdio.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset() */

#define REMOTE_SERVER_PORT 1500
#define MAX_MSG 100
#define SHMSZ 256

main(int argc, char *argv[])
{
    int shmid,sd,rc;
    key_t key = 5678;
    char *shm, *s;
	struct sockaddr_in cliAddr, remoteServAddr;
	struct hostent *h;
    
    
      /* check command line args */
	if(argc<2) {
		printf("usage : %s <server>\n", argv[0]);
		exit(1);
	}
    
    printf("IPCUDPClient:SYSTEM Starting.\n");
    
    if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
        printf("IPCUDPClient:IPC:ERROR Failed to create shared memory segment.\n");
        exit(0);

    }

    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        printf("IPCUDPClient:IPC:ERROR Failed to attach shared memory segment to data space.\n");
        exit(0);
    }
	

  /* get server IP address (no check if input is IP address or DNS name */
  h = gethostbyname(argv[1]);
  if(h==NULL) {
    printf("IPCUDPClient:UDP:ERROR Unknown host '%s' \n",argv[1]);
    exit(1);
  }

  printf("IPCUDPClient:UDP Reporting to '%s' (IP : %s) \n",h->h_name,inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

  remoteServAddr.sin_family = h->h_addrtype;
  memcpy((char *) &remoteServAddr.sin_addr.s_addr, 
	 h->h_addr_list[0], h->h_length);
  remoteServAddr.sin_port = htons(REMOTE_SERVER_PORT);

  /* socket creation */
  sd = socket(AF_INET,SOCK_DGRAM,0);
  if(sd<0) {
    printf("IPCUDPClient:UDP:ERROR Cannot open socket.\n");
    exit(1);
  }
  
  /* bind any port */
  cliAddr.sin_family = AF_INET;
  cliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  cliAddr.sin_port = htons(0);
  
  rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
  if(rc<0) {
    printf("IPCUDPClient:UDP:ERROR Cannot bind port.\n");
    exit(1);
  }

    while (1) {
    	sleep(15);
	s = shm;
	rc = sendto(sd, s, strlen(s)+1, 0,(struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr));

    	if(rc<0) {
      		printf("IPCUDPClient:UDP:ERROR Cannot send data.\n");
      		close(sd);
      		exit(0);
    	}
  }
	
    printf("IPCUDPClient:SYSTEM Exiting.\n");
    exit(0);
}
