#include "simplefs-ops.h"
extern struct filehandle_t file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

int simplefs_create(char *filename){
    /*
	    Create file with name `filename` from disk
	*/
	int exists = 0;

    for(int i=0; i<NUM_INODES; i++){
		struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
		simplefs_readInode(i, inodeptr);

		if (inodeptr->status != INODE_FREE && strcmp(inodeptr->name, filename) == 0){
			exists = 1;
			free(inodeptr);
			break;
		}
		free(inodeptr);	
	}

	if (exists == 1)
    	return -1;

	int id = simplefs_allocInode();
	if (id == -1)
		return -1;
	
	struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
	simplefs_readInode(id, inodeptr);

	strcpy(inodeptr->name, filename);
	inodeptr->status = INODE_IN_USE;
	inodeptr->file_size = 0;
	
	for (int i=0; i<MAX_FILE_SIZE; ++i){
		inodeptr->direct_blocks[i] = -1;
	}
	simplefs_writeInode(id, inodeptr);
	free(inodeptr);
	return id;

}


void simplefs_delete(char *filename){
    /*
	    delete file with name `filename` from disk
	*/


    for(int i=0; i<NUM_INODES; i++){
		struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
		simplefs_readInode(i, inodeptr);

		if (inodeptr->status != INODE_FREE && strcmp(inodeptr->name, filename) == 0){
			for (int j=0;j<MAX_FILE_SIZE; ++j){
				if (inodeptr->direct_blocks[j] != -1){
					simplefs_freeDataBlock(inodeptr->direct_blocks[j]);
				}
			}
			simplefs_freeInode(i);
			free(inodeptr);
			break;
		}
		free(inodeptr);
		
	}

}

int simplefs_open(char *filename){
    /*
	    open file with name `filename`
	*/
	int id = -1;
	for(int i=0; i<MAX_OPEN_FILES; i++){
		if (file_handle_array[i].inode_number == -1){
			id = i;
			break;
		}
	}
	if (id == -1)
    	return -1;

	for(int i=0; i<NUM_INODES; i++){
		struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
		simplefs_readInode(i, inodeptr);

		if (inodeptr->status != INODE_FREE && strcmp(inodeptr->name, filename) == 0){
			file_handle_array[id].inode_number = i;
			file_handle_array[id].offset = 0;
			
			free(inodeptr);
			return id;
		}

		free(inodeptr);
	}

	return -1;
}

void simplefs_close(int file_handle){
    /*
	    close file pointed by `file_handle`
	*/
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;

}

int simplefs_read(int file_handle, char *buf, int nbytes){
    /*
	    read `nbytes` of data into `buf` from file pointed by `file_handle` starting at current offset
	*/
	int i = file_handle_array[file_handle].inode_number;
	int offset = file_handle_array[file_handle].offset;

	struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
	simplefs_readInode(i, inodeptr);

	if (offset + nbytes > inodeptr->file_size){
		free(inodeptr);
		return -1;
	}
	int end = offset + nbytes;

	int write_ptr = 0;
	int current_offset = 0;
	int reading = 0;

	for (int i=0; i<MAX_FILE_SIZE; ++i){
		if (current_offset + BLOCKSIZE <= offset && reading == 0){
			current_offset += BLOCKSIZE;
			continue;
		}
		reading = 1;
		char temp[BLOCKSIZE];
		simplefs_readDataBlock(inodeptr->direct_blocks[i], temp);


		int cur_end;
		int finish_this_cycle = 0;

		if (end > current_offset + BLOCKSIZE){
			cur_end = BLOCKSIZE;
		}
		else {
			cur_end = end - current_offset;
			finish_this_cycle = 1;
		}

		int cur_start;

		if (current_offset >= offset){
			cur_start = 0;
		}
		else {
			cur_start = offset - current_offset;
		}

		memcpy(buf + write_ptr, temp + cur_start,  cur_end - cur_start);
		write_ptr += cur_end - cur_start;

		current_offset += BLOCKSIZE;

		if (finish_this_cycle == 1){
			break;
		}
	}
	free(inodeptr);
    return 0;
}


int simplefs_write(int file_handle, char *buf, int nbytes){
    /*
	    write `nbytes` of data from `buf` to file pointed by `file_handle` starting at current offset
	*/
	int i = file_handle_array[file_handle].inode_number;

	int offset = file_handle_array[file_handle].offset;

	struct inode_t *inodeptr = (struct inode_t*) malloc(sizeof(struct inode_t));
	simplefs_readInode(i, inodeptr);

	int old_size = inodeptr->file_size;

	int start_index = offset/BLOCKSIZE;

	if (start_index >= MAX_FILE_SIZE || offset + nbytes > MAX_FILE_SIZE*BLOCKSIZE){
		free(inodeptr);
		return -1;
	}

	if (offset + nbytes > old_size){
		inodeptr->file_size = offset + nbytes;
	}

	int rem = nbytes;
	int success = 1;
	int write_ptr = 0;

	int allocated_block_number[MAX_FILE_SIZE];
	for (int i=0; i<MAX_FILE_SIZE; ++i){
		allocated_block_number[i] = -1;
	}

	for (int i=0; i<MAX_FILE_SIZE; ++i){
		if (i*BLOCKSIZE >= inodeptr->file_size){
			break;
		}
		if (inodeptr->direct_blocks[i] == -1){
			// allocate 
			int id = simplefs_allocDataBlock();
			if (id == -1){
				success = 0;
				break;
			}
			inodeptr->direct_blocks[i] = id;
			allocated_block_number[i] = id;
		}
	}
	if (success == 0){
		for (int i=0; i<MAX_FILE_SIZE; ++i){
			if (allocated_block_number[i] != -1)
				simplefs_freeDataBlock(allocated_block_number[i]);
		}
		return -1;
	}

	for (int i=start_index; i<MAX_FILE_SIZE && rem > 0; i++){
		char temp[BLOCKSIZE];
		if (allocated_block_number[i] != -1){
			memset(temp, '0', sizeof(temp));
		}
		else {
			simplefs_readDataBlock(inodeptr->direct_blocks[i], temp);
		}
	
		int cur_start = 0;
		if (BLOCKSIZE*i  <= offset){
			cur_start = offset - BLOCKSIZE*i;
		}

		int cur_end = BLOCKSIZE;
		if (rem + cur_start < cur_end){
			cur_end = rem + cur_start;
		}

		memcpy(temp + cur_start, buf + write_ptr, cur_end - cur_start);

		write_ptr += cur_end - cur_start;
		rem -= cur_end - cur_start;

		simplefs_writeDataBlock(inodeptr->direct_blocks[i], temp);
	}
	simplefs_writeInode(i, inodeptr);
	free(inodeptr);
    return 0;
}


int simplefs_seek(int file_handle, int nseek){
    /*
	   increase `file_handle` offset by `nseek`
	*/
	int i = file_handle_array[file_handle].inode_number;
	struct inode_t *inodeptr = (struct inode_t *)malloc(sizeof(struct inode_t));
	simplefs_readInode(i, inodeptr);

	int candidate = file_handle_array[file_handle].offset + nseek;
	if (candidate < 0 || candidate > inodeptr->file_size){
		free(inodeptr);
    	return -1;
	}

	file_handle_array[file_handle].offset = candidate;
	free(inodeptr);
    return 0;
}