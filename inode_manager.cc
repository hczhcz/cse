#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk() {
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(uint32_t id, char *buf) {
  if (id < BLOCK_NUM && buf) {
    memcpy(buf, blocks[id], BLOCK_SIZE);
  }
}

void disk::write_block(uint32_t id, const char *buf) {
  if (id < BLOCK_NUM && buf) {
    memcpy(blocks[id], buf, BLOCK_SIZE);
  }
}

template <class T>
diskcache<T>::diskcache(block_manager *to_bm, uint32_t to_id, bool read, bool write) {
  to_bm->direct_access(this);
  id = to_id;

  if (read) {
    d->read_block(id, buf);
  }
  do_write = write;
}

template <class T>
diskcache<T>::diskcache(disk *to_d, uint32_t to_id, bool read, bool write) {
  d = to_d;
  id = to_id;

  if (read) {
    d->read_block(id, buf);
  }
  do_write = write;
}

template <class T>
diskcache<T>::~diskcache() {
  if (do_write) {
    d->write_block(id, buf);
  }
}

// block layer -----------------------------------------

void block_manager::lock_block(uint32_t id) {
  diskcache<struct superblock> sb(d, 0, true, true);

  if (id >= sb->nblocks) {
    return;
  }

  uint32_t g = U32MAP_GLOBAL(id);
  uint32_t l = U32MAP_LOCAL(id);
  uint32_t p = U32MAP_POS(id);

  diskcache<struct mapblock> mb(d, g + 1, true, true);

  // set 0
  mb->map[l] &= ~(1 << p);

  if (!mb->map[l]) {
    uint32_t l_l = U32MAP_LOCAL(l);
    uint32_t l_p = U32MAP_POS(l);

    sb->metamap[g][l_l] &= ~(1 << l_p);
  }
}

void block_manager::unlock_block(uint32_t id) {
  diskcache<struct superblock> sb(d, 0, true, true);

  if (id >= sb->nblocks) {
    return;
  }

  uint32_t g = U32MAP_GLOBAL(id);
  uint32_t l = U32MAP_LOCAL(id);
  uint32_t p = U32MAP_POS(id);

  diskcache<struct mapblock> mb(d, g + 1, true, true);

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

uint32_t block_manager::pick_free_block() {
  diskcache<struct superblock> sb(d, 0, true, true);

  uint32_t g = sb->metamap_g;
  do {
    uint32_t l_l = sb->metamap_l_l;
    do {
      if (sb->metamap[g][l_l]) {
        uint32_t l_p = de_bruijn_pos(sb->metamap[g][l_l]);
        uint32_t l = U32MAP(0, l_l, l_p);

        diskcache<struct mapblock> mb(d, g + 1, true, true);

        uint32_t p = de_bruijn_pos(mb->map[l]);

        // set 0
        mb->map[l] &= ~(1 << p);

        if (!mb->map[l]) {
          sb->metamap[g][l_l] &= ~(1 << l_p);
        }

        sb->metamap_g = g;
        sb->metamap_l_l = l_l;

        return U32MAP(g, l, p);
      }

      l_l = (l_l + 1) % (U32MAP_TOTAL / 32);
    } while (l_l != sb->metamap_l_l);
    g = (g + 1) % sb->nmaps;
  } while (g != sb->metamap_g);

  return 0; // disk is full
}

// Allocate a free disk block.
uint32_t block_manager::alloc_block(bool check) {
  uint32_t id = pick_free_block();

  if (check && !id) {
    throw 1; // ?
  }

  return id;
}

void block_manager::free_block(uint32_t id) {
  unlock_block(id);
}

block_manager::block_manager() {
  d = new disk();

  diskcache<struct superblock> sb(d, 0, false, true);

  // format the disk
  sb->size = BLOCK_SIZE * BLOCK_NUM;
  sb->nblocks = BLOCK_NUM;
  sb->nmaps = MAP_NUM;

  sb->metamap_g = 0;
  sb->metamap_l_l = 0;

  for (uint32_t g = 0; g < sb->nmaps; ++g) {
    diskcache<struct mapblock> mb(d, g + 1, false, true);

    for (uint32_t l_l = 0; l_l < U32MAP_TOTAL / 32; ++l_l) {
      for (uint32_t l_p = 0; l_p < 32; ++l_p) {
        mb->map[U32MAP(0, l_l, l_p)] = U32FILL;
      }
      sb->metamap[g][l_l] = U32FILL;
    }
  }

  // lock sb and mb
  diskcache<struct mapblock> mmb(d, 1, true, true);
  mmb->map[0] &= ~((1 << (1 + sb->nmaps)) - 1);
}

void block_manager::read_block(uint32_t id, char *buf) {
  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf) {
  d->write_block(id, buf);
}

template <class T>
void block_manager::direct_access(diskcache<T> *dc) {
  dc->d = d;
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
  bm = new block_manager();
  root_id = alloc_inode(extent_protocol::T_DIR);
}

uint32_t inode_manager::alloc_inum(uint32_t block_id) {
  return block_id - 8;
}

uint32_t inode_manager::addr_inum(uint32_t inum) {
  // TODO: is inum valid?

  return inum + 8;
}

void inode_manager::free_inum(uint32_t inum) {
  //
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type) {
  uint32_t inum = alloc_inum(bm->alloc_block());
  diskcache<struct inode> ni(bm, addr_inum(inum), false, true);

  ni->njnode = 0;
  ni->nknode = 0;

  ni->attr.type = type;
  ni->attr.atime = time(0);
  ni->attr.mtime = time(0);
  ni->attr.ctime = time(0);
  ni->attr.size = 0;

  return inum;
}

void inode_manager::free_inode(uint32_t inum) {
  // TODO: all arguments should be checked!!!

  bm->free_block(addr_inum(inum));
  free_inum(inum);
}


/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size) {
  diskcache<struct inode> ni(bm, addr_inum(inum), true, true);

  ni->attr.atime = time(0);
  *size = ni->attr.size;
  char *begin = (char *) malloc(ni->attr.size);
  char *end = begin + ni->attr.size;
  *buf_out = begin;

  memcpy(begin, ni->data, NDATA_MIXED_TRUNC(end - begin));
  begin += NDATA_MIXED;

  uint32_t j = 0;
  uint32_t k = 0;
  for (; j < ni->njnode; ++j) {
    diskcache<struct inode> nj(bm, ni->map[j], true, false);

    memcpy(begin, nj->data, NDATA_MIXED_TRUNC(end - begin));
    begin += NDATA_MIXED;

    for (; k < ni->nknode && k < (ni->njnode + 1) * NMAP_J; ++k) {
      diskcache<struct inode> nk(bm, nj->map[k % NMAP_J], true, false);

      memcpy(begin, nk->data, NDATA_FULL_TRUNC(end - begin));
      begin += NDATA_FULL;
    }
  }
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size) {
  if (!buf) {
    return;
  }

  diskcache<struct inode> ni(bm, addr_inum(inum), true, true);

  ni->attr.atime = time(0);
  ni->attr.mtime = time(0);
  ni->attr.size = size;
  const char *begin = buf;
  const char *end = begin + size;

  memcpy(ni->data, begin, NDATA_MIXED_TRUNC(end - begin));
  begin += NDATA_MIXED;

  uint32_t j = 0;
  uint32_t k = 0;
  uint32_t jdel = 0;
  uint32_t kdel = 0;
  for (; j < ni->njnode; ++j) {
    if (begin < end) {
      diskcache<struct inode> nj(bm, ni->map[j], true, true);

      memcpy(nj->data, begin, NDATA_MIXED_TRUNC(end - begin));
      begin += NDATA_MIXED;

      for (; k < ni->nknode && k < (ni->njnode + 1) * NMAP_J; ++k) {
        if (begin < end) {
          diskcache<struct inode> nk(bm, nj->map[k % NMAP_J], true, true);

          memcpy(nk->data, begin, NDATA_FULL_TRUNC(end - begin));
          begin += NDATA_FULL;

          if (k == ni->nknode - 1) {
            if (k + 1 == (j + 1) * NMAP_J) {
              ni->map[j + 1] = bm->alloc_block();
              ++(ni->njnode);
            } else {
              if (j + 1 == NMAP_I) {
                throw 2; // ?
              } else {
                nj->map[(k + 1) % NMAP_J] = bm->alloc_block();
                ++(ni->nknode);
              }
            }
          }
        } else {
          ++kdel;
          bm->free_block(nj->map[k % NMAP_J]);
        }
      }
    } else {
      diskcache<struct inode> nj(bm, ni->map[j], true, false);

      for (; k < ni->nknode && k < (ni->njnode + 1) * NMAP_J; ++k) {
        ++kdel;
        bm->free_block(nj->map[k % NMAP_J]);
      }

      ++jdel;
      bm->free_block(ni->map[j]);
    }
  }

  ni->njnode -= jdel;
  ni->nknode -= kdel;
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a) {
  diskcache<struct inode> ni(bm, addr_inum(inum), true, false);

  a = ni->attr;
}

void inode_manager::remove_file(uint32_t inum) {
  diskcache<struct inode> ni(bm, addr_inum(inum), true, false);

  uint32_t j = 0;
  uint32_t k = 0;
  for (; j < ni->njnode; ++j) {
    diskcache<struct inode> nj(bm, ni->map[j], true, false);

    for (; k < ni->nknode && k < (ni->njnode + 1) * NMAP_J; ++k) {
      bm->free_block(nj->map[k % NMAP_J]);
    }

    bm->free_block(ni->map[j]);
  }

  free_inode(inum);
}
