#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk() {
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf) {
  if (id >= 0 && id < BLOCK_NUM && buf) {
    memcpy(buf, blocks[id], BLOCK_SIZE);
  }
}

void disk::write_block(blockid_t id, const char *buf) {
  if (id >= 0 && id < BLOCK_NUM && buf) {
    memcpy(blocks[id], buf, BLOCK_SIZE);
  }
}

template <class T>
diskcache<T>::diskcache(disk *to_d, uint32_t to_id) {
  d = to_d;
  id = to_id;

  d->read_block(id, buf);
}

template <class T>
diskcache<T>::~diskcache() {
  d->write_block(id, buf);
}

// block layer -----------------------------------------

void block_manager::lock_block(uint32_t id) {
  diskcache<struct superblock> sb(d, 0);

  uint32_t g = U32MAP_GLOBAL(id);
  uint32_t l = U32MAP_LOCAL(id);
  uint32_t p = U32MAP_POS(id);

  diskcache<struct mapblock> mb(d, g + 1);

  // set 0
  mb->map[l] &= ~(1 << p);

  if (!mb->map[l]) {
    uint32_t l_l = U32MAP_LOCAL(l);
    uint32_t l_p = U32MAP_POS(l);

    sb->metamap[g][l_l] &= ~(1 << l_p);
  }
}

void block_manager::unlock_block(uint32_t id) {
  diskcache<struct superblock> sb(d, 0);

  uint32_t g = U32MAP_GLOBAL(id);
  uint32_t l = U32MAP_LOCAL(id);
  uint32_t p = U32MAP_POS(id);

  diskcache<struct mapblock> mb(d, g + 1);

  // set 1
  mb->map[l] |= 1 << p;

  if (mb->map[l]) {
    uint32_t l_l = U32MAP_LOCAL(l);
    uint32_t l_p = U32MAP_POS(l);

    sb->metamap[g][l_l] |= 1 << l_p;
  }
}

uint32_t de_bruijn_pos(uint32_t value) {
  static const uint32_t map[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
  };

  static const uint32_t dnum = 0x077CB531;

  return map[uint32_t((value & -value) * dnum) >> 27];
}

blockid_t block_manager::pick_free_block() {
  diskcache<struct superblock> sb(d, 0);

  uint32_t g = sb->metamap_g;
  do {
    uint32_t l_l = sb->metamap_l_l;
    do {
      if (sb->metamap[g][l_l]) {
        uint32_t l_p = de_bruijn_pos(sb->metamap[g][l_l]);
        uint32_t l = U32MAP(0, l_l, l_p);

        diskcache<struct mapblock> mb(d, g + 1);

        uint32_t p = de_bruijn_pos(mb->map[l]);

        // set 0
        mb->map[l] &= ~(1 << p);
        sb->metamap[g][l_l] &= ~(1 << l_p);

        sb->metamap_g = g;
        sb->metamap_l_l = l_l;

        return U32MAP(g, l, p);
      }

      l_l = (l_l + 1) % (U32MAP_TOTAL / 32);
    } while (l_l != sb->metamap_l_l);
    g = (g + 1) % sb->nmaps;
  } while (g != sb->metamap_g);

  return -1; // disk is full
}

// Allocate a free disk block.
blockid_t block_manager::alloc_block() {
  return pick_free_block();
}

void block_manager::free_block(uint32_t id) {
  unlock_block(id);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() {
  d = new disk();

  diskcache<struct superblock> sb(d, 0);

  // format the disk
  sb->size = BLOCK_SIZE * BLOCK_NUM;
  sb->nblocks = BLOCK_NUM;
  sb->ninodes = INODE_NUM;
  sb->nmaps = MAP_NUM;

  sb->metamap_g = 0;
  sb->metamap_l_l = 0;

  for (uint32_t g = 0; g < sb->nmaps; ++g) {
    diskcache<struct mapblock> mb(d, g + 1);

    for (uint32_t l_l = 0; l_l < U32MAP_TOTAL / 32; ++l_l) {
      for (uint32_t l_p = 0; l_p < 32; ++l_p) {
        mb->map[U32MAP(0, l_l, l_p)] = U32FILL;
      }
      sb->metamap[g][l_l] = U32FILL;
    }
  }
}

void block_manager::read_block(uint32_t id, char *buf) {
  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf) {
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */
  return 1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
}
