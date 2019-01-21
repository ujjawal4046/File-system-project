#include "myfs.h"

void run_tc1()
{
	printf("\n\n------Running tc1--------\n\n");
	create_myfs(10);
	copy_pc2myfs("import/mytest.txt","mytest.txt");
	copy_pc2myfs("import/imp1.png","imp1.png");
	copy_pc2myfs("import/Netcat.py","Netcat.py");
	copy_pc2myfs("import/Expt.pdf","Expt.pdf");
	copy_pc2myfs("import/mountScr.sh","mountScr.sh");
	copy_pc2myfs("import/key.txt","key.txt");
	copy_pc2myfs("import/wificonf","wificonf");
	copy_pc2myfs("import/part_a.py","part_a.py");
	copy_pc2myfs("import/part_b.py","part_b.py");
	copy_pc2myfs("import/part_c.py","part_c.py");
	copy_pc2myfs("import/part_d.py","part_d.py");
	ls_myfs();
	
	printf("Enter files to delete,\"break\" to exit\n");
	while(curr_dir->file_size > sizeof(struct directory))
	{
		char delete_file[30];
		scanf("%[^\n]",delete_file);
		getchar();
		if(strcmp(delete_file,"break")==0)break;
		//printf("[DEBUG]::%s will be deleted\n",delete_file);
		rm_myfs(delete_file);
		ls_myfs();	
	}	
}

void run_tc2()
{
	printf("\n\n------Running tc2--------\n\n");
	int fd = open_myfs("mytest.txt",'w');
	int temp;
	for(int i = 0;i<100;i++)
	{
		temp = rand()%10000;
		char str[30];
		sprintf(str,"%04d\n",temp);
		printf("%s",str);
		write_myfs(fd,strlen(str),str);
			
	}
	close_myfs(fd);	
	int n;
	printf("No of copies:");
	scanf("%d",&n);
	getchar();
	char name[257];
	for(int i = 0;i<n;i++)
	{
		char buff[257];
		sprintf(name,"mytest-%d.txt",i+1);
		createfile_myfs(name);
		int rfd = open_myfs("mytest.txt",'r');
		int wfd = open_myfs(name,'w');
		
		while(true)
		{
				//printf("\n----------Read------------------\n");
				read_myfs(rfd,1,buff);
				//printf("\n----------Print------------------\n");
				//showfile_myfs(name);
				//printf("[DEBUG] Offset:%d\n",fd_list[rfd].offset);
				//printf("\n----------Write------------------\n");
				write_myfs(wfd,1,buff);
				
				if(eof_myfs(rfd) == 1)break;
		}
		close_myfs(rfd);
		close_myfs(wfd);
		//showfile_myfs(name);	
		//getchar();
	}
	
	ls_myfs();
	
	dump_myfs("mydump_30.backup");
}

void run_tc3()
{
		printf("\n\n------Running tc3--------\n\n");
		restore_myfs("mydump_30.backup");
		//chdir_myfs(ROOT);
		ls_myfs();
		printf("---------------------------------\n");
		showfile_myfs("mytest.txt");
		printf("---------------------------------\n");
		char buff[257];
		int arr[100];
		int rfd = open_myfs("mytest.txt",'r');
		for(int i = 0;i<100;i++)
		{
			read_myfs(rfd,5,buff);
			buff[4] = '\0';
			arr[i] = atoi(buff);
		}
		close_myfs(rfd);
		sort(arr,arr+100);
		createfile_myfs("sorted.txt");
		int wfd = open_myfs("sorted.txt",'w');
		for(int i = 0;i<100;i++)
		{
			sprintf(buff,"%04d\n",arr[i]);
			write_myfs(wfd,5,buff);
		}
		close_myfs(wfd);
		showfile_myfs("sorted.txt");
		
}

void run_tc4()
{
		printf("\n\n------Running tc4--------\n\n");
		//chdir_myfs(ROOT);
		mkdir_myfs("mydocs");
		mkdir_myfs("mycode");
		chdir_myfs("mydocs");
		mkdir_myfs("mytext");
		mkdir_myfs("mypapers");
		chdir_myfs("..");
		if(fork() == 0)
		{
					//chdir_myfs(ROOT);
					chdir_myfs("mydocs");
					chdir_myfs("mytext");
					createfile_myfs("alpha.txt");
					int fd = open_myfs("alpha.txt",'w');
					char c;
					for(int i = 0;i<26;i++)
					{
						c = 'A' + i;
						write_myfs(fd,1,&c);
					}
					
					close_myfs(fd);
					ls_myfs();
					showfile_myfs("alpha.txt");
					exit(0);
		}
		else if(fork() == 0)
		{
				//chdir_myfs(ROOT);
				chdir_myfs("mycode");
				copy_pc2myfs("udp_wrapper.h","udp_wrapper.h");
				ls_myfs();
		}
		
}
int main()
{
	run_tc1();
	run_tc2();
	run_tc3();
	run_tc4();
	cleanup();
	return 0;
}
