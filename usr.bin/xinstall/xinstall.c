/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1987, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)xinstall.c	8.1 (Berkeley) 7/21/93
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <utime.h>
#include <vis.h>

#include "mtree.h"

/* Bootstrap aid - this doesn't exist in most older releases */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)	/* from <sys/mman.h> */
#endif
#ifndef UF_NOHISTORY
#define UF_NOHISTORY	0
#endif

#define MAX_CMP_SIZE	(16 * 1024 * 1024)

#define	LN_ABSOLUTE	0x01
#define	LN_RELATIVE	0x02
#define	LN_HARD		0x04
#define	LN_SYMBOLIC	0x08
#define	LN_MIXED	0x10

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define	NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
#define	BACKUP_SUFFIX	".old"

static gid_t gid;
static uid_t uid;
static int dobackup, docompare, dodir, dolink, dopreserve, dostrip, dounpriv,
    nommap, safecopy, verbose;
static int haveopt_f, haveopt_g, haveopt_m, haveopt_o;
static mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
static const char *group, *owner;
static const char *suffix = BACKUP_SUFFIX;
static char *destdir, *fflags;

static int	compare(int, const char *, size_t, int, const char *, size_t);
static void	copy(int, const char *, int, const char *, off_t);
static int	create_newfile(const char *, int, struct stat *);
static int	create_tempfile(const char *, char *, size_t);
static int	do_link(const char *, const char *, const struct stat *);
static void	do_symlink(const char *, const char *, const struct stat *);
static void	makelink(const char *, const char *, const struct stat *);
static void	install(const char *, const char *, u_long, u_long, u_int);
static void	install_dir(char *);
static int	parseid(const char *, id_t *);
static void	strip(const char *);
static int	trymmap(int);
static void	usage(void);

int
main(int argc, char *argv[])
{
	struct stat from_sb, to_sb;
	mode_t *set;
	u_long fset;
	u_long fclr;
	int ch, no_target;
	u_int iflags;
	char *p;
	const char *to_name;

	fclr = 0;
	fset = 0;
	iflags = 0;
	group = NULL;
	owner = NULL;

	while ((ch = getopt(argc, argv, "B:bCcD:df:g:L:l:M:m:N:o:pSsUv")) != -1)
		switch((char)ch) {
		case 'B':
			suffix = optarg;
			/* FALLTHROUGH */
		case 'b':
			dobackup = 1;
			break;
		case 'C':
			docompare = 1;
			break;
		case 'c':
			/* For backwards compatibility. */
			break;
		case 'D':
			destdir = optarg;
			break;
		case 'd':
			dodir = 1;
			break;
		case 'f':
			haveopt_f = 1;
			fflags = optarg;
			break;
		case 'g':
			haveopt_g = 1;
			group = optarg;
			break;
		case 'l':
			for (p = optarg; *p; p++)
				switch (*p) {
				case 's':
					dolink &= ~(LN_HARD|LN_MIXED);
					dolink |= LN_SYMBOLIC;
					break;
				case 'h':
					dolink &= ~(LN_SYMBOLIC|LN_MIXED);
					dolink |= LN_HARD;
					break;
				case 'm':
					dolink &= ~(LN_SYMBOLIC|LN_HARD);
					dolink |= LN_MIXED;
					break;
				case 'a':
					dolink &= ~LN_RELATIVE;
					dolink |= LN_ABSOLUTE;
					break;
				case 'r':
					dolink &= ~LN_ABSOLUTE;
					dolink |= LN_RELATIVE;
					break;
				default:
					errx(EXIT_FAILURE, "%c: invalid link type", *p);
					/* NOTREACHED */
				}
			break;
		case 'M':
			nommap = 1;
			break;
		case 'm':
			haveopt_m = 1;
			if (!(set = setmode(optarg)))
				errx(EX_USAGE, "invalid file mode: %s",
				     optarg);
			mode = getmode(set, 0);
			free(set);
			break;
		case 'L':
			/* -L kept for compatibility with pre-5.4 DragonFly */
			warnx("Option -L is deprecated, use -N instead");
			/* FALLTHROUGH */
		case 'N':
			if (!setup_getid(optarg))
				err(EX_OSERR, "Unable to use user and group "
				    "databases in `%s'", optarg);
			break;
		case 'o':
			haveopt_o = 1;
			owner = optarg;
			break;
		case 'p':
			docompare = dopreserve = 1;
			break;
		case 'S':
			safecopy = 1;
			break;
		case 's':
			dostrip = 1;
			break;
		case 'U':
			dounpriv = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* some options make no sense when creating directories */
	if (dostrip && dodir) {
		warnx("-d and -s may not be specified together");
		usage();
	}

	if (getenv("DONTSTRIP") != NULL) {
		warnx("DONTSTRIP set - will not strip installed binaries");
		dostrip = 0;
	}

	/* must have at least two arguments, except when creating directories */
	if (argc == 0 || (argc == 1 && !dodir))
		usage();

	/* need to make a temp copy so we can compare stripped version */
	if (docompare && dostrip)
		safecopy = 1;

	/* get group and owner id's */
	if (group != NULL && !dounpriv) {
		if (gid_from_group(group, &gid) == -1) {
			id_t id;
			if (!parseid(group, &id))
				errx(1, "unknown group %s", group);
			gid = id;
		}
	} else
		gid = (gid_t)-1;

	if (owner != NULL && !dounpriv) {
		if (uid_from_user(owner, &uid) == -1) {
			id_t id;
			if (!parseid(owner, &id))
				errx(1, "unknown user %s", owner);
			uid = id;
		}
	} else
		uid = (uid_t)-1;

	if (fflags != NULL && !dounpriv) {
		if (strtofflags(&fflags, &fset, &fclr))
			errx(EX_USAGE, "%s: invalid flag", fflags);
		iflags |= SETFLAGS;
	}

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv);
		exit(EX_OK);
		/* NOTREACHED */
	}

	to_name = argv[argc - 1];
	no_target = stat(to_name, &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		if (dolink & LN_SYMBOLIC) {
			if (lstat(to_name, &to_sb) != 0)
				err(EX_OSERR, "%s vanished", to_name);
			if (S_ISLNK(to_sb.st_mode)) {
				if (argc != 2) {
					errno = ENOTDIR;
					err(EX_USAGE, "%s", to_name);
				}
				install(*argv, to_name, fset, fclr, iflags);
				exit(EX_OK);
			}
		}
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, fclr, iflags | DIRECTORY);
		exit(EX_OK);
		/* NOTREACHED */
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2) {
		if (no_target)
			warnx("target directory `%s' does not exist",
			    argv[argc - 1]);
		else
			warnx("target `%s' is not a directory",
			    argv[argc - 1]);
		usage();
	}

	if (!no_target && !dolink) {
		if (stat(*argv, &from_sb))
			err(EX_OSERR, "%s", *argv);
		if (!S_ISREG(to_sb.st_mode)) {
			errno = EFTYPE;
			err(EX_OSERR, "%s", to_name);
		}
		if (to_sb.st_dev == from_sb.st_dev &&
		    to_sb.st_ino == from_sb.st_ino)
			errx(EX_USAGE, 
			    "%s and %s are the same file", *argv, to_name);
	}
	install(*argv, to_name, fset, fclr, iflags);
	exit(EX_OK);
	/* NOTREACHED */
}

/*
 * parseid --
 *	parse uid or gid from arg into id, returning non-zero if successful
 */
static int
parseid(const char *name, id_t *id)
{
	char	*ep;
	errno = 0;
	*id = (id_t)strtoul(name, &ep, 10);
	if (errno || *ep != '\0')
		return (0);
	return (1);
}

/*
 * quiet_mktemp --
 *	mktemp implementation used mkstemp to avoid mktemp warnings.  We
 *	really do need mktemp semantics here as we will be creating a link.
 */
static char *
quiet_mktemp(char *template)
{
	int fd;

	if ((fd = mkstemp(template)) == -1)
		return (NULL);
	close (fd);
	if (unlink(template) == -1)
		err(EX_OSERR, "unlink %s", template);
	return (template);
}

/*
 * do_link --
 *	make a hard link, obeying dorename if set
 *	return -1 on failure
 */
static int
do_link(const char *from_name, const char *to_name,
    const struct stat *target_sb)
{
	char tmpl[MAXPATHLEN];
	int ret;

	if (safecopy && target_sb != NULL) {
		(void)snprintf(tmpl, sizeof(tmpl), "%s.inst.XXXXXX", to_name);
		/* This usage is safe. */
		if (quiet_mktemp(tmpl) == NULL)
			err(EX_OSERR, "%s: mktemp", tmpl);
		ret = link(from_name, tmpl);
		if (ret == 0) {
			if (target_sb->st_mode & S_IFDIR && rmdir(to_name) ==
			    -1) {
				unlink(tmpl);
				err(EX_OSERR, "%s", to_name);
			}
			if (target_sb->st_flags & NOCHANGEBITS)
				(void)chflags(to_name, target_sb->st_flags &
				     ~NOCHANGEBITS);
			if (verbose)
				printf("install: link %s -> %s\n",
				    from_name, to_name);
			ret = rename(tmpl, to_name);
			/*
			 * If rename has posix semantics, then the temporary
			 * file may still exist when from_name and to_name point
			 * to the same file, so unlink it unconditionally.
			 */
			(void)unlink(tmpl);
		}
		return (ret);
	} else {
		if (verbose)
			printf("install: link %s -> %s\n",
			    from_name, to_name);
		return (link(from_name, to_name));
	}
}

/*
 * do_symlink --
 *	Make a symbolic link, obeying dorename if set. Exit on failure.
 */
static void
do_symlink(const char *from_name, const char *to_name,
    const struct stat *target_sb)
{
	char tmpl[MAXPATHLEN];

	if (safecopy && target_sb != NULL) {
		(void)snprintf(tmpl, sizeof(tmpl), "%s.inst.XXXXXX", to_name);
		/* This usage is safe. */
		if (quiet_mktemp(tmpl) == NULL)
			err(EX_OSERR, "%s: mktemp", tmpl);

		if (symlink(from_name, tmpl) == -1)
			err(EX_OSERR, "symlink %s -> %s", from_name, tmpl);

		if (target_sb->st_mode & S_IFDIR && rmdir(to_name) == -1) {
			(void)unlink(tmpl);
			err(EX_OSERR, "%s", to_name);
		}
		if (target_sb->st_flags & NOCHANGEBITS)
			(void)chflags(to_name, target_sb->st_flags &
			     ~NOCHANGEBITS);
		if (verbose)
			printf("install: symlink %s -> %s\n",
			    from_name, to_name);
		if (rename(tmpl, to_name) == -1) {
			/* Remove temporary link before exiting. */
			(void)unlink(tmpl);
			err(EX_OSERR, "%s: rename", to_name);
		}
	} else {
		if (verbose)
			printf("install: symlink %s -> %s\n",
			    from_name, to_name);
		if (symlink(from_name, to_name) == -1)
			err(EX_OSERR, "symlink %s -> %s", from_name, to_name);
	}
}

/*
 * makelink --
 *	make a link from source to destination
 */
static void
makelink(const char *from_name, const char *to_name,
    const struct stat *target_sb)
{
	char	src[MAXPATHLEN], dst[MAXPATHLEN], lnk[MAXPATHLEN];
	struct stat	to_sb;

	/* Try hard links first. */
	if (dolink & (LN_HARD|LN_MIXED)) {
		if (do_link(from_name, to_name, target_sb) == -1) {
			if ((dolink & LN_HARD) || errno != EXDEV)
				err(EX_OSERR, "link %s -> %s", from_name, to_name);
		} else {
			if (stat(to_name, &to_sb))
				err(EX_OSERR, "%s: stat", to_name);
			if (S_ISREG(to_sb.st_mode)) {
				/*
				 * XXX: hard links to anything other than
				 * plain files are not metalogged
				 */
				int omode;
				const char *oowner, *ogroup;
				char *offlags;

				/*
				 * XXX: use underlying perms, unless
				 * overridden on command line.
				 */
				omode = mode;
				if (!haveopt_m)
					mode = (to_sb.st_mode & 0777);
				oowner = owner;
				if (!haveopt_o)
					owner = NULL;
				ogroup = group;
				if (!haveopt_g)
					group = NULL;
				offlags = fflags;
				if (!haveopt_f)
					fflags = NULL;
				mode = omode;
				owner = oowner;
				group = ogroup;
				fflags = offlags;
			}
			return;
		}
	}

	/* Symbolic links. */
	if (dolink & LN_ABSOLUTE) {
		/* Convert source path to absolute. */
		if (realpath(from_name, src) == NULL)
			err(EX_OSERR, "%s: realpath", from_name);
		do_symlink(src, to_name, target_sb);
		/* XXX: src may point outside of destdir */
		return;
	}

	if (dolink & LN_RELATIVE) {
		char *to_name_copy, *cp, *d, *s;

		if (*from_name != '/') {
			/* this is already a relative link */
			do_symlink(from_name, to_name, target_sb);
			/* XXX: from_name may point outside of destdir. */
			return;
		}

		/* Resolve pathnames. */
		if (realpath(from_name, src) == NULL)
			err(EX_OSERR, "%s: realpath", from_name);

		/*
		 * The last component of to_name may be a symlink,
		 * so use realpath to resolve only the directory.
		 */
		to_name_copy = strdup(to_name);
		if (to_name_copy == NULL)
			err(EX_OSERR, "%s: strdup", to_name);
		cp = dirname(to_name_copy);
		if (realpath(cp, dst) == NULL)
			err(EX_OSERR, "%s: realpath", cp);
		/* .. and add the last component. */
		if (strcmp(dst, "/") != 0) {
			if (strlcat(dst, "/", sizeof(dst)) > sizeof(dst))
				errx(1, "resolved pathname too long");
		}
		strcpy(to_name_copy, to_name);
		cp = basename(to_name_copy);
		if (strlcat(dst, cp, sizeof(dst)) > sizeof(dst))
			errx(1, "resolved pathname too long");
		free(to_name_copy);

		/* Trim common path components. */
		for (s = src, d = dst; *s == *d; s++, d++)
			continue;
		while (*s != '/')
			s--, d--;

		/* Count the number of directories we need to backtrack. */
		for (++d, lnk[0] = '\0'; *d; d++)
			if (*d == '/')
				(void)strlcat(lnk, "../", sizeof(lnk));

		(void)strlcat(lnk, ++s, sizeof(lnk));

		do_symlink(lnk, to_name, target_sb);
		/* XXX: Link may point outside of destdir. */
		return;
	}

	/*
	 * If absolute or relative was not specified, try the names the
	 * user provided.
	 */
	do_symlink(from_name, to_name, target_sb);
	/* XXX: from_name may point outside of destdir. */
}

/*
 * install --
 *	build a path name and install the file
 */
static void
install(const char *from_name, const char *to_name, u_long fset, u_long fclr,
	u_int flags)
{
	struct stat from_sb, temp_sb, to_sb;
	struct utimbuf utb;
	int devnull, files_match, from_fd, serrno, target;
	int tempcopy, temp_fd, to_fd;
	u_long nfset;
	char backup[MAXPATHLEN], *p, pathbuf[MAXPATHLEN], tempfile[MAXPATHLEN];

	files_match = 0;
	from_fd = -1;
	to_fd = -1;

	/* If try to install NULL file to a directory, fails. */
	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL)) {
		if (!dolink) {
			if (stat(from_name, &from_sb))
				err(EX_OSERR, "%s", from_name);
			if (!S_ISREG(from_sb.st_mode)) {
				errno = EFTYPE;
				err(EX_OSERR, "%s", from_name);
			}
		}
		/* Build the target path. */
		if (flags & DIRECTORY) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s%s%s",
			    to_name,
			    to_name[strlen(to_name) - 1] == '/' ? "" : "/",
			    (p = strrchr(from_name, '/')) ? ++p : from_name);
			to_name = pathbuf;
		}
		devnull = 0;
	} else {
		devnull = 1;
	}

	target = (lstat(to_name, &to_sb) == 0);

	if (dolink) {
		if (target && !safecopy) {
			if (to_sb.st_mode & S_IFDIR && rmdir(to_name) == -1)
				err(EX_OSERR, "%s", to_name);
			if (to_sb.st_flags & NOCHANGEBITS)
				(void)chflags(to_name,
				    to_sb.st_flags & ~NOCHANGEBITS);
			unlink(to_name);
		}
		makelink(from_name, to_name, target ? &to_sb : NULL);
		return;
	}

	if (target && !S_ISREG(to_sb.st_mode) && !S_ISLNK(to_sb.st_mode)) {
		errno = EFTYPE;
		warn("%s", to_name);
		return;
	}

	/* Only copy safe if the target exists. */
	tempcopy = safecopy && target;

	if (!devnull && (from_fd = open(from_name, O_RDONLY, 0)) < 0)
		err(EX_OSERR, "%s", from_name);

	/* If we don't strip, we can compare first. */
	if (docompare && !dostrip && target && S_ISREG(to_sb.st_mode)) {
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
		if (devnull)
			files_match = to_sb.st_size == 0;
		else
			files_match = !(compare(from_fd, from_name,
			    (size_t)from_sb.st_size, to_fd,
			    to_name, (size_t)to_sb.st_size));

		/* Close "to" file unless we match. */
		if (!files_match)
			(void)close(to_fd);
	}

	if (!files_match) {
		if (tempcopy) {
			to_fd = create_tempfile(to_name, tempfile,
			    sizeof(tempfile));
			if (to_fd < 0)
				err(EX_OSERR, "%s", tempfile);
		} else {
			if ((to_fd = create_newfile(to_name, target,
			    &to_sb)) < 0)
				err(EX_OSERR, "%s", to_name);
			if (verbose)
				(void)printf("install: %s -> %s\n",
				    from_name, to_name);
		}
		if (!devnull)
			copy(from_fd, from_name, to_fd,
			     tempcopy ? tempfile : to_name, from_sb.st_size);
	}

	if (dostrip) {
		strip(tempcopy ? tempfile : to_name);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 * that does not work in-place -- like GNU binutils strip.
		 */
		close(to_fd);
		to_fd = open(tempcopy ? tempfile : to_name, O_RDONLY, 0);
		if (to_fd < 0)
			err(EX_OSERR, "stripping %s", to_name);
	}

	/*
	 * Compare the stripped temp file with the target.
	 */
	if (docompare && dostrip && target && S_ISREG(to_sb.st_mode)) {
		temp_fd = to_fd;

		/* Re-open to_fd using the real target name. */
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);

		if (fstat(temp_fd, &temp_sb)) {
			serrno = errno;
			(void)unlink(tempfile);
			errno = serrno;
			err(EX_OSERR, "%s", tempfile);
		}

		if (compare(temp_fd, tempfile, (size_t)temp_sb.st_size, to_fd,
			    to_name, (size_t)to_sb.st_size) == 0) {
			/*
			 * If target has more than one link we need to
			 * replace it in order to snap the extra links.
			 * Need to preserve target file times, though.
			 */
			if (to_sb.st_nlink != 1) {
				utb.actime = to_sb.st_atime;
				utb.modtime = to_sb.st_mtime;
				utime(tempfile, &utb);
			} else {
				files_match = 1;
				(void)unlink(tempfile);
			}
			(void) close(temp_fd);
		}
	}

	/*
	 * Move the new file into place if doing a safe copy
	 * and the files are different (or just not compared).
	 */
	if (tempcopy && !files_match) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name, to_sb.st_flags & ~NOCHANGEBITS);
		if (dobackup) {
			if ((size_t)snprintf(backup, MAXPATHLEN, "%s%s", to_name,
			    suffix) != strlen(to_name) + strlen(suffix)) {
				unlink(tempfile);
				errx(EX_OSERR, "%s: backup filename too long",
				    to_name);
			}
			if (verbose)
				(void)printf("install: %s -> %s\n", to_name, backup);
			if (rename(to_name, backup) < 0) {
				serrno = errno;
				unlink(tempfile);
				errno = serrno;
				err(EX_OSERR, "rename: %s to %s", to_name,
				     backup);
			}
		}
		if (verbose)
			(void)printf("install: %s -> %s\n", from_name, to_name);
		if (rename(tempfile, to_name) < 0) {
			serrno = errno;
			unlink(tempfile);
			errno = serrno;
			err(EX_OSERR, "rename: %s to %s",
			    tempfile, to_name);
		}

		/* Re-open to_fd so we aren't hosed by the rename(2). */
		(void) close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
	}

	/*
	 * Preserve the timestamp of the source file if necessary.
	 */
	if (dopreserve && !files_match && !devnull) {
		utb.actime = from_sb.st_atime;
		utb.modtime = from_sb.st_mtime;
		utime(to_name, &utb);
	}

	if (fstat(to_fd, &to_sb) == -1) {
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_OSERR, "%s", to_name);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if (!dounpriv && ((gid != (gid_t)-1 && gid != to_sb.st_gid) ||
	    (uid != (uid_t)-1 && uid != to_sb.st_uid) ||
	    (mode != to_sb.st_mode))) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)fchflags(to_fd, to_sb.st_flags & ~NOCHANGEBITS);
	}

	if (!dounpriv && (
	    (gid != (gid_t)-1 && gid != to_sb.st_gid) ||
	    (uid != (uid_t)-1 && uid != to_sb.st_uid)))
		if (fchown(to_fd, uid, gid) == -1) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR,"%s: chown/chgrp", to_name);
		}

	if (mode != to_sb.st_mode) {
		if (fchmod(to_fd,
		     dounpriv ? mode & (S_IRWXU|S_IRWXG|S_IRWXO) : mode)) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR, "%s: chmod", to_name);
		}
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump and history flags.  The dump flag
	 * is left clear on the target while the history flag from when
	 * the target was created (which is inherited from the target's
	 * parent directory) is retained.
	 */
	if (flags & SETFLAGS) {
		nfset = (to_sb.st_flags | fset) & ~fclr;
	} else {
		nfset = (from_sb.st_flags & ~(UF_NODUMP | UF_NOHISTORY)) |
			(to_sb.st_flags & UF_NOHISTORY);
	}

	/*
	 * NFS does not support flags.  Ignore EOPNOTSUPP flags if we're just
	 * trying to turn off UF_NODUMP.  If we're trying to set real flags,
	 * then warn if the fs doesn't support it, otherwise fail.
	 */
	if (!dounpriv && !devnull && fchflags(to_fd, nfset)) {
		if (flags & SETFLAGS) {
			if (errno == EOPNOTSUPP)
				warn("%s: chflags", to_name);
			else {
				serrno = errno;
				(void)unlink(to_name);
				errno = serrno;
				err(EX_OSERR, "%s: chflags", to_name);
			}
		}
	}

	(void)close(to_fd);
	if (!devnull)
		(void)close(from_fd);
}

/*
 * compare --
 *	compare two files; non-zero means files differ
 */
static int
compare(int from_fd, const char *from_name __unused, size_t from_len,
	int to_fd, const char *to_name __unused, size_t to_len)
{
	char *p, *q;
	int rv;
	int done_compare;

	rv = 0;
	if (from_len != to_len)
		return 1;

	if (from_len <= MAX_CMP_SIZE) {
		done_compare = 0;
		if (trymmap(from_fd) && trymmap(to_fd)) {
			p = mmap(NULL, from_len, PROT_READ, MAP_SHARED,
			    from_fd, (off_t)0);
			if (p == (char *)MAP_FAILED)
				goto out;
			q = mmap(NULL, from_len, PROT_READ, MAP_SHARED,
			    to_fd, (off_t)0);
			if (q == (char *)MAP_FAILED) {
				munmap(p, from_len);
				goto out;
			}

			rv = memcmp(p, q, from_len);
			munmap(p, from_len);
			munmap(q, from_len);
			done_compare = 1;
		}
	out:
		if (!done_compare) {
			char buf1[MAXBSIZE];
			char buf2[MAXBSIZE];
			int n1, n2;

			rv = 0;
			lseek(from_fd, 0, SEEK_SET);
			lseek(to_fd, 0, SEEK_SET);
			while (rv == 0) {
				n1 = read(from_fd, buf1, sizeof(buf1));
				if (n1 == 0)
					break;		/* EOF */
				else if (n1 > 0) {
					n2 = read(to_fd, buf2, n1);
					if (n2 == n1)
						rv = memcmp(buf1, buf2, n1);
					else
						rv = 1;	/* out of sync */
				} else
					rv = 1;		/* read failure */
			}
			lseek(from_fd, 0, SEEK_SET);
			lseek(to_fd, 0, SEEK_SET);
		}
	} else
		rv = 1;	/* don't bother in this case */

	return rv;
}

/*
 * create_tempfile --
 *	create a temporary file based on path and open it
 */
static int
create_tempfile(const char *path, char *temp, size_t tsize)
{
	char *p;

	(void)strncpy(temp, path, tsize);
	temp[tsize - 1] = '\0';
	if ((p = strrchr(temp, '/')) != NULL)
		p++;
	else
		p = temp;
	(void)strncpy(p, "INS@XXXX", &temp[tsize - 1] - p);
	temp[tsize - 1] = '\0';
	return (mkstemp(temp));
}

/*
 * create_newfile --
 *	create a new file, overwriting an existing one if necessary
 */
static int
create_newfile(const char *path, int target, struct stat *sbp)
{
	char backup[MAXPATHLEN];

	if (target) {
		/*
		 * Unlink now... avoid ETXTBSY errors later.  Try to turn
		 * off the append/immutable bits -- if we fail, go ahead,
		 * it might work.
		 */
		if (sbp->st_flags & NOCHANGEBITS)
			(void)chflags(path, sbp->st_flags & ~NOCHANGEBITS);

		if (dobackup) {
			if ((size_t)snprintf(backup, MAXPATHLEN, "%s%s",
			    path, suffix) != strlen(path) + strlen(suffix))
				errx(EX_OSERR, "%s: backup filename too long",
				    path);
			(void)snprintf(backup, MAXPATHLEN, "%s%s",
			    path, suffix);
			if (verbose)
				(void)printf("install: %s -> %s\n",
				    path, backup);
			if (rename(path, backup) < 0)
				err(EX_OSERR, "rename: %s to %s", path, backup);
		} else
			unlink(path);
	}

	return (open(path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR));
}

/*
 * copy --
 *	copy from one file to another
 */
static void
copy(int from_fd, const char *from_name, int to_fd,
     const char *to_name, off_t size)
{
	int nr, nw;
	int serrno;
	char *p;
	char buf[MAXBSIZE];
	int done_copy;

	/* Rewind file descriptors. */
	if (lseek(from_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", from_name);
	if (lseek(to_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", to_name);

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
	done_copy = 0;
	if (size <= 8 * 1048576 && trymmap(from_fd) &&
	    (p = mmap(NULL, (size_t)size, PROT_READ, MAP_SHARED,
		    from_fd, (off_t)0)) != (char *)MAP_FAILED) {
		nw = write(to_fd, p, size);
		if (nw != size) {
			serrno = errno;
			(void)unlink(to_name);
			errno = nw > 0 ? EIO : serrno;
			err(EX_OSERR, "%s", to_name);
		}
		done_copy = 1;
	}
	if (!done_copy) {
		while ((nr = read(from_fd, buf, sizeof(buf))) > 0) {
			if ((nw = write(to_fd, buf, nr)) != nr) {
				serrno = errno;
				(void)unlink(to_name);
				errno = nw > 0 ? EIO : serrno;
				err(EX_OSERR, "%s", to_name);
			}
		}
		if (nr != 0) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR, "%s", from_name);
		}
	}
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
static void
strip(const char *to_name)
{
	const char *stripbin;
	int serrno, status;

	switch (fork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_TEMPFAIL, "fork");
	case 0:
		stripbin = getenv("STRIPBIN");
		if (stripbin == NULL)
			stripbin = "strip";
		execlp(stripbin, stripbin, to_name, NULL);
		err(EX_OSERR, "exec(%s)", stripbin);
	default:
		if (wait(&status) == -1 || status) {
			serrno = errno;
			(void)unlink(to_name);
			errc(EX_SOFTWARE, serrno, "wait");
			/* NOTREACHED */
		}
	}
}

/*
 * When doing a concurrent make -j N multiple install's can race the mkdir.
 */
static
int
mkdir_race(const char *path, int nmode)
{
	int res;
	struct stat sb;

	res = mkdir(path, nmode);
	if (res < 0 && errno == EEXIST) {
		if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode))
			return(0);
		res = mkdir(path, nmode);
	}
	return (res);
}

/*
 * install_dir --
 *	build directory hierarchy
 */
static void
install_dir(char *path)
{
	char *p;
	struct stat sb;
	int ch;

	for (p = path;; ++p)
		if (!*p || (p != path && *p  == '/')) {
			ch = *p;
			*p = '\0';
			if (stat(path, &sb)) {
				if (errno != ENOENT ||
				    mkdir_race(path, 0755) < 0) {
					err(EX_OSERR, "mkdir %s", path);
					/* NOTREACHED */
				} else if (verbose)
					(void)printf("install: mkdir %s\n",
						     path);
			} else if (!S_ISDIR(sb.st_mode))
				errx(EX_OSERR, "%s exists but is not a directory", path);
			if (!(*p = ch))
				break;
		}

	if (!dounpriv) {
		if ((gid != (gid_t)-1 || uid != (uid_t)-1) &&
		    chown(path, uid, gid))
			warn("chown %u:%u %s", uid, gid, path);
		/* XXXBED: should we do the chmod in the dounpriv case? */
		if (chmod(path, mode))
			warn("chmod %o %s", mode, path);
	}
}

/*
 * usage --
 *	print a usage message and die
 */
static void
usage(void)
{
	fprintf(stderr,
"usage: install [-bCcpSsUv] [-f flags] [-g group] [-m mode] [-o owner]\n"
"               [-D dest] [-h hash] [-T tags]\n"
"               [-B suffix] [-l linkflags] [-N dbdir]\n"
"               file1 file2\n"
"       install [-bCcpSsUv] [-B suffix] [-D dest] [-f flags] [-g group]\n"
"               [-N dbdir] [-m mode] [-o owner] file1 ... fileN directory\n"
"       install -d [-lUv] [-D dest] [-g group] [-m mode] [-N dbdir] [-o owner]\n"
"               directory ...\n");
	exit(EX_USAGE);
	/* NOTREACHED */
}

/*
 * trymmap --
 *	return true (1) if mmap should be tried, false (0) if not.
 */
static int
trymmap(int fd)
{
/*
 * The ifdef is for bootstrapping - f_fstypename doesn't exist in
 * pre-Lite2-merge systems.
 */
#ifdef MFSNAMELEN
	struct statfs stfs;

	if (nommap || fstatfs(fd, &stfs) != 0)
		return (0);
	if (strcmp(stfs.f_fstypename, "ufs") == 0 ||
	    strcmp(stfs.f_fstypename, "cd9660") == 0)
		return (1);
#endif
	return (0);
}
