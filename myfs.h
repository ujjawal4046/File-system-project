#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <stdio.h>
#include <climits>
#include <algorithm>
#include <queue>
#include <errno.h>
#include <sys/time.h>
#include <bitset>
#include <cmath>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
using namespace std;

#define DIRECTORY 0
#define REGULAR 1
#define MAX_INODE_POSSIBLE 256
#define MAX_DATA_BLOCK_POSSIBLE 9744
#define MAX_FILE_SIZE ((8+(BLOCK_SIZE/sizeof(int))+(BLOCK_SIZE/sizeof(int))*(BLOCK_SIZE/sizeof(int)))*BLOCK_SIZE)
#define BLOCK_SIZE 256
#define INODE_SIZE sizeof(struct inode_)
#define EMPTY -1
#define MAX_FD 12
#define ROOT "myroot"

extern struct file_sys *fs ;
extern struct inode_ *curr_dir;
extern int shmid;
extern struct file_des *fd_list;
struct time{
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
};
struct sup_bck{
	unsigned int file_system_size;
	unsigned int max_inode;
	unsigned int used_inode;
	unsigned int max_data_block;
	unsigned int used_data_block;
	bitset<MAX_INODE_POSSIBLE> map_inode;
	bitset<MAX_DATA_BLOCK_POSSIBLE> map_data_block;
	
};

struct inode_{
	int type;
	unsigned int file_size;
	struct time last_modified;
	struct time last_read;
	struct time last_inode_modified;
	bitset<9> mode;
	unsigned int dir_ptr[8];
	unsigned int  indir_ptr;
	unsigned int double_indir_ptr;
};

struct directory{
	char dir_name[28];
	int inode_no;
};
struct directory_bck{
	struct directory dir_list[8];
};
struct data_bck{
	char data[256];
};

struct file_sys{
	struct sup_bck super_block;
	struct inode_ *inode_list;
	struct data_bck *data_block;

};
struct file_des
{
	unsigned int offset = 0;
	char mode;
	int inode_no = EMPTY;
};



int create_myfs (int size);
int copy_pc2myfs(char* source,char* dest);
int copy_myfs2pc(char* source,char* dest);
int rm_myfs(char* filename);
int showfile_myfs(char *filename);
int ls_myfs();
int mkdir_myfs(char* dirname);
int chdir_myfs(char* dirname);
int rmdir_myfs(char* dirname);
int open_myfs(char* filename,char mode);
int close_myfs(int fd);
int read_myfs(int fd,int nbytes,char* buff);
int write_myfs(int fd,int nbytes,char* buff);
int eof_myfs(int fd);
int dump_myfs(char* dumpfile);
int restore_myfs(char* dumpfile);
int status_myfs();
int chmod_myfs(char* name,int mode);



void add_file(char* name,int inode_no,struct inode_*& parent);
char* extract_filename(char* path,char* dest);
int get_free_data_block();
void print_data_block(int n);
void init_inode(struct inode_ *inode);
int search_directory_block(struct directory_bck* cd,char* filename);
struct inode_ *search_directory(struct inode_ *parent,char* filename);
bool namei(char* path,struct inode_*& here);
void add_file(char* name,int inode_no,struct inode_*& parent);
void add_file(char* name,int inode_no,struct inode_*& parent);
void print_directory(struct directory dir);
void decrement_dir_count(struct inode_ *parent,int inode_no);
int createfile_myfs(char* name);
void cleanup();
