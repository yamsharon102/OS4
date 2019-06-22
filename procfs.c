#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define false 0
#define true 1
#define start_of_PIDs 400
#define START_OF_INODES 4
#define BUFFER_SIZE 2048
#define MAX_NUM_LEN 32
#define MAX_NAME_LEN 100

#define IS_ALL(ip) (ip->inum < block.ninodes ? true : false)
#define IS_IDEINFO(ip) (ip->inum == block.ninodes + 1 ? true : false)
#define IS_FILESTAT(ip) (ip->inum == block.ninodes + 2 ? true : false)
#define IS_INODEINFO(ip) (ip->inum == block.ninodes + 3 ? true : false)
#define IS_PIDDir(ip) (((ip->inum - start_of_PIDs - block.ninodes) % 3) == 0 ? true : false)
#define IS_IN_PIDs(ip) ((ip->inum >= block.ninodes + start_of_PIDs) ? true : false)

struct superblock block;
int blockInit = 0;


int get_free(struct file **files);
int get_unique(struct file **files);
int get_wr(struct file **files, int iswrite);
int get_refs(struct file **files);

char*
get_buff(char* buff){
	return buff + strlen(buff);
}


static void
putc(char* buff, char c, int *index)
{
  buff[(*index)++] = c;
}

static void
sprinti_helper(char *buff, int x, int *index)
{
  char buf[16];
  int i = 0;
  char digits[] = "0123456789";

  do{
    buf[i++] = digits[x % 10];
  } while((x /= 10) != 0);

  while(--i >= 0)
    putc(buff, buf[i], index);
}

void sprinti(char *buff, int x){
	int index = 0;
	sprinti_helper(buff, x, &index);
}

void
sprintf(char* buff, char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;
  int index = 0;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(buff, c, &index);
      }
    } else if(state == '%'){
      if(c == 'd'){
        sprinti_helper(buff, *ap, &index);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(buff, *s, &index);
          s++;
        }
      } else if(c == '%'){
        putc(buff, c, &index);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(buff, '%', &index);
        putc(buff, c, &index);
      }
      state = 0;
    }
  }
}

int 
procfsisdir(struct inode *ip) {
	if (!blockInit){
  		readsb(ip->dev, &block);
  		blockInit = true;
	}
	return (((ip->type == T_DEV) && (ip->major == PROCFS)) 
		&& (!IS_IDEINFO(ip)) && (!IS_FILESTAT(ip))) || (IS_PIDDir(ip)) || (IS_INODEINFO(ip));
	//Maybe should add pids
}


void 
procfsiread(struct inode* dp, struct inode *ip) {
	ip->major = PROCFS;
	ip->type = T_DEV;
	ip->valid = 1;
}

void 
add_one(char *buff, int inum, const char *name, int index)
{
	struct dirent tmp_dirent;
	tmp_dirent.inum = inum;
	memmove(&tmp_dirent.name, name, strlen(name) + 1);
	memmove(buff + (index * sizeof(tmp_dirent)), &tmp_dirent, sizeof(tmp_dirent));
}

int add_all(char *buff, int inum)
{
	int index = 0;
	add_one(buff, namei("/proc")->inum, ".", index++);
	add_one(buff, namei("")->inum, "..", index++);
	add_one(buff, inum + 1, "ideinfo", index++);
	add_one(buff, inum + 2, "filestat", index++);
	add_one(buff, inum + 3, "inodeinfo", index++);

	int pids[NPROC] = { 0 };
	set_pids_for_fs(pids);

	for (int i = 0; i < NPROC; i++)
	{
		if(!pids[i])
			continue;
		char tmp_num[MAX_NUM_LEN] = { 0 };
		sprinti(tmp_num, pids[i]);
		add_one(buff, inum + start_of_PIDs + (3 * i), tmp_num, index++);
		
	}
	return (index * sizeof(struct dirent));
}

int
ideinfo_handler(char* buff){
	sprintf(get_buff(buff), "%s:%d\n%s:%d\n%s:%d\n%s:",
					"Waiting operations", get_waiting(),
				    "Read waiting operations", get_read(),
				    "Write waiting operations", get_write(),
				    "Working blocks");

	wblock *blocks[BSIZE];
	get_working(blocks);

	for (int i = 0; i < BSIZE; i++){
		wblock *curr = blocks[i];
		if(!curr)
			break;
		sprintf(get_buff(buff), "(%d,%d);", curr->device, curr->block);
	}

	sprintf(get_buff(buff), "\n");
	return strlen(buff);
}

int
filestat_handler(char* buff){
	struct file *files[NOFILE];
	get_proc_files(files);

	sprintf(get_buff(buff), "%s:%d\n%s:%d\n%s:%d\n%s:%d\n%s:%d\n",
		"Free fds", get_free(files),
		"Unique inode fds", get_unique(files),
		"Writable fds", get_wr(files, 1),
		"Readable fds", get_wr(files, 0),
		"Refs per fds", get_refs(files));
	return strlen(buff);
}

int
inodeinfo_handler(char* buff, int inum){

	int index = 0;
	add_one(buff, namei("/proc/inodeinfo")->inum, ".", index++);
	add_one(buff, namei("/proc")->inum, "..", index++);

	int indices[NINODE] = { 0 };
	get_inodes(indices);

	for (int i = 0; i < NINODE; ++i){
		if(!indices[i])
			continue;
		char name[5] = { 0 };
		sprinti(name, i);
		add_one(buff, inum + START_OF_INODES + i, name, index++);
	}
	return (index * sizeof(struct dirent));

	/* struct inode *ip = get_inode(inum);
  	if(!ip)
  		return;

	sprintf(get_buff(buff), "%s:%d\n%s:%d\n%s:%d\n%s:%d\n%s:(%d,%d)\n",
		"Device", ip->dev,
		"Inode number",ip->inum,
		"is valid",ip->valid,
		"type",ip->type == T_DEV ? "DEV" : ip->type == T_DIR ? "DIR" : "FILE",
		"major minor",ip->major, ip->minor,
		"hard links", ip->nlink, 
		"blocks used", ip->type == T_DEV ? 0 : ip->size / 128);*/
}

int
genPID_handler(char *buff, int inum){
	int index = 0;
	add_one(buff, inum, ".", index++);
	add_one(buff, namei("/proc")->inum, "..", index++);
	add_one(buff, inum+1, "name", index++);
	add_one(buff, inum+2, "status", index++);
	return (index * sizeof(struct dirent));
}

int
namePID_handler(char *buff, int inum){

	char proc_name[MAX_NAME_LEN];
	memset(proc_name, 0, MAX_NAME_LEN);
	set_proc_name(proc_name, inum-block.ninodes- start_of_PIDs);
	sprintf(buff, "%s\n", proc_name);
	return strlen(proc_name) + 1;
}

int
statusPID_handler(char *buff, int inum){
	
	int curr_id = inum-block.ninodes- start_of_PIDs;
	char proc_status[MAX_NAME_LEN];
	memset(proc_status, 0, MAX_NAME_LEN);
	char proc_sz[MAX_NUM_LEN];
	memset(proc_sz, 0, MAX_NUM_LEN);
	set_proc_status(proc_status, curr_id);
	int sz = set_proc_sz(curr_id);
	sprinti(proc_sz, sz);
	sprintf(buff, "%s %s\n", proc_status, proc_sz);
	return sizeof(proc_status) + sizeof(proc_sz) + 1;
}

int
in_pids_handler(char *buff, struct inode *ip){
	return ip->minor == GEN ? genPID_handler(buff, ip->inum) :
		   ip->minor == NAME ? namePID_handler(buff, ip->inum) :
		   ip->minor == STATUS ? statusPID_handler(buff, ip->inum) : 0;
}

int procfsread(struct inode *ip, char *dst, int off, int n)
{
	if (!blockInit){
  		readsb(ip->dev, &block);
  		blockInit = true;
	}

	char buff[BSIZE * 10];
	memset(buff, 0, BSIZE * 10);

	int offset = IS_ALL(ip) ? add_all(buff, block.ninodes) :
				 IS_IDEINFO(ip) ? ideinfo_handler(buff) :
				 IS_FILESTAT(ip) ? filestat_handler(buff) :
				 IS_INODEINFO(ip) ? inodeinfo_handler(buff, block.ninodes) :
				 IS_IN_PIDs(ip) ? in_pids_handler(buff, ip) : 0;
	/*  code of IDEINFO def
		Need To Put In File -
		Waiting operations: <Number of waiting operations starting from idequeue>
		Read waiting operations: <Number of read operations>
		Write waiting operations: <Number of write operations>
		Working blocks: <List (#device,#block) that are currently in the queue separated by the ‘;’
		symbol> */
	/* code of FILESTAT def
		Need To Put In File -
			Free fds: <free fd number (ref = 0)>
			Unique inode fds: <Number of different inodes open by all the fds>
			Writeable fds: <Writable fd number>
			Readable fds: <Readable fd number>
			Refs per fds: <ratio of total number of refs / number of used fds>*/
	/* code of INODEINFO def
		Directory - Each File -
		Name - index in the open inode table (in use)
		Content - 
		Device: <device the inode belongs to>
		Inode number: <inode number in the device>
		is valid: <0 for no, 1 for yes>
		type: <DIR, FILE or DEV>
		major minor: <(major number, minor number)>
		hard links: <number of hardlinks>
		blocks used: <number of blocks used in the file, 0 for DEV files>*/

	memmove(dst, buff + off, n);
	int diff = offset - off;
	return diff < 0 || diff > n ? n : diff;
}

int procfswrite(struct inode *ip, char *buf, int n)
{
  panic("System Can't Write, Only Read");
	return 0;
}

void procfsinit(void)
{
	devsw[PROCFS].isdir = procfsisdir;
	devsw[PROCFS].iread = procfsiread;
	devsw[PROCFS].write = procfswrite;
	devsw[PROCFS].read = procfsread;
}


int
get_free(struct file **files){
	int ret = 0;
    for(int i = 0; i < NOFILE; i++)
        if(!files[i]->ref)
            ret++;
    return ret;
} 

int
get_unique(struct file **files){
	return 0;
} 

int
get_wr(struct file** files, int iswrite){
	int ret = 0;
    for(int i = 0; i < NOFILE; i++){
    	struct file *curr = files[i];
        if(iswrite ? curr->writable : curr->readable)
            ret++;
    }
    return ret;
}

int
get_refs(struct file **files){
	int ref, ret = 0, i = 0, counter = 0;
    for(; i < NOFILE; i++){
    	ref = files[i]->ref;
        if(ref <= 0)
        	continue;
        ret += ref;
    	counter++;
    }
    return ret / counter;
}
