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
#define BUFFER_SIZE 2048
#define IS_ALL(ip) (ip->inum < block.ninodes ? true : false)
#define IS_IDEINFO(ip) (ip->inum == block.ninodes + 1 ? true : false)
#define IS_FILESTAT(ip) (ip->inum == block.ninodes + 2 ? true : false)
#define IS_INODEINFO(ip) (ip->inum == block.ninodes + 3 ? true : false)

int my_minor = 0;
enum my_inum {PROC, IDEINFO, FILESTAT, INODEINFO}; 
struct superblock block;
int blockInit = 0;

static void
putc(char* buff, char c, int *index)
{
  buff[(*index)++] = c;
}

static void
sprinti(char *buff, int x, int *index)
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
        sprinti(buff, *ap, &index);
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

void //check src type
copy_to_buff(char *buff, void *src, int *offset){
	int sizeToAdd = sizeof(src);
	memmove(buff + *offset, src, sizeToAdd);
	*offset += sizeToAdd;
}

void
add_one(char *buff, int inum, char *dir, int *offset){
	struct dirent dirent;
	dirent.inum = inum;
	memmove(&dirent.name, dir, strlen(dir) + 1);
	copy_to_buff(buff, (void *)&dirent, offset);
}

void
add_all(char *buff, int inum, int *offset){
	add_one(buff, namei("/proc")->inum, ".", offset);
	add_one(buff, namei("")->inum, "..", offset);
	add_one(buff, inum + 1, "ideinfo", offset);
	add_one(buff, inum + 2, "filestat", offset);
	add_one(buff, inum + 3, "ideinfo", offset);
}

void
ideinfo_handler(char* buff){
	/*  
		code of IDEINFO def
		Need To Put In File -
		Waiting operations: <Number of waiting operations starting from idequeue>
		Read waiting operations: <Number of read operations>
		Write waiting operations: <Number of write operations>
		Working blocks: <List (#device,#block) that are currently in the queue separated by the ‘;’
		symbol> 
	*/
	sprintf(buff, "Waiting operations: %d\n", 1);
}


void
filestat_handler(char* buff){

	/* code of FILESTAT def
		Need To Put In File -
			Free fds: <free fd number (ref = 0)>
			Unique inode fds: <Number of different inodes open by all the fds>
			Writeable fds: <Writable fd number>
			Readable fds: <Readable fd number>
			Refs per fds: <ratio of total number of refs / number of used fds>
	*/
}

void
inodeinfo_handler(char* buff){

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
				blocks used: <number of blocks used in the file, 0 for DEV files>
	*/
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
	if (!blockInit){
  		readsb(ip->dev, &block);
  		blockInit = true;
	}
	if (IS_ALL(ip)) {
		int offset = 0;
		char buff[BUFFER_SIZE] = {0};
		add_all(buff, block.ninodes, &offset);
	}
	char buff[100];

	if (IS_IDEINFO(ip))
		ideinfo_handler(buff);

	if (IS_FILESTAT(ip))
		filestat_handler(buff);

	if (IS_INODEINFO(ip))
		inodeinfo_handler(buff);

    return 0;
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