#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct linkinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct direntraw {
    yfs_client::inum inum;
    int name_length;
    int name_hash;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 public:
  yfs_client(std::string, std::string);

  bool isdir(inum);
  bool isfile(inum);
  bool islink(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getlink(inum, linkinfo &);

  int lookup(inum, const char *, bool &, inum &);

  int create(inum, const char *, mode_t, inum &);
  int mkdir(inum, const char *, mode_t , inum &);
  int mklink(inum, const char *, const char *, inum &); // symlink

  int read(inum, size_t, off_t, std::string &);
  int readdir(inum, std::list<dirent> &);
  int readlink(inum, std::string &); // symlink

  int setattr(inum, size_t);
  int write(inum, size_t, off_t, const char *, size_t &);
  int unlink(inum, const char *);
};

#endif 
