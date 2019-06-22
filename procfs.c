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
#define BUFFER_SIZE 2048
#define MAX_NUM_LEN 32

#define IS_ALL(ip) (ip->inum < block.ninodes ? true : false)
#define IS_IDEINFO(ip) (ip->inum == block.ninodes + 1 ? true : false)
#define IS_FILESTAT(ip) (ip->inum == block.ninodes + 2 ? true : false)
#define IS_INODEINFO(ip) (ip->inum == block.ninodes + 3 ? true : false)

int my_minor = 0;
enum my_inum {PROC, IDEINFO, FILESTAT, INODEINFO}; 
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
  	return !((ip->major == PROCFS) & (ip->type == T_DEV) & 
  		(!IS_IDEINFO(ip)) & (!IS_FILESTAT(ip)));
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	ip->major = PROCFS;
	ip->type = T_DEV;
	ip->valid = 1;
	ip->minor = my_minor++; //???
}

void
add_one(char *buff, int inum, const char *dir){
	struct dirent dirent;
	dirent.inum = inum;
	memmove(&dirent.name, dir, strlen(dir) + 1);
	sprintf(get_buff(buff), (char*) &dirent);
}

void
add_all(char *buff, int inum){
	add_one(buff, namei("/proc")->inum, ".");
	add_one(buff, namei("")->inum, "..");
	add_one(buff, inum + 1, "ideinfo");
	add_one(buff, inum + 2, "filestat");
	add_one(buff, inum + 3, "ideinfo");
	int pids[NPROC];
	memset(pids, 0, NPROC);
	int i;
	for (i = 0; i < NPROC; ++i)
	{
		char tmp_num[MAX_NUM_LEN];

	}
}

void
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
}

void
filestat_handler(char* buff){
	struct file *files[NOFILE];
	get_proc_files(files);

	sprintf(get_buff(buff), "%s:%d\n%s:%d\n%s:%d\n%s:%d\n%s:%d\n",
		"Free fds", get_free(files),
		"Unique inode fds", get_unique(files),
		"Writable fds", get_wr(files, 1),
		"Readable fds", get_wr(files, 0),
		"Refs per fds", get_refs(files));
}

void
inodeinfo_handler(char* buff, int inum){

	struct inode *ip = get_inode(inum);
  	if(!ip)
  		return;

	sprintf(get_buff(buff), "%s:%d\n%s:%d\n%s:%d\n%s:%d\n%s:(%d,%d)\n",
		"Device", ip->dev,
		"Inode number",ip->inum,
		"is valid",ip->valid,
		"type",ip->type == T_DEV ? "DEV" : ip->type == T_DIR ? "DIR" : "FILE",
		"major minor",ip->major, ip->minor,
		"hard links", ip->nlink, 
		"blocks used", ip->type == T_DEV ? 0 : ip->size / 128);
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	if (!blockInit){
  		readsb(ip->dev, &block);
  		blockInit = true;
	}

	char buff[BSIZE * 10];
	memset(buff, 0, BSIZE * 10);

	if (IS_ALL(ip))
		add_all(buff, block.ninodes);

	/*  code of IDEINFO def
		Need To Put In File -
		Waiting operations: <Number of waiting operations starting from idequeue>
		Read waiting operations: <Number of read operations>
		Write waiting operations: <Number of write operations>
		Working blocks: <List (#device,#block) that are currently in the queue separated by the ‘;’
		symbol> */
	if (IS_IDEINFO(ip))
		ideinfo_handler(buff);

	/* code of FILESTAT def
		Need To Put In File -
			Free fds: <free fd number (ref = 0)>
			Unique inode fds: <Number of different inodes open by all the fds>
			Writeable fds: <Writable fd number>
			Readable fds: <Readable fd number>
			Refs per fds: <ratio of total number of refs / number of used fds>*/
	if (IS_FILESTAT(ip))
		filestat_handler(buff);

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
	if (IS_INODEINFO(ip))
		inodeinfo_handler(buff, ip->inum);

	sprintf(dst, buff);

    return strlen(buff);
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
  panic("System Can't Write, Only Read");
  return 0;
}

void
procfsinit(void)
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
	int ref, ret = 0, i = 0, empties = 0;
    for(; i < NOFILE; i++){
        ret += (ref = files[i]->ref);
        if(!ref)
        	empties++;
    }
    return ret / (i - empties);
}