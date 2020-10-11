#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

extern int errno;
// Problem 3 - Recursive filesystem statistics checker
// In collaboration :
//    Thodori Kapouranis
//    Philip Blumin
//    Brian Doan 
//
// ECE357 : Computer Operating Systems
// Professor Hakner

// Holds all system information desired for report at end
struct fileData{
  
  // file type counts
  int fifoCount;
  int chrCount;
  int dirCount;
  int blkCount;
  int regCount;
  int lnkCount;
  int sockCount;

  // for S_ISREG files only
  long long  int fileSize;
  long long  int allocatedBlocks;

  // any file other than dir that has >1 link
  int multiLink; 

  // symlink does not lead to anywhere valid
  int invalidLink;

  //problematic names
  int badName;
};

// Holds all information critical for the search algorithm
struct nodes{
  char toVisit[200000][4096];  // directory addresses left to search
  
  // Array for holding inode #'s of inodes whose st_link>1 
  // will be linearly searched through.
  int nodesWithMulti[10000000]; 

  int visitsLeft;            // size of toVisit[]
  int volume;                // Only search inodes of the argument's volume
};

//-------------FUNCTION DECLARATIONS------------//
// Check if a name is valid to by typed by regular keyboard in a terminal
// return 1 if valid
// return 0 if invalid
int checkName(char* fileName){
  char buffer[4096];
  strcpy(buffer, fileName);
  int i;
  for(i = 0; i < strlen(buffer); i++) {
    if ( ! (isprint(buffer[i]) || isspace(buffer[i])) ){
      return -1;
    }
  }
  return 0;
}

// Add a directory to our "To visit" array
void addDirectory(struct nodes *nodes, char* addr){  
  strcpy( nodes->toVisit[nodes->visitsLeft], addr);
  nodes->visitsLeft++;
}

// Check for broken symlinks
void checkLink(char* addr, struct fileData *data){
  if(access(addr, F_OK) == -1) data->invalidLink++;
}


// return 0 on new inode
// return -1 on duplicate
int handleMultipleLinks(struct stat ino, struct fileData *data, struct nodes *nodes){
  int dontCount=0;
  int i;
  
  // Check if this file has been viewed before
  for (i=0; i<data->multiLink; i++){
    if (nodes->nodesWithMulti[i] == ino.st_ino){
      dontCount=1;
      break;
    }
  }

  // if it's a unique file, count it and keep its inode # in memory
  if (!dontCount){ 
    nodes->nodesWithMulti[data->multiLink] = ino.st_ino;
    data->multiLink++;
    return 0;
  }

  else return -1;
}

// 0 on successfully reporting the inode information
// -1 on duplicate inode, not reported.
int handleInode(struct stat ino, struct fileData *data, struct nodes *nodes, char* addr){

  if( ((ino.st_mode & S_IFMT) != S_IFDIR) && ino.st_nlink>1){
    if (handleMultipleLinks(ino, data, nodes)==-1) return -1;
  }
  
  switch (ino.st_mode & S_IFMT) {
        // These inode types dont need any special handling.
        case S_IFBLK:  data->blkCount++;                   break;
        case S_IFCHR:  data->chrCount++;                   break;
        case S_IFSOCK: data->sockCount++;                  break;
        case S_IFIFO:  data->fifoCount++;                  break;

        case S_IFDIR: 
          data->dirCount++;
          addDirectory(nodes,addr);
          break;
        
        case S_IFLNK:
          data->lnkCount++;
          checkLink(addr, data);         
          break;
        
        case S_IFREG:
          data->regCount++;
          data->fileSize += ino.st_size;
          data->allocatedBlocks += ino.st_blocks;
          break;
        
        default: 
          fprintf(stderr, "Unknown inode type encountered");
          break;
  }
  return 0;
};

void printData(struct fileData data){
  printf("\n\
  ---------FILE TYPE COUNT---------\n\
  Named pipe (FIFO):\t\t%d \n\
  Character Device:\t\t%d \n\
  Directory:\t\t\t%d \n\
  Block Device:\t\t\t%d \n\
  Regular File:\t\t\t%d \n\
  Symlink:\t\t\t%d \n\
  Network/IPC socket:\t\t%d \n\
  Bad Name count:\t\t%d \n\n\
  ",data.fifoCount, data.chrCount, data.dirCount, data.blkCount, data.regCount, data.lnkCount, data.sockCount, data.badName);

  printf("-----------REGULAR FILES----------\n\
  Size of files (in bytes):\t%lld \n\
  Number of blocks:\t\t%lld \n\n\
  ", data.fileSize, data.allocatedBlocks/8);
  // st_blocks returns number of 512 byte blocks
  // we are assuming disk block size is 4k, so we divide it by 8.

  printf("-----------LINK STATS-------------\n\
  Files with multiple links:\t%d \n\
  Invalid symlinks:\t\t%d \n\n\
  ", data.multiLink, data.invalidLink);
}

//-----------------MAIN----------------//
int main(int argc, char**argv){
  struct fileData data = {0,0,0,0,0,0,0,0,0,0,0,0};

  struct stat sb;
  struct nodes *nodes = malloc(sizeof(struct nodes));
  
  if(argc==1){
    fprintf(stderr, "ERROR: No arguments given!\n");
    exit(-1);
  };
  
  // Place argument directory in our address queue
  strcpy(nodes->toVisit[1], argv[1]);
  nodes->visitsLeft=1;
  data.dirCount++;

  // Grab the device number we will search through
  if(lstat(argv[1], &sb)==-1){
    fprintf(stderr, "\nCannot lstat %s\n", argv[1]);
    perror("ERROR");

  };
  nodes->volume = sb.st_dev;

  // Journey through every directory in our "directories left to visit" array
  for (nodes->visitsLeft; nodes->visitsLeft!=0; nodes->visitsLeft--){
    
    char cur_dir[4096];
    strcpy( cur_dir, nodes->toVisit[nodes->visitsLeft] );

    char child_addr[4096];
    DIR *dir;             //directory pointer
    struct dirent *dp;    //directory entry struct

    // Error handling of invalid directories
    if ((dir = opendir(cur_dir)) == NULL){

      if(errno == 13){
        fprintf(stderr, "\nCannot open %s\n",cur_dir);
        perror("ERROR");

      }

      else {
        exit(-1);
      }
    }

    // Iterate through every directory entry
    else {

      while ((dp = readdir(dir)) != NULL){

        if( strcmp(dp->d_name,".")!=0 && strcmp(dp->d_name,"..")!=0) {
          memset(child_addr,0,4096);
          
        // strcat the child address together
          strcat(child_addr,cur_dir);  
          strcat(child_addr,"/");      
          strcat(child_addr,dp->d_name);
          
          if(checkName(dp->d_name) == -1){
            data.badName++;
          }
          
          if(lstat(child_addr, &sb) == -1){
            fprintf(stderr, "\nCannot lstat %s\n", child_addr);
            perror("ERROR");
          }

          // If this child of the correct volume, throw it in our handler
          else if(sb.st_dev == nodes->volume){
            handleInode(sb, &data, nodes, child_addr);
          }
        }
      }
    }
    closedir(dir);
  }
  printData(data);
  exit(0);
}
