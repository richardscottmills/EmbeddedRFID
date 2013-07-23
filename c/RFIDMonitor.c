	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/ipc.h>
	#include <sys/shm.h>
	#include <fcntl.h>
	#include <termios.h>
	#include <stdio.h>
	#include <unistd.h>
        
	#define BAUDRATE B57600
	#define MODEMDEVICE "/dev/ttyAM0"
	#define _POSIX_SOURCE 1
	#define FALSE 0
	#define TRUE 1
	#define SHMSZ 256
      
	volatile int STOP=FALSE; 
	int fd,rx_chars,firstevent,packetlen,returnV,shmid,RxArrPos=0,inventoryRecieved=FALSE,dataPacketRecieved=FALSE,sid_valid=0,readAllCompleted=FALSE;
	unsigned char inventoryPacket[12],UIDArray[30][10],oldUIDString[128],inUIDString[128],RxArr[256],ANI[12]="8032766292 ",udpOutBuf[64]="",*shm,*s;
	struct termios oldtio,newtio;
	key_t key = 5678;
  
       
	main() {
        int res,i;
        unsigned char buf[3];
        
        printf("RFIDMonitor:SYSTEM Starting.\n");
        
        printf("RFIDMonitor:IPC Starting \n");
		if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
			printf("RFIDMonitor:IPC:ERROR Failed to create shared memory segment.\n");
			exit(0);
		}
		if ((shm = shmat(shmid, NULL, 0)) == (unsigned char *) -1) {
			printf("RFIDMonitor:IPC:ERROR Failed to attach shared memory segment to data space.\n");
			exit(0);
		}

        	inventoryPacket[0] = 0x01;
        	inventoryPacket[1] = 0x0D;
        	inventoryPacket[2] = 0x00;
        	inventoryPacket[3] = 0x00;
        	inventoryPacket[4] = 0x00;
        	inventoryPacket[5] = 0x00;
        	inventoryPacket[6] = 0x60;
        	inventoryPacket[7] = 0x03;
        	inventoryPacket[8] = 0x07;
        	inventoryPacket[9] = 0x01;
        	inventoryPacket[10] = 0x00;
        	inventoryPacket[11] = 0x69;
        	inventoryPacket[12] = 0x96;

        
        	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY ); 
        	if (fd <0) {perror(MODEMDEVICE); exit(-1); }
        
        	tcgetattr(fd,&oldtio); /* save current port settings */
        
        	bzero(&newtio, sizeof(newtio));
        	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        	newtio.c_iflag = 0;
        	newtio.c_oflag = 0;
        
        	/* set input mode (non-canonical, no echo,...) */
        	newtio.c_lflag = 0;
         
        	newtio.c_cc[VTIME]    = 0; /* inter-character timer unused */
        	newtio.c_cc[VMIN]     = 3; /* blocking read until 3 chars received */
        
		while(1) {
			//sleep(1);
        	tcflush(fd, TCIFLUSH);
        	tcsetattr(fd,TCSANOW,&newtio);
			
			write(fd,inventoryPacket,13);
			//printf("Wrote inv req packet (write returned %i)\n",write(fd,inventoryPacket,13));
			firstevent=TRUE;
			inventoryRecieved=FALSE;
			rx_chars=0;
			setVMIN(3);
        	while (inventoryRecieved==FALSE) {  /* loop for inventory packet */
				//printf("waiting for response...\n");
				res = read(fd,&buf,newtio.c_cc[VMIN]); /* returns after VMIN chars have been input */
				processInput(&buf,res);
        	}
			//printf("read inv res packet rx_chars=%i\n",rx_chars);
			
			
			i=0;
			while (i < sid_valid) {
				UIDArray[i][0]=0;
				UIDArray[i][1]=0;
				UIDArray[i][2]=0;
				UIDArray[i][3]=0;
				UIDArray[i][4]=0;
				UIDArray[i][5]=0;
				UIDArray[i][6]=0;
				UIDArray[i][7]=0;
				UIDArray[i][8]=0;
				UIDArray[i][9]=0xCC;
				i++;
			}
			
			if (ProcessInventoryPacket() == TRUE) { /* if inventory successful read data from all RFID tags */
				if (memcmp(oldUIDString,inUIDString,sid_valid*8) != 0 || memcmp(oldUIDString,"ERROR",5) == 0) {
					memcpy(oldUIDString,inUIDString,sid_valid*8);
        				if (doReadAll() != TRUE) {
							sid_valid=0;
							strcpy(oldUIDString,"ERROR");
							//printf("readAll failed\n");
						}
						else {
							//printf("(memcmp(oldUIDString,inUIDString,sid_valid*8) != 0)=%i\n",(memcmp(oldUIDString,inUIDString,sid_valid*8) != 0));
							//printf("(memcmp(oldUIDString,""ERROR"",5) == 0)=%i\n",(memcmp(oldUIDString,"ERROR",5) == 0));
						}
						writeToSharedMemory();
				}
				else {
					//printf("readAll skipped\n");
					//printf("(memcmp(oldUIDString,inUIDString,sid_valid*8) != 0)=%i\n",(memcmp(oldUIDString,inUIDString,sid_valid*8) != 0));
					//printf("(memcmp(oldUIDString,""ERROR"",5) == 0)=%i\n",(memcmp(oldUIDString,"ERROR",5) == 0));
				}			
			}
			else {
				//printf("ProcessInventoryPacket failed\n");
			}
		}

        tcsetattr(fd,TCSANOW,&oldtio);
		printf("RFIDMonitor:SYSTEM Exiting.\n");
	}

	int processInput(unsigned char RxData[], int dlen) {  
		int arr_index=0;
		

		if (firstevent == TRUE) {        		

			//Save the data into array
			while (arr_index < dlen) {
				RxArr[arr_index+rx_chars] = RxData[arr_index];
				arr_index++;
			}
			rx_chars = rx_chars + dlen;
			packetlen = RxData[1] + (RxData[2] * 256); //Get the number of bytes in the response packet
			setVMIN(1); //Set the Receive Threshold for one byte
			firstevent = FALSE;
		}
		else {

			while (arr_index < dlen) {
				RxArr[arr_index+rx_chars] = RxData[arr_index];
				arr_index++;
			}
			rx_chars = rx_chars + dlen;
		}
		if (rx_chars == packetlen) {
			endCommand();
		}
	}
	
	int endCommand() {
		if (inventoryRecieved==FALSE) {
			inventoryRecieved=TRUE;
		}
		else {
			dataPacketRecieved=TRUE;
		}
	}
	
	int setVMIN(int newVMIN) {
		newtio.c_cc[VMIN]=newVMIN;   /* blocking read until newVMIN chars received */
		tcsetattr(fd,TCSANOW,&newtio);
	}
	
	
	int ProcessInventoryPacket() {

		int valid_mask=0x01,k=0,RxArrPos=0,i=0;
		unsigned char bcclsb=0x00,bccmsb=0x00;


		sid_valid=0;

		while (i < rx_chars - 2) {
            		bcclsb = bcclsb ^ RxArr[i];
	    	i++;
		}

		if (bcclsb != RxArr[i]) {
            		printf("RFIDMonitor:RFID:INVENTORY:ERROR Inventory CRC error (LSB)\n");
            		return(FALSE);
		}
		i++;
		bccmsb = ~ bcclsb;
		if (bccmsb != RxArr[i]) {
            		printf("RFIDMonitor:RFID:INVENTORY:ERROR Inventory CRC error (MSB)\n");
            		return(FALSE);
		}

		if (RxArr[5] == 0x10) {
            		printf("RFIDMonitor:RFID:INVENTORY:ERROR Inventory packet error flag set.\n");
            		return(FALSE);
		}


		//Get the number of UID's found
		valid_mask = 0x01;
		i=0;

		while (i <= 7) {
            		if (valid_mask & RxArr[7]) {
        			sid_valid++;
            		}
            		valid_mask = (valid_mask * 2) & 0xFF;
    	    		i++;
		}
		valid_mask = 0x01;
		i=0;
		while (i <= 7) {
            	if (valid_mask & RxArr[8]) {
        		sid_valid++;
            	}
            	valid_mask = (valid_mask * 2) & 0xFF;
    	    	i++;
		}
    
		if (sid_valid == 0) {
			return(FALSE);
		}
    
    
		//Get the UID's
		i=0;
		k=0;
		RxArrPos=rx_chars-3;
		while (i < sid_valid) {
			k=0;
			while (k < 8) {
				UIDArray[i][k]=RxArr[RxArrPos];
				inUIDString[i*8+k]=RxArr[RxArrPos];
				RxArrPos--;
				k++;
			}
			RxArrPos=RxArrPos-2;
			i++;
		}
		return(TRUE);
    	}


	int doReadAll() {
		unsigned char TxArr[22],bccmsb,bcclsb,buf[3];
    	int count,i=0,res;
    		
		tcflush(fd, TCIFLUSH);
        tcsetattr(fd,TCSANOW,&newtio);
		
    		while (i < sid_valid) {

         		TxArr[0] = 0x01;
         		TxArr[1] = 0x16;
         		TxArr[2] = 0x00;
        		TxArr[3] = 0x00;
         		TxArr[4] = 0x00;
         		TxArr[5] = 0x10;
         		TxArr[6] = 0x60;
         		TxArr[7] = 0x03;
         		TxArr[8] = 0x63;
         		TxArr[9] = 0x23;
         		TxArr[10] = UIDArray[i][7];
         		TxArr[11] = UIDArray[i][6];
         		TxArr[12] = UIDArray[i][5];
         		TxArr[13] = UIDArray[i][4];
         		TxArr[14] = UIDArray[i][3];
         		TxArr[15] = UIDArray[i][2];
         		TxArr[16] = UIDArray[i][1];
         		TxArr[17] = UIDArray[i][0];
         		TxArr[18] = 0x00;
         		TxArr[19] = 0x0F;
         
	 
	 		count=0;
			bccmsb=0x00;
			bcclsb=0x00;
         		/* Compute CRC */
			while (count < 20) {
             		bcclsb = bcclsb ^ TxArr[count];
         			count++;
			}
 			bccmsb = ~ bcclsb;
         
         	TxArr[20] = bcclsb;
         	TxArr[21] = bccmsb;
			

                 
         	setVMIN(3);
         	returnV = write(fd,TxArr,22);

			
			firstevent=TRUE;
			dataPacketRecieved=FALSE;
			rx_chars=0;
			
			while (dataPacketRecieved==FALSE) {
				res = read(fd,&buf,newtio.c_cc[VMIN]);   /* returns after VMIN chars have been input */
				processInput(&buf,res);
        		}
			if (ProcessDataPacket(i)!=TRUE) {
				return(FALSE);
			}
    		i++;
    		}
		return(TRUE);
	}
	
	
	int ProcessDataPacket(int UIDArrayPos) {
    		unsigned char bcclsb,bccmsb;
    		int i=0;


    		// check CRC 
    		bcclsb = 0x00;
    		bccmsb = 0x00;

    
		while (i < rx_chars - 2) {
            		bcclsb = bcclsb ^ RxArr[i];
	    	i++;
		}

		if (bcclsb != RxArr[i]) {
            		printf("RFIDMonitor:RFID:BLOCKREAD:ERROR Data CRC error (LSB)\n");
            		return(FALSE);
		}
		i++;
		bccmsb = ~ bcclsb;
		if (bccmsb != RxArr[i]) {
            		printf("RFIDMonitor:RFID:BLOCKREAD:ERROR Data CRC Error (MSB)\n");
            		return(FALSE);
		}
		if (RxArr[5] == 0x10) {
            		printf("RFIDMonitor:RFID:BLOCKREAD:ERROR Data packet error flag set.\n");
            		return(FALSE);
		}
		
		UIDArray[UIDArrayPos][8]=RxArr[12];
		if (RxArr[12]!=0xFF) {
			UIDArray[UIDArrayPos][9]=RxArr[11];
		}
		else {
			UIDArray[UIDArrayPos][9]=0xFF;
		}
		return(TRUE);
	}
    
    int writeToSharedMemory() {
		unsigned char boxId[9],days[128],outPutBuf[128];
		int UIDArrayIndex,innerUIDIndex,dayscount,c;
		
		outPutBuf[0]=0;
		UIDArrayIndex=0;
		innerUIDIndex=0;
		dayscount=0;
		boxId[0]=0;
		boxId[1]=0;
		boxId[2]=0;
		boxId[3]=0;
		boxId[4]=0;
		boxId[5]=0;
		boxId[6]=0;
		boxId[7]=0;
		boxId[8]=0;
	
			
		if (sid_valid > 0 ) {
			while  (UIDArrayIndex < sid_valid) {
				if (UIDArray[UIDArrayIndex][8]==0xFF) {
					while (innerUIDIndex < 8) {
						boxId[innerUIDIndex]=UIDArray[UIDArrayIndex][innerUIDIndex];
						innerUIDIndex++;
					}
				}
				else {
					if (UIDArray[UIDArrayIndex][9] != 0xCC) {
						days[dayscount++]=0x20; /*ASCII SPACE*/
						days[dayscount++]=0x30;
						days[dayscount++]=UIDArray[UIDArrayIndex][9]+0x30;
						days[dayscount++]=0x3A;
						days[dayscount++]=0x30;
						days[dayscount++]=0x30;
					}
				}
				UIDArrayIndex++;
			}
			days[dayscount]=0;
			sprintf(boxId,"%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",boxId[0],boxId[1],boxId[2],boxId[3],boxId[4],boxId[5],boxId[6],boxId[7]);
			strcat(outPutBuf,ANI);
			strcat(outPutBuf,boxId);
			strcat(outPutBuf,days);
			printf("%s\n",outPutBuf);
			s = shm;
			for (c = 0; c < strlen(outPutBuf) ; c++)
				*s++ = outPutBuf[c];
			*s = 0x00;
		}
		else {
			sprintf(boxId,"%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",boxId[0],boxId[1],boxId[2],boxId[3],boxId[4],boxId[5],boxId[6],boxId[7]);
			strcat(outPutBuf,ANI);
			strcat(outPutBuf,boxId);
			printf("%s\n",outPutBuf);
			s = shm;
			for (c = 0; c < strlen(outPutBuf) ; c++)
				*s++ = outPutBuf[c];
			*s = 0x00;
		}
	}
	
	
