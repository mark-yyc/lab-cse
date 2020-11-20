#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if(id<0 || id>BLOCK_NUM){
    // printf("\tbm: id out of range\n");
    return;
  }
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if(id<0 || id>BLOCK_NUM){
    // printf("\tbm: id out of range\n");
    return;
  }
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
  //  */
  // for(int i = IBLOCK(INODE_NUM, sb.nblocks) + 1; i < BLOCK_NUM; i++)
  //   if(!using_blocks[i]){
  //     using_blocks[i] = 1;
  //     return i;
  //   }

  if(head->next!=NULL){
    freeNode *p=head->next;
    freeNode *q=p->next;
    int id=p->block_id;
    using_blocks[id]=1;
    delete p;
    head->next=q;
    return id;
  }

  // printf("\tbm: no available blocks\n");
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = 0;
  freeNode *p=head->next;
  freeNode *q=new freeNode(id);
  head->next=q;
  q->next=p;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

  // for(uint i = 0; i <=IBLOCK(INODE_NUM, sb.nblocks); i++){
  //   using_blocks[i] = 1;
  // }

  head=new freeNode();
  freeNode *rear=head;
  for (uint i=IBLOCK(INODE_NUM, sb.nblocks)+1;i<sb.nblocks;i++){
    freeNode *tmp=new freeNode(i);
    rear->next=tmp;
    rear=tmp; 
  }
}

block_manager::~block_manager(){
  freeNode *p=head,*q=head;
  while(p!=NULL){
    q=p->next;
    delete p;
    p=q;
  }
  head=NULL;
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  // uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  inode_t *ino = (inode_t *)malloc(sizeof(inode_t));
  bzero(ino, sizeof(inode_t));
  ino->type = extent_protocol::T_DIR;
  ino->atime = time(NULL);
  ino->mtime = time(NULL);
  ino->ctime = time(NULL);
  put_inode(1, ino);
  free(ino);
  // if (root_dir != 1) {
  //   // printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
  //   exit(0);
  // }

  head=new freeNode();
  freeNode *rear=head;
  for (uint32_t i=2;i<INODE_NUM;i++){
    freeNode *tmp=new freeNode(i);
    rear->next=tmp;
    rear=tmp; 
  }
}

inode_manager::~inode_manager(){
  freeNode *p=head,*q=head;
  while(p!=NULL){
    q=p->next;
    delete p;
    p=q;
  }
  head=NULL;
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  // static int inum = 0;

  // for(int i = 0; i < INODE_NUM; i++){
  //   inum = (inum + 1) % INODE_NUM;
  //   if(inum==0){
  //     inum++;
  //   }
  //   inode_t *ino = get_inode(inum);
  //   if(!ino){
  //     printf("alloc inum: %d\n",inum);
  //     ino = (inode_t *)malloc(sizeof(inode_t));
  //     bzero(ino, sizeof(inode_t));
  //     ino->type = type;
  //     ino->atime = time(NULL);
  //     ino->mtime = time(NULL);
  //     ino->ctime = time(NULL);
  //     put_inode(inum, ino);
  //     free(ino);
  //     break;
  //   }
  //   free(ino);
  // }

  uint32_t inum=0;
  if(head->next!=NULL){
    freeNode *p=head->next;
    freeNode *q=p->next;
    inum=p->inum;
    // printf("alloc inum: %d\n",inum);
    // inode_t *ino = get_inode(inum);
    inode_t *ino = (inode_t *)malloc(sizeof(inode_t));
    bzero(ino, sizeof(inode_t));
    ino->type = type;
    ino->atime = time(NULL);
    ino->mtime = time(NULL);
    ino->ctime = time(NULL);
    put_inode(inum, ino);
    free(ino);
    delete p;
    head->next=q;
  }
  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode_t *ino = get_inode(inum);
  if(ino){
    ino->type = 0;
    put_inode(inum, ino);
    free(ino);

    freeNode *tmp=new freeNode(inum);
    tmp->next=head->next;
    head->next=tmp;
  }
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  // printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    // printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    // printf("\tim: inode not exist\n");
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

  // printf("\tim: put_inode %d\n", inum);
  assert(ino);

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

blockid_t
inode_manager::get_nth_blockid(inode_t *ino, uint32_t n)
{
  /*get blockid given inode and n for blocks[]*/
  char buf[BLOCK_SIZE];
  blockid_t id;

  if(!ino){
    // printf("\tim: ino not found\n");
    exit(0);
  }
  /*without using indirect block*/
  if(n < NDIRECT)
    id = ino->blocks[n];

  /*using indirect block*/
  else if(n < MAXFILE){
    bm->read_block(ino->blocks[NDIRECT], buf);
    id = ((blockid_t *)buf)[n-NDIRECT];
  }else{
    // printf("\tim: get_nth_blockid out of range\n");
    exit(0);
  }

  return id;
}

void 
inode_manager::alloc_nth_block(inode_t *ino, uint32_t n)
{
  char buf[BLOCK_SIZE];
  if(!ino){
    // printf("\tim: ino not found\n");
    return;
  }

  /*without using indirect block*/
  if(n < NDIRECT)
    ino->blocks[n] = bm->alloc_block();

  /*using indirect block*/
  else if(n < MAXFILE){
    if(!ino->blocks[NDIRECT]){
      // printf("\tim: alloc_nth_block new NINDIRECT!\n");
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    bm->read_block(ino->blocks[NDIRECT], buf);      
    ((blockid_t *)buf)[n-NDIRECT] = bm->alloc_block();
    bm->write_block(ino->blocks[NDIRECT], buf); 
  }
  
  else{
    // printf("\tim: alloc_nth_block out of range\n");
    return;
  }
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  int block_num = 0;
  int remain_size = 0;
  char buf[BLOCK_SIZE];
  int i = 0;
  // printf("\tim: read_file %d\n", inum);
  inode_t *ino = get_inode(inum);
  if(!ino){
    // printf("\tim: inode not found\n");
    return;
  }
  if(ino->size>MAXFILE*BLOCK_SIZE){
    // printf("\tim: size overflow\n");
    free(ino);
    return;
  }
  *size = ino->size;
  *buf_out = (char *)malloc(*size);

  block_num = *size / BLOCK_SIZE;
  remain_size = *size % BLOCK_SIZE;

  for(; i < block_num; i++){
    bm->read_block(get_nth_blockid(ino, i), buf);
    memcpy(*buf_out + i*BLOCK_SIZE, buf, BLOCK_SIZE);
  }  
  if(remain_size){
    bm->read_block(get_nth_blockid(ino, i), buf);
    memcpy(*buf_out + i*BLOCK_SIZE, buf, remain_size);
  }
  ino->atime=time(NULL);
  put_inode(inum,ino);
  free(ino);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  int block_num = 0;
  int remain_size = 0;
  int old_blocks, new_blocks;
  char temp[BLOCK_SIZE];
  int i = 0;

  // printf("\tim: write_file %d of size %d\n", inum, size);
  inode_t *ino = get_inode(inum);

  if(ino){
    assert((unsigned int)size <= MAXFILE * BLOCK_SIZE);
    assert(ino->size <= MAXFILE * BLOCK_SIZE);

    old_blocks = ino->size == 0? 0 : ino->size/BLOCK_SIZE+1;
    new_blocks = size == 0? 0 : size/BLOCK_SIZE+1;

    /*size of old blocks < size of new blocks*/  
    if(old_blocks < new_blocks)
      for(int j = old_blocks; j < new_blocks; j++)
        alloc_nth_block(ino, j);

    /*size of old blocks > size of new blocks*/ 
    else if(old_blocks > new_blocks)
      for(int j = new_blocks; j < old_blocks; j++)
        bm->free_block(get_nth_blockid(ino, j));

    block_num = size / BLOCK_SIZE;
    remain_size = size % BLOCK_SIZE;

    for(; i < block_num; i++)
      bm->write_block(get_nth_blockid(ino, i), buf + i*BLOCK_SIZE);

    if(remain_size){
      memcpy(temp, buf + i*BLOCK_SIZE, remain_size);
      bm->write_block(get_nth_blockid(ino, i), temp);
    }

    ino->size = size;
    ino->atime = time(NULL);
    ino->mtime = time(NULL);
    ino->ctime = time(NULL);
    put_inode(inum, ino);
    free(ino);
  }
  else{
    // printf("\tim: inode not found\n");
  }
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t *ino = get_inode(inum);
  if(!ino)
    return;
  a.type = ino->type;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;
  a.size = ino->size;
  free(ino);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  inode_t *ino = get_inode(inum);
  if(!ino){
    // printf("\tim: inode not found\n");
    return;
  }

  int block_num = ino->size == 0? 0 : ino->size/BLOCK_SIZE+1;
  
  for(int i = 0; i < block_num; i++)
    bm->free_block(get_nth_blockid(ino, i));
  if(block_num > NDIRECT)
    bm->free_block(ino->blocks[NDIRECT]);
  bzero(ino, sizeof(inode_t));

  free_inode(inum);
  free(ino);
  return;
}
