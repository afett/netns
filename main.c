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

static int add(const char *name)
{
	if (unshare(CLONE_NEWNET) != 0) {
		fprintf(stderr, "can't create new namespace: %s\n",
			strerror(errno));
		return -1;
	}

	int fd = open(name, O_RDONLY|O_CREAT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "can't create %s: %s\n",
			name, strerror(errno));
		return -1;
	}
	close(fd);

	int ret = mount("/proc/self/ns/net", name, 0, MS_BIND, 0);
	if (ret != 0) {
		fprintf(stderr, "can't mount namespace: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

static int del(const char *name)
{
	if (umount(name) != 0) {
		fprintf(stderr, "can't umount namespace '%s': %s\n",
			name, strerror(errno));
		return -1;
	}

	if (unlink(name) != 0) {
		fprintf(stderr, "can't unlink %s: %s\n",
			name, strerror(errno));
		return -1;
	}

	return 0;
}

static int exec(const char *name, char *argv[])
{
	int fd = open(name, O_RDONLY|O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "can't open namesapce %s: %s\n",
			name, strerror(errno));
		return -1;
	}

	if (setns(fd, CLONE_NEWNET) != 0) {
		fprintf(stderr, "can't enter namespace %s: %s\n",
			name, strerror(errno));
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

int main(int argc, char *argv[])
{
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "add") == 0) {
		return add(argv[2]) == 0 ? 0 : 1;
	} else if (strcmp(argv[1], "del") == 0) {
		return del(argv[2]) == 0 ? 0 : 1;
	} else if (strcmp(argv[1], "list") == 0) {
		fprintf(stderr, "unimplemented\n");
		usage(argv[0]);
		return 1;
	} else if (strcmp(argv[1], "exec") == 0) {
		if (argc < 4) {
			fprintf(stderr, "exec is missing command\n");
			usage(argv[0]);
			return 1;
		}

		return exec(argv[2], &argv[3]);
	} else {
		fprintf(stderr, "unknown command '%s'\n", argv[1]);
		usage(argv[0]);
		return 1;
	}

	return 0;
}
