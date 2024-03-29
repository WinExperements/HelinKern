#include <typedefs.h>
#include <vfs.h>

#include "ext2/ext2.h"

#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <elf.h>
//__attribute__((section(".modname"))) char *name = "ext2";

/*
 * The original code belongs to:
 * https://github.com/SamyPesse/How-to-Make-a-Computer-Operating-System/blob/master/src/kernel/modules/ext2.cc
 * https://github.com/klange/toaruos/blob/toaru-1.x/modules/ext2.c
 * Thanks for all there projects for it's drivers code
*/

struct dirent *ext2_readdir(struct vfs_node *dir,uint32_t index);

static void read(vfs_node_t *node, int pos, void *buffer, uint32_t sizee) {
    vfs_read(node,pos,sizee,buffer);
}

static bool check_ext2(vfs_node_t *disk) {
	struct ext2_super_block *sb = (struct ext2_super_block *)kmalloc(sizeof(struct ext2_super_block));
    	read(disk,1024,sb,1024);
	if (sb->s_magic != 0xEF53) {
		kprintf("%s: EXT2 magic invalid, 0x%x\n",__func__,sb->s_magic);
		kfree(sb);
		return false;
	}
	kfree(sb);
	return true;
}


static void read_sb(vfs_node_t *node,struct ext2_super_block *sb) {
	if (!sb) return;
	read(node,1024,sb,1024);
}

static void read_gr(vfs_node_t *node,struct ext2_group_desc *desc,struct ext2_disk *info) {
	if (!node || !desc) return;
	uint32_t offset = ((info->blocksize == 1024) ? 2048 : info->blocksize);
	int gd_size = (info->groups * ((int)sizeof(struct ext2_group_desc)));
	read(node,offset,desc,gd_size);
}

static struct ext2_disk *readInfo(vfs_node_t *dev) {
	struct ext2_disk* info=(struct ext2_disk*)kmalloc(sizeof(struct ext2_disk));
	info->sb=(struct ext2_super_block*)kmalloc(sizeof(struct ext2_super_block));
	info->dev=dev;
	int i, j;
	read_sb(dev,info->sb);
	info->blocksize = 1024 << ((info->sb)->s_log_block_size);
	i = (info->sb->s_blocks_count / info->sb->s_blocks_per_group) + ((info->sb->s_blocks_count % info->sb->s_blocks_per_group) ? 1 : 0);
	j = (info->sb->s_inodes_count / info->sb->s_inodes_per_group) + ((info->sb->s_inodes_count % info->sb->s_inodes_per_group) ? 1 : 0);
	info->groups = (i > j) ? i : j;
	int b = info->sb->s_blocks_count / info->sb->s_blocks_per_group;
	if (info->sb->s_blocks_per_group * b < info->sb->s_blocks_count) {
		b+=1;
	}
	int gd_size = info->groups * ((int)sizeof(struct ext2_group_desc));
	info->gd = (struct ext2_group_desc *) kmalloc(gd_size);
	info->inodes_per_group = info->sb->s_inodes_count / b;
	read_gr(info->dev,info->gd,info);
	return info;
}

static struct ext2_inode *ext2_read_inode(struct ext2_disk* hd, int i_num)
{
    if (!hd || i_num <= 0) {
        kprintf("Invalid parameters!\n");
        return NULL;
    }

    uint32_t group = (i_num - 1) / hd->sb->s_inodes_per_group;
    if (group >= hd->groups) {
        kprintf("Out of groups: %d!\n", group);
        return NULL;
    }

    uint32_t inta = hd->gd[group].bg_inode_table;
    uint32_t inode_index = (i_num - 1) % hd->inodes_per_group; // Use hd->inodes_per_group instead of hd->sb->s_inodes_per_group
    uint32_t block_offset = inode_index * hd->sb->s_inode_size / hd->blocksize; // Use hd->sb->s_inode_size instead of hd->sb->s_inode_size
    uint32_t offset_in_block = inode_index * hd->sb->s_inode_size % hd->blocksize; // Use hd->sb->s_inode_size instead of hd->sb->s_inode_size

    struct ext2_inode *inode = (struct ext2_inode *)kmalloc(hd->sb->s_inode_size);
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
    if (!inode) {
        kprintf("Failed to allocate memory for inode!\n");
        return NULL;
    }

    uint8_t *buf = kmalloc(hd->blocksize);
    if (!buf) {
        kprintf("Failed to allocate memory for buffer!\n");
        kfree(inode);
        return NULL;
    }
    memset(buf,0,hd->blocksize);
	kprintf("MEMSET!\r\n");
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
    //kprintf("offset in block: %d\n",offset_in_block);
    //vfs_readBlock(hd->dev, (inta + block_offset)+(hd->blocksize/512), hd->blocksize,buf);
    read(hd->dev, (inta + block_offset) * hd->blocksize, buf, hd->blocksize);
	kprintf("READED, leak\r\n");
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
    memcpy(inode, buf + offset_in_block, hd->sb->s_inode_size);
	kprintf("MEMCPY! Loeak\r\n");
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
    kfree(buf);
    return inode;
}


static void dump_ext2_inode(const struct ext2_inode* inode) {
    kprintf("i_mode: %d\n", inode->i_mode);
    kprintf("i_uid: %d\n", inode->i_uid);
    kprintf("i_size: %d\n", inode->i_size);
    kprintf("i_atime: %d\n", inode->i_atime);
    kprintf("i_ctime: %d\n", inode->i_ctime);
    kprintf("i_mtime: %d\n", inode->i_mtime);
    kprintf("i_dtime: %d\n", inode->i_dtime);
    kprintf("i_gid: %d\n", inode->i_gid);
    kprintf("i_links_count: %d\n", inode->i_links_count);
    kprintf("i_blocks: %d\n", inode->i_blocks);
    kprintf("i_flags: %d\n", inode->i_flags);
    kprintf("i_osd1: %d\n", inode->i_osd1);

    /*for (int i = 0; i < 15; i++) {
        kprintf("i_block[%d]: %d\n", i, inode->i_block[i]);
    }*/

    kprintf("i_generation: %d\n", inode->i_generation);
    kprintf("i_file_acl: %d\n", inode->i_file_acl);
    kprintf("i_dir_acl: %d\n", inode->i_dir_acl);
    kprintf("i_faddr: %d\n", inode->i_faddr);
    /*for (int i = 0; i < 12; i++) {
        kprintf("i_osd2[%d]: %d\n", i, inode->i_osd2[i]);
	}*/
}

static int ext2_is_directory(struct ext2_disk *dev,int in_id)
{
	struct ext2_inode *inod=ext2_read_inode(dev,in_id);
	int ret=(inod->i_mode & EXT2_S_IFDIR) ? 1 : 0;
	kfree(inod);
	return ret;
}

char *ext2_read_file(struct ext2_disk *hd,struct ext2_inode *inode, int size)
{
	vfs_node_t *dev=hd->dev;

	char *mmap_base, *mmap_head, *buf;

	int *p, *pp, *ppp;
	int i, j, k;
	int n;

	buf = (char *) kmalloc(hd->blocksize);
	p = (int *) kmalloc(hd->blocksize);
	pp = (int *) kmalloc(hd->blocksize);
	ppp = (int *) kmalloc(hd->blocksize);

	/* taille totale du fichier */
	mmap_head = mmap_base = (char*)kmalloc(size);
	/* direct block number */
	for (i = 0; i < 12 && inode->i_block[i]; i++) {
        read(dev,(inode->i_block[i] * hd->blocksize), buf, (hd->blocksize));
		n = ((size > (int)hd->blocksize) ? (int)hd->blocksize : size);
		memcpy(mmap_head, buf, n);
		mmap_head += n;
		size -= n;
	}
	/* indirect block number */
	if (inode->i_block[12]) {
	    read(dev,(inode->i_block[12] * hd->blocksize), p, (hd->blocksize));


		for (i = 0; i < (int)hd->blocksize / 4 && p[i]; i++) {
            read(dev,(p[i] * hd->blocksize),buf, (hd->blocksize));
			n = ((size > (int)hd->blocksize) ? (int)hd->blocksize : size);
			memcpy(mmap_head, buf, n);
			mmap_head += n;
			size -= n;
		}
	}

	/* bi-indirect block number */
	if (inode->i_block[13]) {
	    read(dev,(inode->i_block[13] * hd->blocksize), p, (hd->blocksize));

		for (i = 0; i < (int)hd->blocksize / 4 && p[i]; i++) {
            read(dev,(p[i] * (int)hd->blocksize), pp,(hd->blocksize));
			for (j = 0; j < (int)hd->blocksize / 4 && pp[j]; j++) {
                read(dev,(pp[j] * hd->blocksize),buf,(hd->blocksize));
				n = ((size > (int)hd-> blocksize) ? (int)hd->blocksize : size);
				memcpy(mmap_head, buf, n);
				mmap_head += n;
				size -= n;
			}
		}
	}
	/* tri-indirect block number */
	if (inode->i_block[14]) {
        read(dev,(inode->i_block[14] * hd->blocksize), p,(hd->blocksize));
		for (i = 0; i < (int)hd->blocksize / 4 && p[i]; i++) {
            read(dev,(p[i] * hd->blocksize), pp,(hd->blocksize));
			for (j = 0; j < (int)hd->blocksize / 4 && pp[j]; j++) {
                read(dev,(pp[j] * hd->blocksize), ppp,(hd->blocksize));
				for (k = 0; k < (int)hd->blocksize / 4 && ppp[k]; k++) {
                    read(dev,(ppp[k] * hd->blocksize), buf,(hd->blocksize));
					n = ((size > (int)hd->blocksize) ? (int)hd->blocksize : size);
					memcpy(mmap_head, buf, n);
					mmap_head += n;
					size -= n;
				}
			}
		}
	}
	kfree(buf);
	kfree(p);
	kfree(pp);
	kfree(ppp);;
	return mmap_base;
}

static void ext2_read_directory(struct ext2_disk *disk,struct ext2_inode *inode) {
    char *filename;
    struct ext2_directory_entry *dentry;
    uint32_t dsize = inode->i_size;
    void *d = ext2_read_file(disk,inode,inode->i_size);
    dentry = (struct ext2_directory_entry *)d;
    while(inode && dsize) {
        filename = (char *) kmalloc(dentry->name_len + 1);
        memcpy(filename,(char*)&(dentry->name), dentry->name_len);
        filename[dentry->name_len] = 0;
        if (strcmp(filename,".") || strcmp(filename,"..")) {
            kfree(filename);
            dsize -= dentry->rec_len;
            dentry = (struct ext2_directory_entry *) ((char *) dentry + dentry->rec_len);
            continue;
        }
        //kprintf("File name: %s, inode id: %d\n",filename,dentry->inode);
        kfree(filename);
        if (!ext2_is_directory(disk,dentry->inode)) {
            struct ext2_inode *file = ext2_read_inode(disk,dentry->inode);
            char *cont = ext2_read_file(disk,file,file->i_size);
	    elf_load_file(cont,thread_getThread(thread_getCurrent()));
           // kprintf("Reading file: %s\n",cont);
            kfree(cont);
            kfree(file);
        }
        dsize -= dentry->rec_len;
        dentry = (struct ext2_directory_entry *) ((char *) dentry + dentry->rec_len);
    }
}

static vfs_fs_t *ext2;

typedef struct ext2_priv_data {
	int inode_id;
	struct ext2_disk *disk;
} ext2_pr;

static bool ext2_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *) {
	if (dev == NULL || mountpoint == NULL) return false;
	if (!check_ext2(dev)) {
		return false;
	}
	struct ext2_disk *d = readInfo(dev);
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
	if (!ext2_is_directory(d,2)) {
		kprintf("EXT2: root inode invalid\n");
		kfree(d);
		return false;
	}
	for (int i = 0; i < 10; i++) {
		kmalloc(1024);
	}
	kprintf("10KB is leacked\r\n");
	kprintf("Last mount point: %s\n",d->sb->s_last_mounted);
	strcpy(d->sb->s_last_mounted,"/fat");
	vfs_writeBlock(dev,2,512,d->sb);
	ext2_pr *pr = (ext2_pr *)kmalloc(sizeof(ext2_pr));
	memset(pr,0,sizeof(ext2_pr));
	pr->inode_id = 2;
	pr->disk = d;
	mountpoint->fs = ext2;
	mountpoint->priv_data = pr;
	return true;
}

struct vfs_node *ext2_finddir(struct vfs_node *in,char *name) {
	ext2_pr *pr = (ext2_pr *)in->priv_data;
	struct ext2_inode *inode = ext2_read_inode(pr->disk,pr->inode_id);
	char *filename;
    	struct ext2_directory_entry *dentry;
    	uint32_t dsize = inode->i_size;
	struct ext2_disk *disk = pr->disk;
    	void *d = ext2_read_file(disk,inode,inode->i_size);
    	dentry = (struct ext2_directory_entry *)d;
    	while(inode && dsize) {
        	filename = (char *) kmalloc(dentry->name_len + 1);
        	memcpy(filename,(char*)&(dentry->name), dentry->name_len);
        	filename[dentry->name_len] = 0;
        	if (strcmp(filename,name)) {
			ext2_pr *prv = (ext2_pr *)kmalloc(sizeof(ext2_pr));
			memset(prv,0,sizeof(ext2_pr));
			prv->disk = disk;
			prv->inode_id = dentry->inode;
			struct ext2_inode *ub = ext2_read_inode(disk,prv->inode_id);
			vfs_node_t *ret = kmalloc(sizeof(vfs_node_t));
			memset(ret,0,sizeof(vfs_node_t));
			ret->name = strdup(filename);
			ret->fs = ext2;
			ret->priv_data = prv;
			ret->size = ub->i_size;
			kfree(ub);
			kfree(filename);
			return ret;
		}
		kfree(filename);
        dsize -= dentry->rec_len;
        dentry = (struct ext2_directory_entry *) ((char *) dentry + dentry->rec_len);
    }
    return NULL;
}

static struct dirent *ff = 0;

struct dirent *ext2_readdir(struct vfs_node *dir,uint32_t index) {
	ext2_pr *pr = (ext2_pr *)dir->priv_data;
        struct ext2_inode *inode = ext2_read_inode(pr->disk,pr->inode_id);
	if (!inode) {
		kprintf("%s: invalid dir\n",__func__);
		return NULL;
	}
	if (ff == NULL) {
		//kprintf("Allocation ff\n");
		ff = /*(struct dirent *)sbrk(thread_getThread(thread_getCurrent()),sizeof(struct dirent));*/kmalloc(sizeof(struct dirent));
		memset(ff,0,sizeof(struct dirent));
	}
        char *filename;
        struct ext2_directory_entry *dentry;
        uint32_t dsize = inode->i_size;
        void *d = ext2_read_file(pr->disk,inode,inode->i_size);
        dentry = (struct ext2_directory_entry *)d;
        while(inode && dsize) {             
                filename = (char *) kmalloc(dentry->name_len + 1);
                memcpy(filename,(char*)&(dentry->name), dentry->name_len);
                filename[dentry->name_len] = 0;                      
                if (index == 0) {
			//kprintf("Returning node, %s, 0x%x\n",filename,ff);
			strcpy(ff->name,filename);
			kfree(filename);
			return ff;
		}
                kfree(filename);
		index--;
        dsize -= dentry->rec_len;
        dentry = (struct ext2_directory_entry *) ((char *) dentry + dentry->rec_len);
    }
	//kprintf("Returning zero :(\n");
	if (ff != NULL) {
		kfree(ff);
		ff = NULL;
	}
	return NULL;
}

static int ext2_fs_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buff) {
	if (node == NULL || buff == NULL) return 0;
	ext2_pr *prv = (ext2_pr *)node->priv_data;
	struct ext2_inode *in = ext2_read_inode(prv->disk,prv->inode_id);
	if (in == NULL) return 0;
	void *read = ext2_read_file(prv->disk,in,size);
	memcpy(buff,read,size);
	kfree(read);
	kfree(in);
	return size;
}

void ext2_main() {
	ext2 = kmalloc(sizeof(vfs_fs_t));
	memset(ext2,0,sizeof(vfs_fs_t));
	ext2->fs_name = "ext2";
	ext2->mount = ext2_mount;
	ext2->finddir = ext2_finddir;
	ext2->readdir = ext2_readdir;
	ext2->read = ext2_fs_read;
	vfs_addFS(ext2);
	kprintf("EXT2 registred\n");
	
}
