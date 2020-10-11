#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#define BUFFER_SIZE 4096 

// STUDENT    : THODORI KAPOURANIS
// PROFESSOR  : PROF. HAKNER 
// CLASS      : ECE-357 COMPUTER OPERATING SYSTEMS
// PROJECT    : PSET 1 QUESTION 3

// Redundant fix for some weird behavior I was experiencing
int bufferLength(char buffer[]){
  if (strlen(buffer)<=BUFFER_SIZE) return strlen(buffer);
  else return BUFFER_SIZE;
}


// loops through buffer until a binary character is found or end of buffer reached
int isBinary(char buffer[],int bufferLength){
  for (int c=0;c<bufferLength;c++)
    if (!(isprint(buffer[c]) || isspace(buffer[c]))){
      //printf("Found non-printable char: %d | %c at index %d\n",buffer[c],buffer[c],c);
      return 1;
    }
  return 0;
}


// Reports error information before exit(-1).
// Every file error will handled by this function. 
void errorHandler(int fd, char *fileName, char *action){
  if (fd<0){
    printf("\033[0;31m\nEncountered error %s: %s\n", action, fileName);
    perror("Error");
    exit(-1); 
  }
}


// Handles every file or hyphen argument. 
// All the reading and writing is here.
int argHandler(char *fileInputName, int ofd, char buffer[]){
  int ifd,rfd,wfd; // file input, read, write descriptors
  int byteCount=0;
  int writeCalls=0;
  int readCalls=1; //start with 1. Accounts for the last read, -1, or 0. Both of these will stop the loop and skip readCalls++.
  int binaryFlag=0;
  if  (strcmp(fileInputName,"-")==0){
    if (ifd=open(fileInputName,O_RDONLY)==-1){
      ifd=0;
      fileInputName="standard input";
    }
    else ifd=open(fileInputName,O_RDONLY); // redundant? maybe...
  }  

  else ifd=open(fileInputName,O_RDONLY);
  errorHandler(ifd,fileInputName, "opening file for read");

  while((rfd=read(ifd,buffer,BUFFER_SIZE))>0){
    readCalls++;
    wfd=write(ofd,buffer,rfd);
    writeCalls++;

    if (wfd>0 && wfd<rfd){    //check for partial write
      write(ofd,buffer+wfd,rfd-wfd);
      writeCalls++;
    } 
  
    errorHandler(wfd,fileInputName,"writing from file");
    
    if(binaryFlag!=1) binaryFlag=isBinary(buffer,rfd);
    byteCount+=wfd;
    memset(buffer,0,BUFFER_SIZE);
  }

  //if reach here, either finish read/writing or ERROR occured!
  errorHandler(rfd,fileInputName,"reading from file");

  /*----------Writing useful logistics to Stderr----------*/
  /*---------This could be a function of its own---------*/
  char bWarning[40]="";
  if (binaryFlag==1) strcpy(bWarning,"\033[0;33m  **BINARY FILE**\033[1;32m");
  sprintf(buffer,"\033[1;32m\n<%s>%s\nNumber of bytes Written:%d\nNumber of read calls:%d\nNumber of write calls:%d\n\n\033[0m",fileInputName,bWarning,byteCount,readCalls,writeCalls);
  write(2,buffer,bufferLength(buffer));
  if (ifd!=0) close(ifd); //Close everything except stdin
}


int main(int argc, char**argv){
  int opt, argIt=1;   //operator iterator. Argument iterator.
  int fd; 
  int writeMode=0;    // 0=>stdin 1=>file
  char buf[BUFFER_SIZE];

  //-----------Option Parsing-------------------//
  while ((opt = getopt (argc, argv,"o")) != -1)
    switch(opt){
      case 'o':
        printf("Output set to file: %s\n",argv[optind]);
        fd=open(argv[optind],O_WRONLY|O_CREAT|O_TRUNC,0666);
        errorHandler(fd,argv[optind],"opening output file for write");
        if (fd>0) {argIt=3; writeMode=1;};
    }

  //-------Argument handling--------//
  //no arguments case, act as if '-' get entered
  if (argc==1) argHandler("-",1,buf);

  // output file specified, no arguments
   else if (writeMode==1 &&  argc==3) argHandler("-",fd,buf);

  // else just parse through every argument and deal with properly.
  else while(argIt!=(argc)){ 
      if (writeMode==0) fd=1; //set to (1:stdout)
      argHandler(argv[argIt],fd,buf);
      argIt++;
    }
  close(fd); // Close the output file descriptor
  exit(0); // end peacefully :)
}
