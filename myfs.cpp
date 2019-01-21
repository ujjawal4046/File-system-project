#include "myfs.h"
//copy_myfs2pc() double_indir_ptr set last block properly
//while loop in read and write

struct file_sys *fs = NULL;
struct inode_ *curr_dir;
int shmid = 0;
struct file_des *fd_list;
pthread_mutex_t  pmutex;
pthread_mutexattr_t attrmutex;



void reset_map()
{
		for(int i = 0;i<fs->super_block.max_inode;i++)
		fs->super_block.map_inode[i] = 0;
		for(int i = 0;i<fs->super_block.max_data_block;i++)
		fs->super_block.map_data_block[i] = 0;
}
bool start_with(char* str1,char* str2)
{
	int len1=strlen(str1);
	int len2=strlen(str2);
	if(len1<len2)return false;
	char finals[len2+1];
	strncpy(finals,str1,len2);
	finals[len2] = '\0';
	if(strcmp(finals,str2)==0) return true;
	return false;
}
void get_time(struct time *ctime)
{
	time_t now; 
	time(&now);
	
	struct tm ltime;
	localtime_r(&now,&ltime);
	
	
	ctime->year = (unsigned char)(ltime.tm_year);
	ctime->month = (unsigned char)ltime.tm_mon;
	ctime->day = (unsigned char)ltime.tm_mday;
	ctime->hour = (unsigned char)ltime.tm_hour;
	ctime->minute  = (unsigned char)ltime.tm_min;
	ctime->second = (unsigned char)ltime.tm_sec;
	
	
}

void print_time(struct time& ctime)
{
		printf("%d/%d/%d %d:%d:%d",ctime.day,ctime.month,ctime.year+1900,ctime.hour,ctime.minute,ctime.second);
}
void set_permission(struct inode_*& inode,int perms)
{
	unsigned long  o_perm = perms%10;
	unsigned long g_perm = (perms/10)%10;
	unsigned  long u_perm = (perms/100)%10;
	bitset<3> u(u_perm);
	bitset<3> g(g_perm);
	bitset<3> o(o_perm);
	
	for(int i = 0;i<9;i++)
		if(i < 3)
			inode->mode[i] = u[i];
		else if(i < 6)
			inode->mode[i] = g[i%3];
		else
			inode->mode[i] = o[i%3];
	
}
int get_free_fd()
{
		for(int i = 0;i<MAX_FD;i++)
			if(fd_list[i].inode_no == EMPTY)
				return i;
		return -1;
}
int get_free_inode()
{
	
	for(int i = 0;i<fs->super_block.max_inode;i++)
		if(fs->super_block.map_inode[i] == 0){
			
			return i;
		}
	printf("ERROR: NO REMAINING INODE!!\n");
	return -1;
}

int get_free_data_block()
{
		for(int i = 0;i<fs->super_block.max_data_block;i++)
			if(fs->super_block.map_data_block[i] == 0)
				return i;
		printf("ERROR: NO REMAINING DATA BLOCK!!\n");
		return -1;
}

char* extract_filename(char* path,char* dest)
{
	char copy_path[strlen(path)+1];
	strcpy(copy_path,path);
	char* pre = copy_path;
	char* cur = strtok(copy_path,"/");
	while(cur != NULL)
	{
		pre = cur;
		cur=strtok(NULL,"/");
	}
	strcpy(dest,pre);
	return dest;
}
void print_data_block(int n)
{
	char* temp=(char*)(&fs->data_block[n]);
	for(int i=0;i<256;i++)
	{
		printf("%c",temp[i]);
	}
	return;
}
void init_inode(struct inode_ *inode)
{
	for(int i  = 0;i<8;i++)
		*(inode->dir_ptr+i) = EMPTY;
	inode->indir_ptr=EMPTY;
	inode->double_indir_ptr = EMPTY;
	for(int i = 0;i<9;i++)
		inode->mode[i] = 0;
}

int search_directory_block(struct directory_bck* cd,char* filename)
{
	for(int i = 0;i<8;i++)
	{
		if(strcmp((cd->dir_list[i]).dir_name,filename) == 0)
		{	
			//printf("[MATCHED]:: %s %s\n",(cd->dir_list[i]).dir_name,filename);
			return (cd->dir_list[i]).inode_no;
		}
	}
	
	return -1;
}
void decrement_dir_count(struct inode_ *parent,int inode_no)
{
	if(parent->type == DIRECTORY)
	{
		int bck_count = ceil(parent->file_size/(double)BLOCK_SIZE);
		int dir_count = parent->file_size/sizeof(struct directory);
		int count = 0;
		struct directory *here = NULL,*last = NULL;
		for(int i = 0;i<bck_count;i++)
		{
				struct directory_bck *dir_bck = (struct directory_bck*)(fs->data_block+parent->dir_ptr[i]);
				for(int j = 0;j<8;j++)
				{
						if(count == dir_count -1)
						{
								last = &(dir_bck->dir_list[j]);
								break;
						}
						if(dir_bck->dir_list[j].inode_no == inode_no)
							here = &(dir_bck->dir_list[j]);
						
						++count;
				}
		}
		
		if(here != NULL && last != NULL)
		{
			strcpy(here->dir_name,last->dir_name);
			here->inode_no = last->inode_no;
		}
		parent->file_size -= sizeof(struct directory);
	}
	
}
struct inode_ *search_directory(struct inode_ *parent,char* filename)
{
	
	unsigned int size = parent->file_size;
	unsigned int tot_blk = ceil(size/(double)BLOCK_SIZE);
	int found_inode = -1;
	int i;
	for(i = 0;i<tot_blk;i++)
	{
		
			if( (found_inode = search_directory_block((struct directory_bck*)(fs->data_block+(parent->dir_ptr[i])),filename)) >= 0)break;
	}
	
	if(i == tot_blk)
	return NULL;
	return fs->inode_list+found_inode;
}
//RETURNS true if matched exactly else upto matched part
bool namei(char* path,struct inode_*& here)
{
	
	
	struct inode_ *work_inode,*temp; 
	if(start_with(path,ROOT))
	{	
		
		work_inode = fs->inode_list;
	}
	
	else
		work_inode = curr_dir;
	char copy_path[strlen(path)+1];
	strcpy(copy_path,path);
	char* cur = strtok(copy_path,"/");
	
	unsigned int cur_inode = 0;

	while(cur != NULL)
	{
			if(work_inode->type == DIRECTORY)
				temp = search_directory(work_inode,cur);
			cur = strtok(NULL,"/");
			if(temp != NULL)
				work_inode = temp;
			else 
				break;	
	}
	here = work_inode;
	
	if(cur == NULL && temp == NULL)return false;
	
	return true;

}
/**
void change_size(struct inode_*& pred,int change)
{
	pred->file_size += change;
	
	int inode_pred_pred = ((struct directory_bck*)(&(fs->data_block[pred->dir_ptr[0]])))->dir_list[0].inode_no;
	struct inode_* pred_pred ;	
	if(inode_pred_pred != (pred - fs->inode_list)){
		
		pred_pred  = fs->inode_list+inode_pred_pred;
		change_size(pred_pred,change);
	}
	
}
**/
void add_file(char* name,int inode_no,struct inode_*& parent)
{
	if(parent->type == REGULAR)return;
	struct directory_bck *dir_bck = NULL;
	if(parent->file_size%BLOCK_SIZE == 0)
	{
		int new_bck = get_free_data_block();
		fs->super_block.used_data_block++ ;
		parent->dir_ptr[parent->file_size/BLOCK_SIZE]  = new_bck;
		fs->super_block.map_data_block[new_bck] = 1;
		dir_bck = (struct directory_bck *)&(fs->data_block[new_bck]);	
	}
	else
	{
			dir_bck = (struct directory_bck *)&(fs->data_block[parent->dir_ptr[parent->file_size/BLOCK_SIZE]]);
	}
	struct directory *file  = &(dir_bck->dir_list[(parent->file_size%BLOCK_SIZE)/32]);
	strncpy(file->dir_name,name,strlen(name)+1);
	file->dir_name[28] = '\0';
	file->inode_no = inode_no;
	
}
int copy_pc2myfs(char* source,char* dest)
{
	struct inode_ *parent;
	if(namei(dest,parent)){
		printf("DEBUG file already exists\n");
		return -1;
	}
	int inode_no;
	if( (inode_no = get_free_inode()) < 0)
	{
		printf("DEBUG No remaining inode\n");
		return -1;
	}
	fs->super_block.used_inode++;
	struct inode_ *dest_inode = fs->inode_list+inode_no;
	dest_inode->type = REGULAR;
	get_time(&dest_inode->last_modified);
	get_time(&dest_inode->last_read);
	get_time(&dest_inode->last_inode_modified);
	init_inode(dest_inode);
	set_permission(dest_inode,755);
	
	fs->super_block.map_inode[inode_no] = 1;
	char dest_file[28];  
	extract_filename(dest,dest_file);
	add_file(dest_file,inode_no,parent);
	int sourcefd = open(source,O_RDONLY);
	struct stat buff;
	stat(source,&buff);
	if(buff.st_size > MAX_FILE_SIZE)
	{
			printf("DEBUG:: To big a file to copy\n");
			close(sourcefd);
			return -1;
	}
	dest_inode->file_size = (unsigned int)buff.st_size;
	parent->file_size += sizeof(struct directory);
	int indir_ptr = EMPTY,double_indir_ptr = EMPTY;
	int dir_count = 0,indir_count = 0,double_indir_count = 0;
	unsigned int *start = &(dest_inode->dir_ptr[0]);
	int *double_start = NULL;
	struct data_bck *cur = NULL;
	while(true)
	{
		 if(dir_count < 8)
		{
			*start = get_free_data_block();
			fs->super_block.used_data_block++;
			fs->super_block.map_data_block[*start] = 1;
	        cur = (struct data_bck*)(fs->data_block+*start);
	        ++start;
			++dir_count;

	    }
		else if(indir_count < BLOCK_SIZE/sizeof(int))
		{
			if(indir_ptr == EMPTY)
			{
					indir_ptr = get_free_data_block();
					fs->super_block.used_data_block++;
					fs->super_block.map_data_block[indir_ptr] = 1;
					dest_inode->indir_ptr = indir_ptr;
					start = (unsigned int*)(fs->data_block+indir_ptr);
					
			}	
			*start = get_free_data_block();
			fs->super_block.used_data_block++;
			fs->super_block.map_data_block[*start] = 1;
			cur = (struct data_bck*)(fs->data_block + *start);
			++start;
			++indir_count;
		}
		else if(double_indir_count < BLOCK_SIZE/sizeof(int)*BLOCK_SIZE/sizeof(int))
		{
				if(double_indir_ptr == EMPTY)
				{
						double_indir_ptr = get_free_data_block();
						fs->super_block.used_data_block++;
						fs->super_block.map_data_block[double_indir_ptr] = 1;
						dest_inode->double_indir_ptr = double_indir_ptr;
						double_start = (int*)(fs->data_block+double_indir_ptr);
				}
				if(double_indir_count%(BLOCK_SIZE/sizeof(int)) == 0)
				{
						*double_start = get_free_data_block();
						fs->super_block.used_data_block++;
						fs->super_block.map_data_block[*double_start] = 1;
						start = (unsigned int*)(fs->data_block+*double_start);
						++double_start;
				}
				*start = get_free_data_block();
				fs->super_block.used_data_block++;
				fs->super_block.map_data_block[*start] = 1;
				cur = (struct data_bck*)(fs->data_block + *start);
				++start;
				++double_indir_count;
		}
		
		if(read(sourcefd,cur,BLOCK_SIZE) < BLOCK_SIZE)break;
		
	}
	
	close(sourcefd);
}

int copy_myfs2pc(char* source,char* dest)
{
	struct inode_ *source_inode;
	if(!namei(source,source_inode)){
		printf("DEBUG file doesn't exists\n");
		return -1;
	}
	int dest_fd = open(dest,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IXUSR);
	int total_size=source_inode->file_size;
	 int noofblocks=ceil(total_size/(double)BLOCK_SIZE);
    
    unsigned int* dir_ptr = source_inode->dir_ptr;
    int indir_ptr = source_inode->indir_ptr;
    int double_indir_ptr = source_inode->double_indir_ptr;
    if(noofblocks<=8)
    {
        for(int i=0;i<noofblocks;i++){
			if(i == noofblocks-1 && total_size%BLOCK_SIZE != 0)
			{
					 write(dest_fd,fs->data_block + dir_ptr[i],total_size%BLOCK_SIZE);
					 continue;
			}
            write(dest_fd,fs->data_block + dir_ptr[i],BLOCK_SIZE);
        }
    }
    else if(noofblocks<=8+sizeof(struct data_bck)/sizeof(int))
    {
        for(int i=0;i<8;i++){
            write(dest_fd,fs->data_block + dir_ptr[i],BLOCK_SIZE);
        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<noofblocks-8;i++)
        {
			if(i == noofblocks-9 && total_size%BLOCK_SIZE != 0)
			{
					write(dest_fd,fs->data_block + idr_ptrs[i],total_size%BLOCK_SIZE);
					continue;
			}
			 write(dest_fd,fs->data_block + idr_ptrs[i],BLOCK_SIZE); 
        }
        indir_ptr=0;
    }
    else
    {
        for(int i=0;i<8;i++){
			write(dest_fd,fs->data_block + dir_ptr[i],BLOCK_SIZE);
          }
        //write the data from the indirect blocks
        struct data_bck indirect_block= fs->data_block[indir_ptr];
       
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int);i++){
			write(dest_fd,fs->data_block + idr_ptrs[i],BLOCK_SIZE); 
          }
        //write the data from the double indirect blocks 
        
        int blkperblk=sizeof(struct data_bck)/sizeof(int);
        struct data_bck double_indirect_block= fs->data_block[double_indir_ptr];
        idr_ptrs = (int*)(&double_indirect_block);
        int leftdatablocks=noofblocks-8-blkperblk;
        blkperblk=sizeof(struct data_bck)/sizeof(int);

        int data_in_last_block=total_size%BLOCK_SIZE;
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int)&&leftdatablocks>0;i++)
        {
            if(leftdatablocks>blkperblk)
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<blkperblk;j++)
                {
                	write(dest_fd,fs->data_block+temp_idr_ptrs[j],BLOCK_SIZE);
                }
                leftdatablocks=leftdatablocks-blkperblk;
            }
            else
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<leftdatablocks;j++){
                	if(j==leftdatablocks-1&&data_in_last_block!=0)
                		write(dest_fd,fs->data_block+temp_idr_ptrs[j],data_in_last_block);
                    else
                    	write(dest_fd,fs->data_block+temp_idr_ptrs[j],BLOCK_SIZE);
                 }
                leftdatablocks=0;
            }
        }
    }
	close(dest_fd);
}


int mkdir_myfs(char* name)
{
		struct inode_ *parent ;
		if(namei(name,parent)){
			printf("[DEBUG]::%s already in use.Try another name\n",name);
			return -1;
		}
		
		int inode_no = get_free_inode();
		fs->super_block.used_inode++;
		fs->super_block.map_inode[inode_no] = 1;
		char dir_name[28];
		extract_filename(name,dir_name);
		
		struct inode_ *new_inode = fs->inode_list + inode_no;
		
		init_inode(new_inode);
		new_inode->type = DIRECTORY;
		new_inode->file_size = 0;
		set_permission(new_inode,755);
		get_time(&new_inode->last_modified);
		get_time(&new_inode->last_read);
		get_time(&new_inode->last_inode_modified);
		add_file(dir_name,inode_no,parent);
		add_file("..",parent-fs->inode_list,new_inode);
		new_inode->file_size += sizeof(struct directory);
		parent->file_size += sizeof(struct directory);
}
int status_myfs()
{
		if(fs == NULL)return -1;
		printf("STATUS FS:\n");
		printf("Total size:%u\n",fs->super_block.file_system_size);
		unsigned int free_space = 0;
		unsigned int n = 0,d = 0;
		for(int i = 0;i<fs->super_block.max_data_block;i++)
			if(fs->super_block.map_data_block[i] == 0)
				free_space += BLOCK_SIZE;
			else ++d;
				
		for(int i = 0;i<fs->super_block.max_inode;i++)
			if(fs->super_block.map_inode[i] == 0)
				free_space += INODE_SIZE;
			else ++n;
		printf("Free size:%u\n",free_space);
		printf("Max inodes:%u\n",fs->super_block.max_inode);
		printf("Max data blocks:%u\n",fs->super_block.max_data_block);
		printf("Total occupied inodes:%u\n",fs->super_block.used_inode);
		printf("Total occupied data blocks:%u\n",fs->super_block.used_data_block);
		printf("Location:%s/\n",ROOT);
		
}	


int dump_myfs(char *dumpfile)
{
		int fd = open(dumpfile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IXUSR);
		unsigned int tot_size = fs->super_block.file_system_size;
		int written_size  = write(fd,fs,tot_size);
		close(fd);
		return written_size;
}

int restore_myfs(char *dumpfile)
{
		int fd = open(dumpfile,O_RDONLY);
		if(fd < 0)return -1;
		struct stat buff;
		stat(dumpfile,&buff);
		return(read(fd,fs,buff.st_size));
		
}

int chmod_myfs (char *path, int mode)
{
		struct inode_ *inode;
		
		if(!namei(path,inode))
		{
				printf("DEBUG file:%s doesn't exists\n",path);
				return -1;
		}
		set_permission(inode,mode);
		
}

int init_file_sys(int size)
{
	int byte_size = 1024 * 1024 * size;
	fs->super_block.file_system_size = byte_size;
	fs->super_block.max_inode = MAX_INODE_POSSIBLE;
	byte_size -= (sizeof(struct sup_bck) + MAX_INODE_POSSIBLE*INODE_SIZE);
	fs->super_block.max_data_block = (byte_size/BLOCK_SIZE>=MAX_DATA_BLOCK_POSSIBLE)?MAX_DATA_BLOCK_POSSIBLE:byte_size/BLOCK_SIZE;
	
	
	//cout<<fs->super_block.map_inode<<"\n";
	
	//cout<<fs->super_block.map_data_block<<"\n";
	fs->inode_list=(struct inode_*)(fs+1);
	fs->data_block=(struct data_bck*)(fs->inode_list+fs->super_block.max_inode);
	
	for(int i = 0;i<fs->super_block.max_inode;i++)
		init_inode(fs->inode_list+i);
	struct inode_ *root_node = fs->inode_list;
	root_node->type = DIRECTORY;
	root_node->file_size = 0;
	get_time(&root_node->last_modified);
	get_time(&root_node->last_read);
	get_time(&root_node->last_inode_modified);
	set_permission(root_node,755);
	reset_map();
	fs->super_block.map_inode[0] = 1;
	fs->super_block.used_inode = 1;
	fs->super_block.used_data_block = 0;
	curr_dir = root_node;
	add_file("..",0,root_node);
	root_node->file_size += sizeof(struct directory);
	return 0;	
}

int rm_myfs(char* filename)
{
    struct inode_* file_inode;
    if(namei(filename,file_inode)==false) return -1;
    else
    {
			//printf("[DEBUG]:: inode_no: %d\n",file_inode-fs->inode_list);
	}
    int total_size=file_inode->file_size;
    int noofblocks=ceil(total_size/(double)BLOCK_SIZE);
    int inode_no=file_inode-fs->inode_list;
    unsigned int* dir_ptr = file_inode->dir_ptr;
    int indir_ptr = file_inode->indir_ptr;
    int double_indir_ptr = file_inode->double_indir_ptr;
    if(noofblocks<=8)
    {
        for(int i=0;i<noofblocks;i++){
            fs->super_block.map_data_block[dir_ptr[i]]=0;
            fs->super_block.used_data_block--;
        }
        fs->super_block.map_inode[inode_no]=0;
        fs->super_block.used_inode--;
    }
    else if(noofblocks<=8+sizeof(struct data_bck)/sizeof(int))
    {
        for(int i=0;i<8;i++){
            fs->super_block.map_data_block[dir_ptr[i]]=0;
            fs->super_block.used_data_block--;
        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        fs->super_block.map_data_block[indir_ptr]=0;
        fs->super_block.used_data_block--;
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<noofblocks-8;i++)
        {
            fs->super_block.map_data_block[idr_ptrs[i]]=0;
            fs->super_block.used_data_block--;
        }
        indir_ptr=0;
        fs->super_block.map_inode[inode_no]=0;
        fs->super_block.used_inode--;
    }
    else
    {
        for(int i=0;i<8;i++){
           fs->super_block.map_data_block[dir_ptr[i]]=0;
           fs->super_block.used_data_block--;
          }
        //delete the data from the indirect blocks
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        fs->super_block.map_data_block[indir_ptr]=0;
        fs->super_block.used_data_block--;
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int);i++){
            fs->super_block.map_data_block[idr_ptrs[i]]=0;
            fs->super_block.used_data_block--;
          }
        //delete the data from the double indirect blocks
        int blkperblk=sizeof(struct data_bck)/sizeof(int);
        struct data_bck double_indirect_block= fs->data_block[double_indir_ptr];
        fs->super_block.map_data_block[double_indir_ptr]=0;
        fs->super_block.used_data_block--;
        idr_ptrs = (int*)(&double_indirect_block);
        int leftdatablocks=noofblocks-8-blkperblk;
        blkperblk=sizeof(struct data_bck)/sizeof(int);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int)&&leftdatablocks>0;i++)
        {
            if(leftdatablocks>blkperblk)
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<blkperblk;j++){
                    fs->super_block.map_data_block[temp_idr_ptrs[j]]=0;
                    fs->super_block.used_data_block--;
                   }
                fs->super_block.map_data_block[idr_ptrs[i]]=0;
                fs->super_block.used_data_block--;
                leftdatablocks=leftdatablocks-blkperblk;
            }
            else
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<leftdatablocks;j++){
                    fs->super_block.map_data_block[temp_idr_ptrs[j]]=0;
                    fs->super_block.used_data_block--;
                 }
                fs->super_block.map_data_block[idr_ptrs[i]]=0;
                fs->super_block.used_data_block--;
                leftdatablocks=0;
            }
        }
        fs->super_block.map_inode[inode_no]=0;
        fs->super_block.used_inode--;
    }
   decrement_dir_count(curr_dir,file_inode-fs->inode_list);
   
    return 0;
}

int rmdir_myfs(char* path)
{
		struct inode_ *inode;
		struct directory_bck *temp;
		if(!namei(path,inode))
		{
				printf("DEBUG %s doesn't exists\n",path);
				return -1;
		}
		if(inode->type != DIRECTORY)
		{
				printf("DEBUG %s is not a directory\n",path);
				return -1;
		}
		
		int fill_blocks = ceil(inode->file_size/double(BLOCK_SIZE));
		struct inode_ *parent;
		for(int i = 0;i<fill_blocks;i++)
		{
			temp = (struct directory_bck*)(fs->data_block+inode->dir_ptr[i]);
			
			for(int j = 0;j<8;j++)
			{
					
					if((i*8 + j)*sizeof(struct directory) >= inode->file_size)break; 
					
					if(i*8 + j == 0)
					{
							parent = fs->inode_list+temp->dir_list[0].inode_no;
							continue;
					}
					char new_path[strlen(path)+ strlen(temp->dir_list[j].dir_name) + 2];
					strcpy(new_path,path);
					new_path[strlen(path)] =  '/';
					new_path[strlen(path)+1] = '\0';
					strcat(new_path,temp->dir_list[j].dir_name);
					
					if(fs->inode_list[temp->dir_list[j].inode_no].type == REGULAR)
					{	
						rm_myfs(temp->dir_list[j].dir_name);
					}
					else
					{
						
						rmdir_myfs(new_path);
					}	
			}
			fs->super_block.map_data_block[inode->dir_ptr[i]] = 0;
			
		}
		decrement_dir_count(parent,inode-fs->inode_list);
		
		fs->super_block.map_inode[inode-fs->inode_list] = 0;
		
		
}

int create_myfs(int size)
{
	int byte_size = 1024 * 1024 * size;
	int fd_table_size = MAX_FD*sizeof(struct file_des);
	if( (shmid = shmget(IPC_PRIVATE,byte_size+fd_table_size,0777|IPC_CREAT)) == -1)
	{
		return -1;
	}
	fs=(struct file_sys*)(shmat(shmid,NULL,0));
	fd_list = (struct file_des*)((char*)fs+byte_size);
	for(int i = 0;i<MAX_FD;i++){
		fd_list[i].inode_no = EMPTY;
		fd_list[i].offset = 0;
	}
	/* Initialise attribute to mutex. */
	pthread_mutexattr_init(&attrmutex);
	pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

	/* Initialise mutex. */
	pthread_mutex_init(&pmutex, &attrmutex);
	return init_file_sys(size);
}



int showfile_myfs(char *filename)
{
	struct inode_* file_inode;
    if(namei(filename,file_inode)==false) return -1;
    int total_size=file_inode->file_size;
    int noofblocks=ceil(total_size/(double)BLOCK_SIZE);
    int inode_no=file_inode-fs->inode_list;
    unsigned int* dir_ptr = file_inode->dir_ptr;
    int indir_ptr = file_inode->indir_ptr;
    int double_indir_ptr = file_inode->double_indir_ptr;
    if(noofblocks<=8)
    {
        for(int i=0;i<noofblocks;i++)
            print_data_block(dir_ptr[i]);
        return 0;
    }
    else if(noofblocks<=8+sizeof(struct data_bck)/sizeof(int))
    {
        for(int i=0;i<8;i++)
            print_data_block(dir_ptr[i]);
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<noofblocks-8;i++)
            print_data_block(idr_ptrs[i]);
        return 0;
       
    }
    else
    {
        for(int i=0;i<8;i++)
           print_data_block(dir_ptr[i]);
        //delete the data from the indirect blocks
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptrs = (int*)(&indirect_block);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int);i++)
            print_data_block(idr_ptrs[i]);
        //delete the data from the double indirect blocks
        int blkperblk=sizeof(struct data_bck)/sizeof(int);
        struct data_bck double_indirect_block= fs->data_block[double_indir_ptr];
        idr_ptrs = (int*)(&double_indirect_block);
        int leftdatablocks=noofblocks-8-blkperblk;
        blkperblk=sizeof(struct data_bck)/sizeof(int);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int)&&leftdatablocks>0;i++)
        {
            if(leftdatablocks>blkperblk)
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<blkperblk;j++)
                    print_data_block(temp_idr_ptrs[j]);
                leftdatablocks=leftdatablocks-blkperblk;
            }
            else
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<leftdatablocks;j++)
                    print_data_block(temp_idr_ptrs[j]);
                leftdatablocks=0;
            }
        }
    }
    return 0;
}

void print_directory(struct directory dir)
{
	
	struct inode_ my_inode =fs->inode_list[dir.inode_no];
	if(my_inode.type == DIRECTORY)
	printf("DIRECTORY ");
	else
	printf("          ");
	for(int i = 0;i<9;i++)
	{
			if(my_inode.mode[i] == 0)
			printf("-");
			else if(i%3 == 0)
			printf("r");
			else if(i%3 == 1)
			printf("w");
			else
			printf("x");
	}
	printf("\t%d\t",my_inode.file_size);
	print_time(my_inode.last_modified);
	printf("\t%s\n",dir.dir_name);
	return;
}

int ls_myfs()
{
	struct inode_* my_inode = curr_dir;
	int ct_dir= my_inode->file_size/sizeof(directory);
	printf("total %u\n",my_inode->file_size);
	for(int i=0;i<8&&ct_dir>0;i++)
	{
		if(ct_dir>8)
		{
			struct directory_bck* listptr= (struct directory_bck*)(&(fs->data_block[my_inode->dir_ptr[i]]));
			for(int j=0;j<8;j++)
				print_directory(listptr->dir_list[j]);
			ct_dir=ct_dir-8;
		}
		else
		{
			struct directory_bck* listptr= (struct directory_bck*)(&(fs->data_block[my_inode->dir_ptr[i]]));
			for(int j=0;j<ct_dir;j++)
				print_directory(listptr->dir_list[j]);
			ct_dir=0;
		}
	}
	return 0;
}
int chdir_myfs (char *dirname)
{
		struct inode_ *inode;
		if(!namei(dirname,inode))
		{
			printf("DEBUG %s doesn't exists\n",dirname);
			return -1;
		}
		
		curr_dir = inode;
		return 0;
}

int open_myfs (char *filename, char mode)
{
		if(mode != 'r' && mode != 'w')
		{
				printf("DEBUG invalid mode %c\n",mode);
				return -1;
		}
		struct inode_ *inode;
		if(!namei(filename,inode))
		{
				printf("DEBUG %s doesn't exists\n",filename);
				return -1;
		}
		int fd = get_free_fd();
		if(fd < 0)
		{
				printf("DEBUG all fd used\n");
				return fd;
		}
		fd_list[fd].offset = 0;
		fd_list[fd].mode = mode;
		fd_list[fd].inode_no = inode-fs->inode_list;
		return fd;
}

int close_myfs(int fd)
{
		if(fd_list[fd].inode_no == EMPTY)
		return -1;
		fd_list[fd].inode_no = EMPTY;
		return 0;
}

int eof_myfs (int fd)
{
	if(fd_list[fd].inode_no == EMPTY)
		return -1;
	unsigned int file_size = (fs->inode_list+(fd_list[fd].inode_no))->file_size;
	if(fd_list[fd].offset == file_size)
	return 1;
	
	return 0;
}

int read_myfs (int fd, int nbytes, char *buff)
{
	buff[0]='\0';
	if(fd_list[fd].inode_no == EMPTY || fd_list[fd].mode != 'r')
		return -1;
	unsigned int file_size = (fs->inode_list+(fd_list[fd].inode_no))->file_size;
	//printf("%d\n",file_size);
	struct inode_ *file_inode = fs->inode_list+(fd_list[fd].inode_no);
	struct data_bck* last_db;
	
	int offset_rem=fd_list[fd].offset;
	fd_list[fd].offset += nbytes;
	int bytes_left=BLOCK_SIZE-(offset_rem%BLOCK_SIZE);

	int last_node=ceil(offset_rem/(double)BLOCK_SIZE);
	int read_count = 0;
	int block_count=0;

    int total_size=nbytes+offset_rem;
    int noofblocks=ceil(total_size/(double)BLOCK_SIZE);
    int inode_no=file_inode-fs->inode_list;
    unsigned int* dir_ptr = file_inode->dir_ptr;
    int indir_ptr = file_inode->indir_ptr;
    int double_indir_ptr = file_inode->double_indir_ptr;
    //printf("%d %d %d %d %d %d\n",last_node, total_size,BLOCK_SIZE,noofblocks,nbytes,bytes_left);
    if(noofblocks<=8)
    {
        for(int i=0;i<noofblocks;i++)
        {
        	block_count++;
        	//printf("%d\n",block_count);
        	if(block_count==last_node)
        	{
        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        		continue;
        		if(nbytes>=bytes_left)
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			nbytes-=bytes_left;
        			//printf("%s\n-----------llplplp",buff);
        			if(nbytes==0) return strlen(buff);
        			//printf("%s\n-----------llplplp",buff);
        		}
        		else
        		{
        			//printf("DEGU::\n");
        			int temp_off = offset_rem % BLOCK_SIZE;
        			//printf("temp_off:%d\n",temp_off);
        			//temp_off=0;
        			char* bck_data=(char*)(&fs->data_block[dir_ptr[i]]);
					char temp[BLOCK_SIZE+1];
        			int r=0;
        			//printf("DEGU:: %d",temp_off);
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			//printf("%s",temp);
        			strcat(buff,temp);
        			//cout<<buff;
        			return strlen(buff);	
        		}
        	}
        	else if(block_count>last_node)
        	{
        		//printf("%d\n",block_count);
        		if(nbytes>=BLOCK_SIZE)
        		{
        			nbytes-=BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=0;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			if(nbytes==0)
        				return strlen(buff);
        		}
        		else
        		{
        			//printf("-----\n\n");
        			int temp_off=0;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			///printf("%s-----\n\n",buff);
        			return strlen(buff);
        		}
        	}
        	else;

        }
    }
    else if(noofblocks<=8+sizeof(struct data_bck)/sizeof(int))
    {
        for(int i=0;i<8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        			if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        			continue;
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			nbytes-=bytes_left;
        	}
        	else if(block_count>last_node)
        	{
        			nbytes-=BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=0;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			if(nbytes==0)
        				return strlen(buff);
        		
        	}

        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptr = (int*)(&indirect_block);
        for(int i=0;i<noofblocks-8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        		continue;
        		if(nbytes>=bytes_left)
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			nbytes-=bytes_left;
        		}
        		else
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			return strlen(buff);	
        		}
        	}
        	else if(block_count>last_node)
        	{
        		if(nbytes>=BLOCK_SIZE)
        		{
        			nbytes-=BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=0;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			if(nbytes==0)
        				return strlen(buff);
        		}
        		else
        		{
        			int temp_off=0;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			return strlen(buff);
        		}
        	}

        }
    }
    else
    {
     	 for(int i=0;i<8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        			if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        			continue;
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			nbytes-=bytes_left;
        	}
        	else if(block_count>last_node)
        	{
        			nbytes-=BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=0;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			if(nbytes==0)
        				return strlen(buff);
        		
        	}

        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptr = (int*)(&indirect_block);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int);i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        			if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        			continue;
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			nbytes-=bytes_left;
        	}
        	else if(block_count>last_node)
        	{
        			nbytes-=BLOCK_SIZE;
        			char* temp;
        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			int r=0;
        			for(int k=0;k<BLOCK_SIZE;k++)
        				temp[r++]=bck_data[k];
        			temp[r]='\0';
        			strcat(buff,temp);
        			if(nbytes==0)
        				return strlen(buff);
        	}

        }
   
        int blkperblk=sizeof(struct data_bck)/sizeof(int);
        struct data_bck double_indirect_block= fs->data_block[double_indir_ptr];
        int * idr_ptrs = (int*)(&double_indirect_block);
        int leftdatablocks=noofblocks-8-blkperblk;
        blkperblk=sizeof(struct data_bck)/sizeof(int);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int)&&leftdatablocks>0;i++)
        {
            if(leftdatablocks>blkperblk)
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<blkperblk;j++)
                {
                    block_count++;
		        	if(block_count==last_node)
		        	{
		        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        				continue;
		        		if(nbytes>=bytes_left)
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<BLOCK_SIZE;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			nbytes-=bytes_left;
		        		}
		        		else
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<temp_off+nbytes;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			return strlen(buff);	
		        		}
		        	}
		        	else if(block_count>last_node)
		        	{
		        		if(nbytes>=BLOCK_SIZE)
		        		{
		        			nbytes-=BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=0;k<BLOCK_SIZE;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			if(nbytes==0)
		        				return strlen(buff);
		        		}
		        		else
		        		{
		        			int temp_off=0;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<temp_off+nbytes;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			return strlen(buff);
		        		}
		        	}

                 }
                leftdatablocks=leftdatablocks-blkperblk;
            }
            else
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<leftdatablocks;j++)
                {
                     block_count++;
		        	if(block_count==last_node)
		        	{
		        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        				continue;
		        		if(nbytes>=bytes_left)
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<BLOCK_SIZE;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			nbytes-=bytes_left;
		        		}
		        		else
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<temp_off+nbytes;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			return strlen(buff);	
		        		}
		        	}
		        	else if(block_count>last_node)
		        	{
		        		if(nbytes>=BLOCK_SIZE)
		        		{
		        			nbytes-=BLOCK_SIZE;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=0;k<BLOCK_SIZE;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			if(nbytes==0)
		        				return strlen(buff);
		        		}
		        		else
		        		{
		        			int temp_off=0;
		        			char* temp;
		        			temp=(char*)malloc(sizeof(char)*(BLOCK_SIZE+1));
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<temp_off+nbytes;k++)
		        				temp[r++]=bck_data[k];
		        			temp[r]='\0';
		        			strcat(buff,temp);
		        			return strlen(buff);
		        		}
		        	}

                	 
                }
                leftdatablocks=0;
            }
    	}
    }
	fd_list[fd].offset += read_count;
	return read_count;
}
int write_myfs (int fd, int nbytes, char *buff)
{
	if(fd_list[fd].inode_no == EMPTY || fd_list[fd].mode != 'w')
		return -1;
	unsigned int file_size = (fs->inode_list+(fd_list[fd].inode_no))->file_size;
	//printf("%d\n",file_size);
    (fs->inode_list+(fd_list[fd].inode_no))->file_size+=nbytes;
	struct inode_ *file_inode = fs->inode_list+(fd_list[fd].inode_no);
	struct data_bck* last_db;
	//fd_list[fd].offset=60000;
	int offset_rem=fd_list[fd].offset;
	int bytes_left=BLOCK_SIZE-(offset_rem%BLOCK_SIZE);
	fd_list[fd].offset += nbytes;
	int last_node=ceil(offset_rem/(double)BLOCK_SIZE);
	int read_count = 0;
	int block_count=0;
    int write_count=0;
    int total_size=nbytes+offset_rem;
    int noofblocks=ceil(total_size/(double)BLOCK_SIZE);
    int inode_no=file_inode-fs->inode_list;
    unsigned int* dir_ptr = file_inode->dir_ptr;
    int indir_ptr = file_inode->indir_ptr;
    int double_indir_ptr = file_inode->double_indir_ptr;
    //printf("%d %d %d %d %d %d\n",last_node, total_size,BLOCK_SIZE,noofblocks,nbytes,bytes_left);
    if(noofblocks<=8)
    {
        for(int i=0;i<noofblocks;i++)
        {
        	block_count++;
        	//printf("%d\n",block_count);
        	if(block_count==last_node)
        	{
        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        		continue;
        		if(nbytes>=bytes_left)
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			int r=0;
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				bck_data[k]=buff[write_count++];
        			nbytes-=bytes_left;
        			//printf("%s\n-----------llplplp",buff);
        			if(nbytes==0) return write_count;
        			//printf("%s\n-----------llplplp",buff);
        		}
        		else
        		{
        			//printf("DEGU::\n");
        			int temp_off=offset_rem % BLOCK_SIZE;
        			//printf("temp_off:%d\n",temp_off);
        			//temp_off=0;
        			char* bck_data=(char*)(&fs->data_block[dir_ptr[i]]);
        			//printf("DEGU:: %d",temp_off);
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				bck_data[k]=buff[write_count++];
        			//printf("%s",temp);
        			//cout<<buff;
        			return write_count;	
        		}
        	}
        	else if(block_count>last_node)
        	{
        		//printf("%d\n",block_count);

                dir_ptr[i]=get_free_data_block();
                fs->super_block.map_data_block[dir_ptr[i]]=1;
                fs->super_block.used_data_block++;

        		if(nbytes>=BLOCK_SIZE)
        		{
        			nbytes-=BLOCK_SIZE;
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			for(int k=0;k<BLOCK_SIZE;k++)
        				bck_data[k]=buff[write_count++];
        			if(nbytes==0)
        				return write_count;
        		}
        		else
        		{
        			//printf("-----\n\n");
        			char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
        			for(int k=0;k<nbytes;k++)
        				bck_data[k]=buff[write_count++];
        			///printf("%s-----\n\n",buff);
        			return write_count;
        		}
        	}
        	else;

        }
    }
    else if(noofblocks<=8+sizeof(struct data_bck)/sizeof(int))
    {
        for(int i=0;i<8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        			if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        			continue;
                    int temp_off=offset_rem%BLOCK_SIZE;
                    char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
                    int r=0;
                    for(int k=temp_off;k<BLOCK_SIZE;k++)
                        bck_data[k]=buff[write_count++];
                    nbytes-=bytes_left;
                    //printf("%s\n-----------llplplp",buff);
                    if(nbytes==0) return write_count;
                    //printf("%s\n-----------llplplp",buff);
        	}
        	else if(block_count>last_node)
        	{
                    dir_ptr[i]=get_free_data_block();
                    fs->super_block.map_data_block[dir_ptr[i]]=1;
                    fs->super_block.used_data_block++;
                    nbytes-=BLOCK_SIZE;
                    char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
                    for(int k=0;k<BLOCK_SIZE;k++)
                        bck_data[k]=buff[write_count++];
                    if(nbytes==0)
                        return write_count;
        	}

        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptr = (int*)(&indirect_block);
        for(int i=0;i<noofblocks-8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{
        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        		continue;
        		if(nbytes>=bytes_left)
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			for(int k=temp_off;k<BLOCK_SIZE;k++)
        				bck_data[k]=buff[write_count++];
        			nbytes-=bytes_left;
        		}
        		else
        		{
        			int temp_off=offset_rem%BLOCK_SIZE;
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			for(int k=temp_off;k<temp_off+nbytes;k++)
        				bck_data[k]=buff[write_count++];
        			return write_count;	
        		}
        	}
        	else if(block_count>last_node)
        	{
                idr_ptr[i]=get_free_data_block();
                fs->super_block.map_data_block[idr_ptr[i]]=1;
                fs->super_block.used_data_block++;
        		if(nbytes>=BLOCK_SIZE)
        		{
        			nbytes-=BLOCK_SIZE;
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			for(int k=0;k<BLOCK_SIZE;k++)
        			 bck_data[k]=buff[write_count++];
        			if(nbytes==0)
        				return write_count;
        		}
        		else
        		{
        			char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
        			for(int k=0;k<nbytes;k++)
        				bck_data[k]=buff[write_count++];
        			return write_count;
        		}
        	}

        }
    }
    else
    {
     	 for(int i=0;i<8;i++)
        {
        	block_count++;
        	if(block_count==last_node)
            {
            		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        			continue;
                    int temp_off=offset_rem%BLOCK_SIZE;
                    char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
                    int r=0;
                    for(int k=temp_off;k<BLOCK_SIZE;k++)
                        bck_data[k]=buff[write_count++];
                    nbytes-=bytes_left;
                    //printf("%s\n-----------llplplp",buff);
                    if(nbytes==0) return write_count;
                    //printf("%s\n-----------llplplp",buff);
            }
            else if(block_count>last_node)
            {
                    dir_ptr[i]=get_free_data_block();
                    fs->super_block.map_data_block[dir_ptr[i]]=1;
                    fs->super_block.used_data_block++;
                    nbytes-=BLOCK_SIZE;
                    char* bck_data=(char*)(fs->data_block+dir_ptr[i]);
                    for(int k=0;k<BLOCK_SIZE;k++)
                        bck_data[k]=buff[write_count++];
                    if(nbytes==0)
                        return write_count;
            }


        }
        struct data_bck indirect_block= fs->data_block[indir_ptr];
        int * idr_ptr = (int*)(&indirect_block);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int);i++)
        {
        	block_count++;
        	if(block_count==last_node)
        	{	
        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        		continue;
        		int temp_off=offset_rem%BLOCK_SIZE;
                char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
                for(int k=temp_off;k<BLOCK_SIZE;k++)
                    bck_data[k]=buff[write_count++];
                nbytes-=bytes_left;	
        	}
        	else if(block_count>last_node)
        	{
        		idr_ptr[i]=get_free_data_block();
                fs->super_block.map_data_block[idr_ptr[i]]=1;
                fs->super_block.used_data_block++;
                nbytes-=BLOCK_SIZE;
                char* bck_data=(char*)(fs->data_block+idr_ptr[i]);
                for(int k=0;k<BLOCK_SIZE;k++)
                 bck_data[k]=buff[write_count++];
                if(nbytes==0)
                    return write_count;      
        	}

        }

        int blkperblk=sizeof(struct data_bck)/sizeof(int);
        struct data_bck double_indirect_block= fs->data_block[double_indir_ptr];
        int * idr_ptrs = (int*)(&double_indirect_block);
        int leftdatablocks=noofblocks-8-blkperblk;
        blkperblk=sizeof(struct data_bck)/sizeof(int);
        for(int i=0;i<sizeof(struct data_bck)/sizeof(int)&&leftdatablocks>0;i++)
        {
            if(leftdatablocks>blkperblk)
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<blkperblk;j++)
                {
                    block_count++;
		        	if(block_count==last_node)
		        	{
		        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        				continue;
		        		if(nbytes>=bytes_left)
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			for(int k=temp_off;k<BLOCK_SIZE;k++)
		        			   bck_data[k]=buff[write_count++];
		        			nbytes-=bytes_left;
		        		}
		        		else
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
		        			int r=0;
		        			for(int k=temp_off;k<temp_off+nbytes;k++)
		        				bck_data[k]=buff[write_count++];
		        			return write_count;	
		        		}
		        	}
		        	else if(block_count>last_node)
		        	{
                        temp_idr_ptrs[i]=get_free_data_block();
                        fs->super_block.map_data_block[temp_idr_ptrs[i]]=1;
                        fs->super_block.used_data_block++;
		        		if(nbytes>=BLOCK_SIZE)
		        		{
		        			nbytes-=BLOCK_SIZE;
                            char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            for(int k=0;k<BLOCK_SIZE;k++)
                               bck_data[k]=buff[write_count++];
                            nbytes-=bytes_left;
		        			if(nbytes==0)
		        				return write_count;
		        		}
		        		else
		        		{
                            char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            int r=0;
                            for(int k=0;k<nbytes;k++)
                                bck_data[k]=buff[write_count++];
                            return write_count; 
		        		}
		        	}

                 }
                leftdatablocks=leftdatablocks-blkperblk;
            }
            else
            {
                struct data_bck temp_double_indirect_block= fs->data_block[idr_ptrs[i]];
                int * temp_idr_ptrs = (int*)(&temp_double_indirect_block);
                for(int j=0;j<leftdatablocks;j++)
                {
                     block_count++;
		        	if(block_count==last_node)
		        	{
		        		if(offset_rem%BLOCK_SIZE==0&&offset_rem!=0)
        				continue;
		        		if(nbytes>=bytes_left)
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
                            char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            for(int k=temp_off;k<BLOCK_SIZE;k++)
                               bck_data[k]=buff[write_count++];
                            nbytes-=bytes_left;
		        		}
		        		else
		        		{
		        			int temp_off=offset_rem%BLOCK_SIZE;
                            char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            int r=0;
                            for(int k=temp_off;k<temp_off+nbytes;k++)
                                bck_data[k]=buff[write_count++];
                            return write_count; 	
		        		}
		        	}
		        	else if(block_count>last_node)
		        	{
                        temp_idr_ptrs[i]=get_free_data_block();
                        fs->super_block.map_data_block[temp_idr_ptrs[i]]=1;
                        fs->super_block.used_data_block++;
		        		if(nbytes>=BLOCK_SIZE)
		        		{
		        			nbytes-=BLOCK_SIZE;
                            char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            for(int k=0;k<BLOCK_SIZE;k++)
                               bck_data[k]=buff[write_count++];
                            nbytes-=bytes_left;
                            if(nbytes==0)
                                return write_count;
		        		}
		        		else
		        		{
		        			char* bck_data=(char*)(fs->data_block+temp_idr_ptrs[j]);
                            int r=0;
                            for(int k=0;k<nbytes;k++)
                                bck_data[k]=buff[write_count++];
                            return write_count; 
		        		}
		        	}

                	 
                }
                leftdatablocks=0;
            }
    	}
    }
	
	return write_count;
}

int createfile_myfs(char* name)
{
		struct inode_ *parent ;
		if(namei(name,parent)){
			printf("[DEBUG]::%s already in use.Try another name\n",name);
			return -1;
		}
		
		int inode_no = get_free_inode();
		fs->super_block.used_inode++;
		fs->super_block.map_inode[inode_no] = 1;
		char file_name[28];
		extract_filename(name,file_name);
		
		struct inode_ *new_inode = fs->inode_list + inode_no;
		
		init_inode(new_inode);
		new_inode->type = REGULAR;
		new_inode->file_size = 0;
		set_permission(new_inode,755);
		get_time(&new_inode->last_modified);
		get_time(&new_inode->last_read);
		get_time(&new_inode->last_inode_modified);
		add_file(file_name,inode_no,parent);
		parent->file_size += sizeof(struct directory);
}


void cleanup()
{
	pthread_mutex_destroy(&pmutex);
	pthread_mutexattr_destroy(&attrmutex);
	if(shmctl(shmid,IPC_RMID,NULL) == -1)
	{
			printf("ERROR:: in removing shared_memory\n");
	}
}

/**
int main()
{
	printf("[DEBUG]: INODE_SIZE=%d, MAX_INODE_POSSIBLE=%d, MAX_DATA_BLOCK_POSSIBLE=%d, MAX_FILE_SIZE=%d\nBLOCK_SIZE=%d, time_size=%d, sup_bck_size = %d, directory_bck_size=%d\n",INODE_SIZE,MAX_INODE_POSSIBLE,MAX_DATA_BLOCK_POSSIBLE,MAX_FILE_SIZE,BLOCK_SIZE,sizeof(struct time),sizeof(struct sup_bck),sizeof(struct directory_bck));
	create_myfs(10);
	copy_pc2myfs("part_b.py","part_b.py");
	//showfile_myfs("part_b.py");
	int fd = open_myfs("part_b.py",'r');
	//ls_myfs();
	char buff[801];
	//printf("%d\n",fd_list[fd].inode_no);
	read_myfs(fd,800,buff);
	printf("%s\n\n",buff);
	buff[800]='\0';
	//buff[801]='\n';
	createfile_myfs("test.txt");
	fd = open_myfs("test.txt",'w');
	write_myfs(fd,800,buff);
	buff[0] = '4';
	buff[1] = '0';
	write_myfs(fd,2,buff);
	close_myfs(fd);
	showfile_myfs("test.txt");
	ls_myfs();
	//buff[12] = '\0';
	//printf("%s\n",buff);
	close_myfs(fd);
	if(shmctl(shmid,IPC_RMID,NULL) == -1)
	{
			return -1;
	}
	return 0;
}

**/
