/*
   Copyright (c) 2015, Andreas Fett
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>

static void usage(char *argv0)
{
	fprintf(stderr, "%s:\n"
		"add <name>\n"
		"del <name>\n"
		"list <name> \n"
		"exec <name> cmd [args]\n", argv0);
}

/* add a namespace
     - leave the current network namespace
     - create a new regular file
     - bind mount out new namespace to the file
*/

static int add(const char *path)
{
	if (unshare(CLONE_NEWNET) != 0) {
		fprintf(stderr, "can't create new namespace: %s\n",
			strerror(errno));
		return -1;
	}

	int fd = open(path, O_RDONLY|O_CREAT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "can't create %s: %s\n",
			path, strerror(errno));
		return -1;
	}
	close(fd);

	int ret = mount("/proc/self/ns/net", path, 0, MS_BIND, 0);
	if (ret != 0) {
		fprintf(stderr, "can't mount namespace: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

/* delete a namespace
     - umount the namespace
     - unlink the file
*/

static int del(const char *path)
{
	if (umount(path) != 0) {
		fprintf(stderr, "can't umount namespace '%s': %s\n",
			path, strerror(errno));
		return -1;
	}

	if (unlink(path) != 0) {
		fprintf(stderr, "can't unlink %s: %s\n",
			path, strerror(errno));
		return -1;
	}

	return 0;
}

/* exec a program in the given namespace
     - open the namespace
     - associate with the namesapce fd
     - pass command line args to exec
*/

static int exec(const char *path, char *argv[])
{
	int fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "can't open namespace %s: %s\n",
			path, strerror(errno));
		return -1;
	}

	if (setns(fd, CLONE_NEWNET) != 0) {
		fprintf(stderr, "can't enter namespace %s: %s\n",
			path, strerror(errno));
		close(fd);
		return -1;
	}

	if (execvp(argv[0], argv) != 0) {
		fprintf(stderr, "can't exec %s: %s\n",
			argv[0], strerror(errno));
		return -1;
	}

	return 0;
}

enum ns_op {
	NS_OP_UNKNOWN = 0,
	NS_OP_ADD,
	NS_OP_DEL,
	NS_OP_LIST,
	NS_OP_EXEC,
};

static enum ns_op getop(const char *cmd)
{
	struct {
		const char *cmd;
		int op;
	} cmds[] = {
		{ .cmd = "add", .op = NS_OP_ADD },
		{ .cmd = "del", .op = NS_OP_DEL },
		{ .cmd = "list", .op = NS_OP_LIST },
		{ .cmd = "exec", .op = NS_OP_EXEC },
		{ .cmd = 0, .op = NS_OP_UNKNOWN },
	};

	for (size_t i = 0; cmds[i].cmd; ++i) {
		if (strcmp(cmd, cmds[i].cmd) == 0) {
			return cmds[i].op;
		}
	}

	return NS_OP_UNKNOWN;
}


int main(int argc, char *argv[])
{
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	int ret = 0;
	const char *nspath = argv[2];
	switch (getop(argv[1])) {
	case NS_OP_ADD:
		ret = add(nspath);
		break;
	case NS_OP_DEL:
		ret = del(nspath);
		break;
	case NS_OP_LIST:
		fprintf(stderr, "unimplemented\n");
		usage(argv[0]);
		return 1;
	case NS_OP_EXEC:
		if (argc < 4) {
			fprintf(stderr, "exec is missing command\n");
			usage(argv[0]);
			return 1;
		}
		ret = exec(nspath, &argv[3]);
		break;
	case NS_OP_UNKNOWN:
		fprintf(stderr, "unknown command '%s'\n", argv[1]);
		usage(argv[0]);
		return 1;
	}

	return ret == 0 ? 0 : 1;
}
