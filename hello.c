/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello
*/

#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "erl_interface.h"
#include "ei.h" 

typedef struct ferl_state {
	int erlang_fd;
}ferl_state;

#define FERL_DATA ((struct ferl_state *) fuse_get_context()->private_data)

static int hello_getattr(const char *path, struct stat *stbuf)
{
	ETERM * response = erl_rpc(FERL_DATA->erlang_fd,"gen_server","call",erl_format("[nefs,{get_attr,~s}]",path));
	ETERM * pattern = erl_format("{directory,Mode,Nlink}");
	ETERM * pattern2 = erl_format("{file,Mode,Nlink,Size}");
        int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	if(erl_match(pattern, response)) { //directory
		ETERM * Mode = erl_var_content(pattern, "Mode");
		ETERM * Nlink = erl_var_content(pattern, "Nlink");
		if(ERL_IS_INTEGER(Nlink) && ERL_IS_INTEGER(Mode)){
			stbuf->st_mode = S_IFDIR | ERL_INT_VALUE(Mode); //permissions
        	        stbuf->st_nlink = ERL_INT_VALUE(Nlink); // directories have the number of files in them
		}else{
			res = -ENOENT;
		}
	}else if(erl_match(pattern2, response)){ //file
		ETERM * Mode = erl_var_content(pattern2, "Mode");
                ETERM * Nlink = erl_var_content(pattern2, "Nlink");
		ETERM * Size = erl_var_content(pattern2, "Size");
                if(ERL_IS_INTEGER(Nlink) && ERL_IS_INTEGER(Mode) && ERL_IS_INTEGER(Size)){

			stbuf->st_mode = S_IFREG | ERL_INT_VALUE(Mode); //permissions
			stbuf->st_nlink = ERL_INT_VALUE(Nlink); //files only have 1
			stbuf->st_size = ERL_INT_VALUE(Size); // length of the file
                }else{
                        res = -ENOENT;
                }
	}else{
		res = -ENOENT;
	}	

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int res = 0;
	ETERM * response = erl_rpc(FERL_DATA->erlang_fd,"gen_server","call",erl_format("[nefs,{read_dir,~s}]",path));
	ETERM * pattern = erl_format("{ok,Listing}");	
	if(erl_match(pattern, response)) { //directory
		ETERM * Tail = erl_var_content(pattern, "Listing");
		if(ERL_IS_LIST(Tail)){
			while(!ERL_IS_EMPTY_LIST(Tail)){
				ETERM * elem = ERL_CONS_HEAD(Tail);
				// add in the links to the previous folders
				filler(buf, ".", NULL, 0);
        			filler(buf, "..", NULL, 0);
				
				// add in the folders contents
				if(ERL_IS_LIST(elem)){
					filler(buf,(char *)ERL_BIN_PTR(elem),NULL,0);
				}else{
					res = -ENOENT;
					break;
				}
				Tail = ERL_CONS_TAIL(Tail);
			}
		}else{
			res = -ENOENT;
		}
	}else{
                res = -ENOENT;
        }

        return res;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	int res = 0;
        ETERM * response = erl_rpc(FERL_DATA->erlang_fd,"gen_server","call",erl_format("[nefs,{open,~s,~i}]",path,fi->flags));
	ETERM * pattern = erl_format("{ok,Fd}");
	ETERM * pattern2 = erl_format("{error,access}");
	if(erl_match(pattern, response)) {
                ETERM * Fd_Pid = erl_var_content(pattern, "Fd");
		if(ERL_IS_PID(Fd_Pid)){
	                fi->fh = Fd_Pid; //store off the file handle pid to be used in subsequent functions 
		}else{
			res= -ENOENT;
		}
	}else if(erl_match(pattern2, response)){
		res = -EACCES;
	}else{
		res = -ENOENT;
	}
	return res;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	int ret_bytes = 0;
        ETERM * response = erl_rpc(FERL_DATA->erlang_fd,"file","pread",erl_format("[~s,{bof,~i},~i]",offset,size));
        ETERM * pattern = erl_format("{ok,Data}");
	if(erl_match(pattern, response)) {
                ETERM * Data = erl_var_content(pattern, "Data");
                if(ERL_IS_BINARY(Data)){
			ret_bytes = ERL_BIN_SIZE(Data);
			char * data = ERL_BIN_PTR(Data);
			memcpy(buf,data,size); //copy the data over into the buffer
                }
        }
        return ret_bytes;
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	erl_init(NULL, 0);
	if (erl_connect_init(1, "secretcookie", 0) == -1)
    		erl_err_quit("erl_connect_init");
	int fd = -1;
  	if ((fd = erl_connect("nefs@Daniels-MacBook")) < 0)
    		erl_err_quit("erl_connect");
	
	ferl_state FS={
		.erlang_fd = fd
	}; 
	
	fprintf(stderr, "Connected to nefs@Daniels-MacBook\n\r");
	erl_rpc(fd,"io","format",erl_format("[ping]"));
	int retval = fuse_main(argc, argv, &hello_oper, (void *)&FS);
	fprintf(stderr, "Finished\n\r");
	return retval;
}

