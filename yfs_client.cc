// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        return RPCERR; \
    } \
} while (0)

int strhash(std::string s) {
    int v = 1234567890;

    for (size_t i = 0; i < s.size(); ++i) {
        v ^= ((v << 16) + (v << 3) + s[i] * 2333333333 + (v >> 3) + (v >> 16));
    }

    return v;
}

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK) {
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__);
    };
}

bool
yfs_client::isdir(inum inum)
{
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    } 

    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 

    printf("isfile: %lld is not a file\n", inum);
    return false;
}

bool
yfs_client::islink(inum inum)
{
    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("islink: %lld is a symlink\n", inum);
        return true;
    } 

    printf("islink: %lld is not a symlink\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    printf("getfile %016llx\n", inum);

    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    return OK;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    printf("getdir %016llx\n", inum);

    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.mtime;

    return OK;
}

int
yfs_client::getlink(inum inum, linkinfo &lin)
{
    printf("getlink %016llx\n", inum);

    extent_protocol::attr a;
    EXT_RPC(ec->getattr(inum, a));

    lin.atime = a.atime;
    lin.mtime = a.mtime;
    lin.ctime = a.mtime;

    return OK;
}

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    std::string file_data;
    EXT_RPC(ec->get(ino, file_data));

    file_data.resize(size);

    EXT_RPC(ec->put(ino, file_data));

    return OK;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    bool exist = false;

    // lookup

    int st = lookup(parent, name, exist, ino_out);
    if (st != OK) {
        return st;
    }
    if (exist) {
        return EXIST;
    }

    // create

    EXT_RPC(ec->create(extent_protocol::T_FILE, ino_out));

    // modify parent

    std::string dir_info;
    EXT_RPC(ec->get(parent, dir_info));

    std::string file_name(name);

    struct direntraw dr;
    dr.inum = ino_out;
    dr.name_length = file_name.size();
    dr.name_hash = strhash(file_name);

    dir_info.append((char *) &dr, sizeof(struct direntraw));
    dir_info.append(file_name);

    EXT_RPC(ec->put(parent, dir_info));

    return OK;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    bool exist = false;

    // lookup

    int st = lookup(parent, name, exist, ino_out);
    if (st != OK) {
        return st;
    }
    if (exist) {
        return EXIST;
    }

    // create

    EXT_RPC(ec->create(extent_protocol::T_DIR, ino_out));

    // modify parent

    std::string dir_info;
    EXT_RPC(ec->get(parent, dir_info));

    std::string file_name(name);

    struct direntraw dr;
    dr.inum = ino_out;
    dr.name_length = file_name.size();
    dr.name_hash = strhash(file_name);

    dir_info.append((char *) &dr, sizeof(struct direntraw));
    dir_info.append(file_name);

    EXT_RPC(ec->put(parent, dir_info));

    return OK;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    std::string dir_info;
    EXT_RPC(ec->get(parent, dir_info));

    std::string file_name(name);

    int name_length = file_name.size();
    int name_hash = strhash(file_name);

    size_t i = 0;
    while (i < dir_info.size()) {
        std::string ent = dir_info.substr(i, sizeof(struct direntraw));
        struct direntraw dr = *((struct direntraw *) ent.data());
        i += sizeof(struct direntraw);

        if (name_length == dr.name_length && name_hash == dr.name_hash) {
            std::string ent_name = dir_info.substr(i, dr.name_length);
            if (ent_name == file_name) {
                ino_out = dr.inum;
                found = true;
                return OK;
            }
        }
        i += dr.name_length;
    }

    found = false;
    return OK;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    std::string dir_info;
    EXT_RPC(ec->get(dir, dir_info));

    size_t i = 0;
    while (i < dir_info.size()) {
        std::string ent = dir_info.substr(i, sizeof(struct direntraw));
        struct direntraw dr = *((struct direntraw *) ent.data());
        i += sizeof(struct direntraw);

        struct dirent de;
        de.inum = dr.inum;
        de.name = dir_info.substr(i, dr.name_length);
        list.push_back(de);
        i += dr.name_length;
    }

    return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    std::string file_data;
    EXT_RPC(ec->get(ino, file_data));

    data = file_data.substr(off, size);

    return OK;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    std::string file_data;
    std::string new_file_data;
    EXT_RPC(ec->get(ino, file_data));

    if (off + size > file_data.size()) {
        file_data.resize(off + size, 0);
    }

    // notice: naive implementation
    new_file_data = file_data.substr(0, off);
    new_file_data.append(data, size);
    new_file_data.append(file_data.substr(
        off + size,
        file_data.size() - off - size
    ));

    EXT_RPC(ec->put(ino, new_file_data));

    return OK;
}

int yfs_client::unlink(inum parent, const char *name)
{
    std::string dir_info;
    EXT_RPC(ec->get(parent, dir_info));

    std::string file_name(name);

    int name_length = file_name.size();
    int name_hash = strhash(file_name);

    size_t i = 0;
    while (i < dir_info.size()) {
        std::string ent = dir_info.substr(i, sizeof(struct direntraw));
        struct direntraw dr = *((struct direntraw *) ent.data());
        i += sizeof(struct direntraw);

        if (name_length == dr.name_length && name_hash == dr.name_hash) {
            std::string ent_name = dir_info.substr(i, dr.name_length);
            if (ent_name == file_name) {
                // notice: naive implementation
                std::string new_dir_info;

                new_dir_info = dir_info.substr(0, i - sizeof(struct direntraw));
                new_dir_info.append(dir_info.substr(
                    i + dr.name_length,
                    dir_info.size() - i - dr.name_length
                ));

                EXT_RPC(ec->remove(dr.inum));
                EXT_RPC(ec->put(parent, new_dir_info));
                return OK;
            }
        }
        i += dr.name_length;
    }

    EXT_RPC(ec->put(parent, dir_info));
    return NOENT;
}

int yfs_client::mklink(inum parent, const char *name, const char *link, inum &ino_out) {
    bool exist = false;

    // lookup

    int st = lookup(parent, name, exist, ino_out);
    if (st != OK) {
        return st;
    }
    if (exist) {
        return EXIST;
    }

    // create

    EXT_RPC(ec->create(extent_protocol::T_SYMLINK, ino_out));
    EXT_RPC(ec->put(ino_out, link));

    // modify parent

    std::string dir_info;
    EXT_RPC(ec->get(parent, dir_info));

    std::string file_name(name);

    struct direntraw dr;
    dr.inum = ino_out;
    dr.name_length = file_name.size();
    dr.name_hash = strhash(file_name);

    dir_info.append((char *) &dr, sizeof(struct direntraw));
    dir_info.append(file_name);

    EXT_RPC(ec->put(parent, dir_info));

    return OK;
}

int yfs_client::readlink(inum ino, std::string &data) {
    EXT_RPC(ec->get(ino, data));

    return OK;
}
