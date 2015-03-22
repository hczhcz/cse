// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h"

#define DISK_SIZE         1024*1024*16
#define BLOCK_SIZE        512

#define BLOCK_NUM         (DISK_SIZE / BLOCK_SIZE)
#define MAP_NUM           (BLOCK_NUM / BLOCK_SIZE / 8)

#define U32MAP_TOTAL      (BLOCK_SIZE / 4)
#define U32MAP_GLOBAL(i)  ((i) / 32 / U32MAP_TOTAL)
#define U32MAP_LOCAL(i)   ((i) / 32 % U32MAP_TOTAL)
#define U32MAP_POS(i)     ((i) % 32)
#define U32MAP(g, l, p)   ((g) * U32MAP_TOTAL * 32 + (l) * 32 + (p))
#define U32FILL           0xFFFFFFFF


// disk layer -----------------------------------------

class disk {
 private:
  char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

template <class T>
class diskcache {
 private:
  disk *d;
  uint32_t id;

  char buf[BLOCK_SIZE];

 public:
  diskcache(disk *to_d, uint32_t to_id);
  ~diskcache();

  inline T *operator->() {
    return ((T *) &buf);
  }
};

// block layer -----------------------------------------

struct superblock {
  uint32_t size;
  uint32_t nblocks;
  uint32_t ninodes;
  uint32_t nmaps;
  uint32_t metamap_g;
  uint32_t metamap_l_l;
  uint32_t metamap[MAP_NUM][U32MAP_TOTAL / 32];
};

struct mapblock {
  uint32_t map[U32MAP_TOTAL];
};

class block_manager {
 private:
  disk *d;
  std::map <uint32_t, int> using_blocks;

  void lock_block(uint32_t id);
  void unlock_block(uint32_t id);
  uint32_t pick_free_block();

 public:
  block_manager();

  uint32_t alloc_block();
  void free_block(uint32_t id);
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM  1024

// Inodes per block.
#define IPB           1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks)     0 // ((nblocks)/BPB + (i)/IPB + 3)

// Bitmap bits per block
#define BPB           (BLOCK_SIZE*8)

// Block containing bit for block b
#define BBLOCK(b) ((b)/BPB + 2)

#define NDIRECT 32
#define NINDIRECT (BLOCK_SIZE / 4)
#define MAXFILE (NDIRECT + NINDIRECT)

struct inode {
  //short type;
  unsigned int type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  uint32_t blocks[NDIRECT];   // Data block addresses
};

class inode_manager {
 private:
  block_manager *bm;
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);

 public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif

