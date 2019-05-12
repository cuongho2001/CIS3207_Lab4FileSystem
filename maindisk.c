// AUTHOR: CUONG HO
// CIS 3207 - SPRING 2019
// LAB 4: FILE SYSTEM
// JOHN FIORE


// NOTE: This file system uses the FAT approach.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>



#define BLOCKS 4096         // Number of blocks
#define BLOCK_SIZE 512      // Size of each block
#define MAX_ENTRIES 8       // Max number of entries per block


// Indicate the status of the file entry
#define FREE 0
#define BUSY 1


// Entry in the file allocation table (FAT)
typedef struct FILE_ALLOCATION_ENTRY{
    short status;
    short next;
} fileEntry;


// FAT structure - 4096 number of fileEntry in the FAT
typedef struct FILE_ALLOCATION_TABLE{
    fileEntry file[BLOCKS];
} FAT;




// Entry in directory table structure - 64 byes each
typedef struct ENTRY{
    char fileName[37]; // 37 bytes (36 filename + terminator)
    char ext[4]; // 4 bytes (3 extension + terminator)
    char folder; // 1 byte
    unsigned short fileSize; // 2 bytes
    char accessTime[9]; // 9 bytes (8 time + terminator)
    char accessDate[9]; // 9 bytes; (8 time + terminator)
    unsigned short startingIndex; // 2 bytes

} Entry;

// Directory Entry Table. Each can hold up to 8 entries
typedef struct DIRECTORY_TABLE{
    Entry entry[MAX_ENTRIES];
} directory;




// Data block
typedef struct DATA_BLOCK{
    char sect[BLOCK_SIZE];
} datablock;

// DATA - all data blocks
typedef struct DATA_BLOCKS{
    datablock blocks[BLOCKS];
} DATA;




//function prototypes
void create_fs(FAT *fat, Entry *root);
void editFileName(char * oldFilename, char * newFilename);
void editExt(char * oldExtension, char * newExtension);
void editFileSize(unsigned short * oldSize, unsigned short newSize);
void editIndex(unsigned short *oldIndex, unsigned short newIndex);
void editFolder(char *oldFolder, char newFolder);
void getDate(char *dateSt);
void getTime(char *timeSt);
short findFreeBlock(FAT * fat);
short findFreeEntry(directory *dir);
int findFileOffset(FAT * fat, DATA * data, directory * dir, int file);
int findFileEntry(directory * dir, char * fileName, char * ext);
void createFile(FAT * fat, DATA * data, char * fileName, char * ext);
void createDir(FAT * fat, DATA * data, char * fileName);
void deleteFile(FAT * fat, DATA * data, char * fileName, char * ext);
void readFile(FAT * fat, DATA * data, char * fileName, char * ext);
void writeFile(FAT * fat, DATA * data, char * fileName, char * ext, char * buf);


// Directory stack
int stack[BLOCKS];
int top = 0;

// Directory stack functions - navigating directory entries
int isEmpty();
int isFull();
int peek();
int pop();
void push(int dirBlock);
int cd(DATA * data, char * dirName);



//MAIN FUNCTION
int main(int argc, char ** args){

    FAT *fat;
    DATA *data;
    Entry *root;

    stack[top] = 0;

    // Create a drive for FAT, DATA, and root directory
    int fd = open("Drive", O_RDWR|O_CREAT, 0660);

    ftruncate(fd, sizeof(FAT) + sizeof(Entry) + BLOCKS * BLOCK_SIZE);
    fat = mmap(NULL, sizeof(FAT), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    data = mmap(NULL, sizeof(DATA), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT));
    root = mmap(NULL, sizeof(Entry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, sizeof(FAT) + sizeof(DATA));


    create_fs(fat, root);

    // Testing file system
    createDir(fat, data, "Dir1");                //1. create a directory below the root of your file system
    if (cd(data, "Dir1")){                       //change directory to created directory
        createFile(fat, data, "testing", "txt");      //2. create a file in that directory

        /* //3. write a specified formatted data set into the file */
        char buff[1500];                                                //writing 1500 (1499 char + null terminator) to demonstrate linking data blocks for a file
        memset(buff, '\0', sizeof(buff));
        int i;
        for (i = 0; i < 60; i++){
            buff[i] = 'a';
        }
        writeFile(fat, data, "testing", "txt", " Hello this is the first testing file\n");                   //writing the buffer to the file
        writeFile(fat, data, "testing", "txt", " 2nd chunk of text to demonstrate linking data blocks"); //writing random stuff to the file at the end
        readFile(fat, data, "testing", "txt");                         //4. read back and compare the data that have been written to the file

        deleteFile(fat, data, "testing", "txt");
        createFile(fat, data, "test2", "txt");
        writeFile(fat, data, "test2", "txt", " This is the 2nd test file.");
        readFile(fat, data, "test2", "txt");
        deleteFile(fat, data, "test2", "txt");

    } else{                                                          //if cd fails
        printf("couldn't find the directory!\n");
    }
    close(fd); //close the file descriptor
    exit(0);
}



void create_fs(FAT *fat, Entry *root){

   printf(" Initializing file system...\n");
   fat->file[0].status = BUSY;
   fat->file[0].next = -1;

   editFileSize(&(root->fileSize), 0);
   editIndex(&(root->startingIndex), 0);
   editFolder(&(root->folder), 1);
   getTime(root->accessTime);
   getDate(root->accessDate);
   editExt(root->ext, "   ");
   editFileName(root->fileName, "root");

}

// EDIT METADATA
void editFileSize(unsigned short *oldSize, unsigned short newSize){
    *oldSize = newSize;
}
void editIndex(unsigned short *oldIndex, unsigned short newIndex){
    *oldIndex = newIndex;
}
void editFolder(char *oldFolder, char newFolder){
    *oldFolder = newFolder;
}
void editExt(char * oldExtension, char * newExtension){
    strcpy(oldExtension, newExtension);
}
void editFileName(char * oldFilename, char * newFilename){
    strcpy(oldFilename, newFilename);
}


void getTime(char *timeSt){
    time_t current_time;
    struct tm * time_info;

    time(&current_time);
    time_info = localtime(&current_time);
    char time[9];
    strftime(time, sizeof(time), "%X", time_info);
    strcpy(timeSt, time);
}


void getDate(char *dateSt){
    time_t current_time;
    struct tm * time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    char date[9];

    strftime(date, sizeof(date), "%x", time_info);
    strcpy(dateSt, date);
}



short findFreeBlock(FAT * fat){
    short i;
    for (i = 1; i < BLOCKS; i++){
        if (fat->file[i].status == FREE){
            return i;
        }
    }
    return -1;
}


short findFreeEntry(directory *dir){
    short i;
    for (i = 0; i < MAX_ENTRIES; i++){

        if (dir->entry[i].startingIndex == 0){
            return i;
        }
    }
    return -1;
}

int findFileOffset(FAT * fat, DATA * data, directory * dir, int file){

    int filesize = dir->entry[file].fileSize;
    int start = dir->entry[file].startingIndex;
    int count = 0;

    while (fat->file[start].next != -1) {
        start = fat->file[start].next;
        count++;
    }

    return filesize - (BLOCK_SIZE * count);

}


int findFileEntry(directory * dir, char * fileName, char * ext){

    int i;
    for (i = 0; i < MAX_ENTRIES; i++){
        if ( ((strcmp(dir->entry[i].fileName, fileName)) == 0) && ((strcmp(dir->entry[i].ext, ext)) == 0) &&
                (dir->entry[i].folder == 0) && (dir->entry[i].startingIndex != 0)){
            return i;
        }
    }
    return -1;
}


void createFile(FAT * fat, DATA * data, char * fileName, char * ext){

    printf(" Creating file %s...\n", fileName);
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    short freeBlock = findFreeBlock(fat);
    fat->file[freeBlock].status = BUSY;
    fat->file[freeBlock].next = -1;


    short freeEntry = findFreeEntry(dir);

    printf(" FILE META DATA:");

    editFileName(dir->entry[freeEntry].fileName, fileName);
    printf(" File Name: %s \n", dir->entry[freeEntry].fileName);

    editExt(dir->entry[freeEntry].ext, ext);
    printf(" File Extension: %s \n", dir->entry[freeEntry].ext);

    editFileSize(&dir->entry[freeEntry].fileSize, 0);
    printf(" File Size: %d\n", dir->entry[freeEntry].fileSize);

    editIndex(&dir->entry[freeEntry].startingIndex, freeBlock);
    printf(" Starting index: %d\n", dir->entry[freeEntry].startingIndex);

    editFolder(&dir->entry[freeEntry].folder, 0);                           // file = 0
    getTime(dir->entry[freeEntry].accessTime);
    printf(" Access Time: %s\n", dir->entry[freeEntry].accessTime);

    getDate(dir->entry[freeEntry].accessDate);
    printf(" Access Date: %s\n", dir->entry[freeEntry].accessDate);


    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}


void createDir(FAT * fat, DATA * data, char * fileName){

    printf(" Creating directory %s...\n", fileName);
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));

    short freeBlock = findFreeBlock(fat);
    fat->file[freeBlock].status = BUSY;
    fat->file[freeBlock].next = -1;

    short freeEntry = findFreeEntry(dir);

    editFileSize(&dir->entry[freeEntry].fileSize, 0);
    editIndex(&dir->entry[freeEntry].startingIndex, freeBlock);
    editFolder(&dir->entry[freeEntry].folder, 1);                           // folder = 1
    getTime(dir->entry[freeEntry].accessTime);
    getDate(dir->entry[freeEntry].accessDate);
    editExt(dir->entry[freeEntry].ext, "   ");
    editFileName(dir->entry[freeEntry].fileName, fileName);
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}



void deleteFile(FAT * fat, DATA * data, char * fileName, char * ext){

    printf(" Deleting directory %s...\n", fileName);
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));


    int i = findFileEntry(dir, fileName, ext);
    if (i == -1){
        printf(" ERROR: File not found \n");
        return;

    } else {
        int block = dir->entry[i].startingIndex;


        while (fat->file[block].next != -1){
            int temp = fat->file[block].next;
            fat->file[block].status = FREE;
            fat->file[block].next = -1;
            block = temp;
        }
        fat->file[block].status = FREE;
        fat->file[block].next = -1;

        editFileSize(&dir->entry[i].fileSize, 0);
        editIndex(&dir->entry[i].startingIndex, 0);
    }

    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}


void readFile(FAT * fat, DATA * data, char * fileName, char * ext){

    printf(" Reading file %s...\n", fileName);
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));


    int file = findFileEntry(dir, fileName, ext);
    if (file == -1){
        printf(" ERROR: File not found \n");

    } else {
        char buf[dir->entry[file].fileSize + 1];
        memset(buf, '\0', sizeof(buf));

        int startBlock = dir->entry[file].startingIndex;
        char temp[BLOCK_SIZE + 1];


        while(fat->file[startBlock].next != -1){
            memset(temp, '\0', sizeof(temp));
            memcpy(temp, data->blocks[startBlock].sect, BLOCK_SIZE);
            strcat(buf, temp);
            startBlock = fat->file[startBlock].next;
        }

        int offset = findFileOffset(fat, data, dir, file);
        memset(temp, '\0', sizeof(temp));
        memcpy(temp, data->blocks[startBlock].sect, offset);
        strcat(buf, temp);


        printf("%s\n", buf);
    }
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}


// WRITE DATA TO FILE
void writeFile(FAT * fat, DATA * data, char * fileName, char * ext, char * buf){

    printf(" Writing to file %s...\n", fileName);
    directory *dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));


    int file = findFileEntry(dir, fileName, ext);

    if (file == -1){

        printf(" ERROR: File not found. n");
        return;

    } else{

        int startingBlock = dir->entry[file].startingIndex;
        while (fat->file[startingBlock].next != -1){

            startingBlock = fat->file[startingBlock].next;
        }

        int bufLength = strlen(buf);
        while (bufLength > 0){
            printf(" Starting to write at block: %d\n", startingBlock);
            printf(" Buffer Length: %d\n", bufLength);

            int offset = findFileOffset(fat, data, dir, file);
            printf(" Offset: %d\n", offset);

            int n = (bufLength < (BLOCK_SIZE - offset)) ? bufLength : BLOCK_SIZE - offset;
            printf(" N: %d\n", n);
            strncpy(data->blocks[startingBlock].sect + offset, buf + (strlen(buf) - bufLength), n);
            editFileSize(&dir->entry[file].fileSize, n + dir->entry[file].fileSize);
            printf(" Filesize: %d\n", dir->entry[file].fileSize);
            bufLength = bufLength - n;

            if ((offset + n) == BLOCK_SIZE){

                int freeBlock = findFreeBlock(fat);
                fat->file[startingBlock].next = freeBlock;
                fat->file[freeBlock].status = BUSY;
                fat->file[freeBlock].next = -1;
                startingBlock = freeBlock;
            }
        }
    }

    getTime(dir->entry[file].accessTime);
    getDate(dir->entry[file].accessDate);
    memcpy(&(data->blocks[peek()]), dir, sizeof(*dir));
}



int peek(){
   return stack[top];
}

int pop(){
    int data;

    if (top == 0) {
        return 0;
    } else {
        data = stack[top];
        top = top - 1;
        return data;
    }

}

void push(int dirBlock){

    if ( top == BLOCKS) {
        printf(" Stack is full! \n");

    } else {
        top = top + 1;
        stack[top] = dirBlock;
    };


}


int cd(DATA * data, char * dirName){

    if (strcmp(dirName, "..") == 0){
        pop();
        return 1;
    }


    directory * dir = malloc(sizeof(*dir));
    memcpy(dir, &(data->blocks[peek()]), sizeof(*dir));


    for (int i = 0; i < MAX_ENTRIES; i++){
        if ((strcmp(dirName, dir->entry[i].fileName) == 0) && (dir->entry[i].folder == 1)){
            push(dir->entry[i].startingIndex);
            return 1;
        }
    }
    return 0;
}


