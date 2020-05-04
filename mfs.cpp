#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fstream>
#include <iostream>
#include <vector>

#define WHITESPACE " \t\n"      

#define MAX_COMMAND_SIZE 255    

#define MAX_NUM_ARGUMENTS 5   

using namespace std;

struct __attribute__((__packed__)) DirectoryEntry{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

uint16_t BPB_BytesPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATs;
uint32_t BPB_FATSz32;
uint32_t current_directory;

void init(FILE *img){
  fseek(img, 11, SEEK_SET);
  fread(&BPB_BytesPerSec, 2, 1, img);
  fseek(img, 13, SEEK_SET);
  fread(&BPB_SecPerClus, 1, 1, img);
  fseek(img, 14, SEEK_SET);
  fread(&BPB_RsvdSecCnt, 2, 1, img);
  fseek(img, 16, SEEK_SET);
  fread(&BPB_NumFATs, 1, 1, img);
  fseek(img, 36, SEEK_SET);
  fread(&BPB_FATSz32, 4, 1, img);
  uint32_t index = (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);
  current_directory = index;
  for(int i = 0; i < 16; i++){
    fseek(img, index, SEEK_SET);
    fread(&dir[i], 32, 1, img);
    index+=32;
  }
}

int LBAToOffset(int32_t sector){
  return ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

int16_t NextLB(uint32_t sector, FILE * fp){
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector*4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

bool cmpstr(char * IMG_Name, char * input){
  char temp[12];
  strcpy(temp, input);
  char expanded_name[12];
  memset( expanded_name, ' ', 12 );

  char *token = strtok( temp, "." );

  strncpy( expanded_name, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(expanded_name+8), token, strlen(token ) );
  }

  expanded_name[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    expanded_name[i] = toupper( expanded_name[i] );
  }

  if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
  {
    return true;
  }else{
    return false;
  }
}

int check_for_file(char file[12]){
  for(int i = 0; i < 16; i++){
    if(cmpstr(dir[i].DIR_Name, file)){
      return i;
    }
  }
  return -1;
}

void ls(uint32_t index, FILE * img){
  char temp[12];
  for(int i = 0; i < 16; i++){
    if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 || dir[i].DIR_Attr == 0x30) && (unsigned char)dir[i].DIR_Name[0] != 0xe5){
      memset(temp, 0, 12);
      memcpy(temp, dir[i].DIR_Name, 11);
      temp[11] = '\0';
      printf("%s\n", temp);
    }
  }
}


void to_upper(char t[12]){
  for(int i = 0; i < strlen(t); i++){
    t[i] = toupper(t[i]);
  }
  for(int i = strlen(t); i < 12; i++){
    strcat(t, " ");
  }
  t[11] = '\0';
}

bool check_if_dir(char * token, uint32_t temp_cd, FILE * img){
  char dname[12]; 
  char *t = strtok(token, "/");
  while(t != NULL){
    char * s = (char*)malloc(strlen(t));
    strcpy(s,t);
    to_upper(s);
    for(int i = 0; i < 16; i++){
      memset(dname, 0, 12);
      memcpy(dname, dir[i].DIR_Name, 11);
      if(!strcmp(s, dname) && (dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x30)){
        int add;
        if(dir[i].DIR_FirstClusterLow == 0){
          add = LBAToOffset(2);
        }else{
          add = LBAToOffset(dir[i].DIR_FirstClusterLow);
        }
        for(int i = 0; i < 16; i++){
          fseek(img, add, SEEK_SET);
          fread(&dir[i], 32, 1, img);
          add += 32;
        }
        break;
      }else if(i == 15){
        return false;
      }
    }
    t = strtok(NULL, "/");
  }
  return true;
}



int main()
{
  FILE * img = NULL;
  string filename;
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  while( 1 )
  {
    printf ("mfs> ");

    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                              
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    char *working_root = working_str;

    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }
    if(token[0] != NULL){
      if(!strcmp(token[0], "open")){
        if(token[1] == NULL){
          cout << "No Filename Specified... ex. open file.img" << endl;
        }else{
          if(img){
            cout << "Error: File system image already open." << endl;
          }else{
            img = fopen(token[1], "r");
            filename = token[1];
            if(img){
              init(img);
              cout << filename << " opened..." << endl;
            }else{
              cout << "Error: File system image not found." << endl;
            }
          }
        }
      }else if(!strcmp(token[0], "close")){
        if(img){
          fclose(img);
          img = NULL;
          cout << filename << " closed..." << endl;
        }else{
          cout << "Error: File system not open" << endl;
        }
      }else if(img == NULL){
        cout << "Error: File system image must be opened first." << endl;
      }else if(!strcmp(token[0], "info")){
        if(img == NULL){
          cout << "Error: File system image must be opened first." << endl;
        }else{
          printf("BPB_BytesPerSec: %d\n", BPB_BytesPerSec);
          printf("BPB_BytesPerSec: 0x%x\n", BPB_BytesPerSec);
          printf("BPB_SecPerClus: %d\n", BPB_SecPerClus);
          printf("BPB_SecPerClus: 0x%x\n", BPB_SecPerClus);
          printf("BPB_RsvdSecCnt: %d\n", BPB_RsvdSecCnt);
          printf("BPB_RsvdSecCnt: 0x%x\n", BPB_RsvdSecCnt);
          printf("BPB_NumFATs: %d\n", BPB_NumFATs);
          printf("BPB_NumFATs: 0x%x\n", BPB_NumFATs);
          printf("BPB_FATSz32: %d\n", BPB_FATSz32);
          printf("BPB_FATSz32: 0x%x\n", BPB_FATSz32);
        }
      }else if(!strcmp(token[0], "stat")){
        if(token[1] == NULL){
          cout << "No File or Directory Specified... ex. stat file.img" << endl;
        }else{
          for(int i = 0; i < 16; i++){
            if(cmpstr(dir[i].DIR_Name, token[1])){
              printf("Attributes      Size            Starting Cluster Number\n");
              printf("%-16d%-16d%-16d\n", dir[i].DIR_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
              break;
            }else if(i == 15){
              cout << "Error: File not found" << endl;
            }
          }
        }
      }else if(!strcmp(token[0], "ls")){
        if(token[1] == NULL){
          ls(current_directory, img);
        }else{
          char temp[12];
          DirectoryEntry d;
          int ls_add;
          char * s = (char*)malloc(strlen(token[1]));
          strcpy(s,token[1]);
          to_upper(s);
          for(int j = 0; j < 16; j++){
            memset(temp, 0, 12);
            memcpy(temp, dir[j].DIR_Name, 11);
            if(!strcmp(temp, s)){
              if(dir[j].DIR_FirstClusterLow == 0){
                ls_add = LBAToOffset(2);
              }else{
                ls_add = LBAToOffset(dir[j].DIR_FirstClusterLow);
              }
              for(int i = 0; i < 16; i++){
                if((d.DIR_Attr == 0x01 || d.DIR_Attr == 0x10 || d.DIR_Attr == 0x20 || d.DIR_Attr == 0x30) && (unsigned char)d.DIR_Name[0] != 0xe5){
                  memset(temp, 0, 12);
                  memcpy(temp, d.DIR_Name, 11);
                  printf("%s\n", temp);
                }
                fseek(img, ls_add, SEEK_SET);
                fread(&d, 32, 1, img);
                ls_add += 32;
              }
              break;
            }else if(j == 15){
              cout << token[1] << " is not a directory" << endl;
            }
          }
        }
      }else if(!strcmp(token[0], "get")){
        if(token[1] == NULL){
          cout << "No File or Directory Specified... ex. get bar.txt" << endl;
        }else{
          bool found = false;
          FILE *newfile;
          char ch;
          for(int i = 0; i < 16; i++){
            if(cmpstr(dir[i].DIR_Name, token[1]) && dir[i].DIR_Attr != 0x10){
              found = true;
              newfile = fopen(token[1], "w");
              int sector = dir[i].DIR_FirstClusterLow;
              int address = LBAToOffset(sector);
              for(int j = 0; j < dir[i].DIR_FileSize; j++){
                if(j % 512 == 0 && j != 0){
                  sector = NextLB(sector, img);
                  address = LBAToOffset(sector);
                }
                fseek(img, address++, SEEK_SET);
                fread(&ch, 1, 1, img);
                fputc(ch, newfile);
              }
              fclose(newfile);
              break;
            }
          }
          if(!found){
            cout << "Error : File Not Found" << endl;
          }
        }
      }else if(!strcmp(token[0], "cd")){
        if(token[1] == NULL){
          cout << "No File or Directory Specified... ex. cd foldera" << endl;
        }else{
          uint32_t temp_cd;
          temp_cd = current_directory;
          if(!check_if_dir(token[1], temp_cd, img)){
            cout << token[1] << " is not a Directory..." << endl;
          }
        }
      }else if(!strcmp(token[0], "read")){
        if(token[1] == NULL || token[2] == NULL || token[3] == NULL){
          cout << "Invalid Command... ex. read bar.txt 0 2" << endl;
        }else{
          bool found = false;
          int ch;
          for(int i = 0; i < 16; i++){
            if(cmpstr(dir[i].DIR_Name, token[1]) && dir[i].DIR_Attr != 0x10){
              found = true;
              int sector = dir[i].DIR_FirstClusterLow;
              int address = LBAToOffset(sector);
              for(int j = 0; j < dir[i].DIR_FileSize; j++){
                if(j % 512 == 0 && j != 0){
                  sector = NextLB(sector, img);
                  address = LBAToOffset(sector);
                }
                fseek(img, address++, SEEK_SET);
                fread(&ch, 1, 1, img);
                if(j >= atoi(token[2]) && j < (atoi(token[3]) + atoi(token[2]))){
                  printf("0x%X ", ch);
                }
              }
              printf("\n");
              break;
            }
          }
          if(!found){
            cout << "Error : File Not Found" << endl;
          }
        }
      }
    }
    free( working_root );
  }
  return 0;
}
