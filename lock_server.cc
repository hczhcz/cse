// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  printf("stat request from clt %d\n", clt);
  r = nacquire;

  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&mutex);

  if (lock.find(lid) == lock.end()) {
    lock[lid] = clt;
    ++nacquire;
  } else if (lock[lid] == clt) {
    // ignore?
    ret = lock_protocol::RETRY;
  } else {
    ret = lock_protocol::RETRY;
  }

  pthread_mutex_unlock(&mutex);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&mutex);

  if (lock.find(lid) == lock.end()) {
    // ret = lock_protocol::NOENT;
  } else if (lock[lid] == clt) {
    lock.erase(lid);
    --nacquire;
  } else {
    lock.erase(lid);
    --nacquire;
    // ret = lock_protocol::IOERR;
  }

  pthread_mutex_unlock(&mutex);

  return ret;
}
