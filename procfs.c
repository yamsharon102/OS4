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

#define BUFFER_SIZE 2048

int my_minor = 0;
enum my_inum {PROC, IDEINFO, FILESTAT, INODEINFO}; 

int 
procfsisdir(struct inode *ip) {
  return 0;
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	ip->major = PROCFS;
	ip->type = T_DEV;
	ip->valid = 1;
	ip->minor = my_minor++; //???
}

void
add_all(char *buff){

	dir.inum = inum;
	memmove(&dir.name, ".", strlen(dirName)+1);
 	memmove(buff + 0*sizeof(dir) , (void*)&dir, sizeof(dir));
}


int
procfsread(struct inode *ip, char *dst, int off, int n) {
    //HERE
    struct superblock block;
  	readsb(ip->dev, &block);
	if (ip->inum < block.ninodes)
	{
		char buf[BUFFER_SIZE] = {0};
		add_all(&buf);
	}
    return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n)
{
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
