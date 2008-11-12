/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By downloading, copying, installing or
 * using the software you agree to this license. If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 * Intel License Agreement
 *
 * Copyright (c) 2000, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * -Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *  following disclaimer.
 *
 * -Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *  following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * -The name of Intel Corporation may not be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TARGET_H_
#define _TARGET_H_

#include "iscsi.h"
#include "iscsiutil.h"
#include "parameters.h"

/* a device can be made up of an extent or another device */
typedef struct disc_de_t {
	int32_t type;		/* device or extent */
	uint64_t size;		/* size of underlying extent or device */
	union {
		struct disc_extent_t *xp;	/* pointer to extent */
		struct disc_device_t *dp;	/* pointer to device */
	} u;
} disc_de_t;

/* this struct describes an extent of storage */
typedef struct disc_extent_t {
	char *extent;		/* extent name */
	char *dev;		/* device associated with it */
	uint64_t sacred;	/* offset of extent from start of device */
	uint64_t len;		/* size of extent */
	int fd;			/* in-core file descriptor */
	int used;		/* extent has been used in a device */
} disc_extent_t;

DEFINE_ARRAY(extv_t, disc_extent_t);

/* this struct describes a device */
typedef struct disc_device_t {
	char *dev;		/* device name */
	int raid;		/* RAID level */
	uint64_t off;		/* current offset in device */
	uint64_t len;		/* size of device */
	uint32_t size;		/* size of device/extent array */
	uint32_t c;		/* # of entries in device/extents */
	disc_de_t *xv;		/* device/extent array */
	int used;		/* device has been used in a device/target */
} disc_device_t;

DEFINE_ARRAY(devv_t, disc_device_t);

enum {
	TARGET_READONLY = 0x01
};

/* this struct describes an iscsi target's associated features */
typedef struct disc_target_t {
	char *target;		/* target name */
	disc_de_t de;		/* pointer to its device */
	uint16_t port;		/* port to listen on */
	char *mask;		/* mask to export it to */
	uint32_t flags;		/* any flags */
	uint16_t tsih;		/* target session identifying handle */
} disc_target_t;

DEFINE_ARRAY(targv_t, disc_target_t);

/* Default configuration */

#define DEFAULT_TARGET_MAX_SESSIONS 16	/* n+1 */
#define DEFAULT_TARGET_NUM_LUNS     1
#define DEFAULT_TARGET_BLOCK_LEN    512
#define DEFAULT_TARGET_NUM_BLOCKS   204800
#define DEFAULT_TARGET_NAME         "iqn.1994-04.org.netbsd.iscsi-target"
#define DEFAULT_TARGET_QUEUE_DEPTH  8
#define DEFAULT_TARGET_TCQ          0

enum {
	MAX_TGT_NAME_SIZE = 512,
	MAX_INITIATOR_ADDRESS_SIZE = 256,
	MAX_CONFIG_FILE_NAME = 512,

	ISCSI_IPv4 = AF_INET,
	ISCSI_IPv6 = AF_INET6,
	ISCSI_UNSPEC = PF_UNSPEC,

	MAXSOCK = 8
};

/* global variables, moved from target.c */
typedef struct globals_t {
	char targetname[MAX_TGT_NAME_SIZE];	/* name of target */
	uint16_t port;		/* target port */
	int sock;	/* socket on which it's listening */
	int sockc;		/* # of live sockets on which it's listening */
	int sockv[MAXSOCK];	/* sockets on which it's listening */
	int famv[MAXSOCK];	/* address families */
	int state;		/* current state of target */
	int listener_pid;	/* PID on which it's listening */
	volatile int listener_listening;	/* whether a listener is listening */
	char targetaddress[MAX_TGT_NAME_SIZE];	/* iSCSI TargetAddress set after iscsi_sock_accept() */
	targv_t *tv;		/* array of target devices */
	int address_family;	/* global default IP address family */
	int max_sessions;	/* maximum number of sessions */
	char config_file[MAX_CONFIG_FILE_NAME];	/* config file name */
	uint32_t last_tsih;	/* the last TSIH that was used */
} globals_t;

/* session parameters */
typedef struct target_session_t {
	int id;
	int d;
	int sock;
	uint16_t cid;
	uint32_t StatSN;
	uint32_t ExpCmdSN;
	uint32_t MaxCmdSN;
	uint8_t *buff;
	int UsePhaseCollapsedRead;
	int IsFullFeature;
	int IsLoggedIn;
	int LoginStarted;
	uint64_t isid;
	int tsih;
	globals_t *globals;
	iscsi_worker_t worker;
	iscsi_parameter_t *params;
	iscsi_sess_param_t sess_params;
	char initiator[MAX_INITIATOR_ADDRESS_SIZE];
	int address_family;
	int32_t last_tsih;
} target_session_t;

typedef struct target_cmd_t {
	iscsi_scsi_cmd_args_t *scsi_cmd;
	int (*callback) (void *);
	void *callback_arg;
} target_cmd_t;

int target_init(globals_t *, targv_t *, char *);
int target_shutdown(globals_t *);
int target_listen(globals_t *);
int target_transfer_data(target_session_t *, iscsi_scsi_cmd_args_t *,
			 struct iovec *, int);

int find_target_tsih(globals_t *, int);
int find_target_iqn(target_session_t *);

/* 
 * Interface from target to device:
 *
 * device_init() initializes the device
 * device_command() sends a SCSI command to one of the logical units in the device.
 * device_shutdown() shuts down the device.
 */

int device_init(globals_t *, targv_t *, disc_target_t *);
int device_command(target_session_t *, target_cmd_t *);
int device_shutdown(target_session_t *);
void device_set_var(const char *, char *);

#endif /* _TARGET_H_ */
