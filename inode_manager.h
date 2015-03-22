// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h"

#define DISK_SIZE         1024*1024*16
#define BLOCK_SIZE        512

// if BLOCK_NUM != BLOCK_SIZE * 8 * C, the non-existent blocks should be "locked"
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

class block_manager;

template <class T>
class diskcache {
 private:
  disk *d;
  uint32_t id;
  bool do_write;

  char buf[BLOCK_SIZE];

 public:
  friend class block_manager;

  diskcache(block_manager *to_bm, uint32_t to_id, bool read, bool write);
  diskcache(disk *to_d, uint32_t to_id, bool read, bool write);
  ~diskcache();

  inline T *operator->() {
    return ((T *) &buf);
  }
};

// block layer -----------------------------------------

struct superblock {
  uint32_t size;
  uint32_t nblocks;
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
  template <class T>
  void direct_access(diskcache<T> *dc);
};

// inode layer -----------------------------------------

#define NMAP_I (U32MAP_TOTAL / 4)
#define NMAP_J (U32MAP_TOTAL / 2)
#define NDATA_MIXED (BLOCK_SIZE / 2)
#define NDATA_FULL BLOCK_SIZE

struct inode {
  uint32_t map[NMAP_I];
  uint32_t njnode;
  uint32_t nknode;
  extent_protocol::attr attr;
  char data[NDATA_MIXED];
};

struct jnode {
  uint32_t map[NMAP_J];
  char data[NDATA_MIXED];
};

struct knode {
  char data[NDATA_FULL];
};

class inode_manager {
 private:
  block_manager *bm;

  uint32_t alloc_inum(uint32_t block_id);
  uint32_t addr_inum(uint32_t inum);
  void free_inum(uint32_t inum);

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
