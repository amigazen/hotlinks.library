/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_pass.c - HotLinks:Users password file
 *
 * Simple "username:password" lines (plain text, 1992-era style).
 * Default superuser account "root" with empty password is created when
 * the file is first opened.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <string.h>

#include "private/hl_bases.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <libraries/hotlinks.h>
#include "private/hl_internal.h"
#include "private/hl_pass.h"

static int HLPassParseLine(char *line, char *name, char *pwdOut, LONG pwdLen);
static int HLPassWriteDefault(char *path);
static int HLPassEnsureUsersAt(char *path);
static int HLPassPathIsDirectory(char *path);

static int
HLPassPathIsDirectory(char *path)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    int isdir;

    isdir = 0;
    lock = Lock(path, ACCESS_READ);
    if (lock == 0) {
        return 0;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL) {
        if (Examine(lock, fib) != 0) {
            if (fib->fib_DirEntryType >= 0) {
                isdir = 1;
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return isdir;
}

static int
HLPassUsersPath(struct HLPrivateBase *pb, char *buf, LONG buflen)
{
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }
    if (HLEnsureRoot(pb) != NOERROR) {
        return IOERROR;
    }
    Strncpy(buf, tok->t_RootPath, buflen - 1);
    buf[buflen - 1] = 0;
    AddPart(buf, HL_USERS_NAME, buflen);
    return NOERROR;
}

static int
HLPassWriteDefault(char *path)
{
    BPTR fh;

    fh = Open(path, MODE_NEWFILE);
    if (fh == 0) {
        return IOERROR;
    }
    Write(fh, "# HotLinks users (username:password)\n", 38);
    Write(fh, "root:\n", 6);
    Close(fh);
    return NOERROR;
}

static int
HLPassEnsureRootLine(char *path)
{
    BPTR fh;
    char line[128];
    char user[32];
    char pwd[64];
    char c;
    LONG got;
    int pos;
    int found;

    fh = Open(path, MODE_OLDFILE);
    if (fh == 0) {
        return HLPassWriteDefault(path);
    }

    found = 0;
    pos = 0;
    while ((got = Read(fh, &c, 1)) == 1) {
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                line[pos] = 0;
                if (line[0] != '#' &&
                    HLPassParseLine(line, user, pwd, (LONG)sizeof(pwd)) &&
                    Stricmp(user, HL_SUPERUSER_NAME) == 0)
                {
                    found = 1;
                    break;
                }
                pos = 0;
            }
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    Close(fh);

    if (!found && pos > 0) {
        line[pos] = 0;
        if (line[0] != '#' &&
            HLPassParseLine(line, user, pwd, (LONG)sizeof(pwd)) &&
            Stricmp(user, HL_SUPERUSER_NAME) == 0)
        {
            found = 1;
        }
    }

    if (found) {
        return NOERROR;
    }

    DeleteFile(path);
    return HLPassWriteDefault(path);
}

static int
HLPassEnsureUsersAt(char *path)
{
    BPTR lock;
    int err;

    if (HLPassPathIsDirectory(path)) {
        return IOERROR;
    }

    lock = Lock(path, ACCESS_READ);
    if (lock == 0) {
        err = HLPassWriteDefault(path);
        if (err != NOERROR) {
            return err;
        }
    } else {
        UnLock(lock);
    }
    return HLPassEnsureRootLine(path);
}

int
HLPassEnsureUsers(struct HLPrivateBase *pb)
{
    char path[280];
    struct HLToken *tok;
    int err;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return IOERROR;
    }

    if (HLPassUsersPath(pb, path, (LONG)sizeof(path)) != NOERROR) {
        return IOERROR;
    }
    err = HLPassEnsureUsersAt(path);
    if (err == NOERROR) {
        return NOERROR;
    }

    if (Stricmp(tok->t_RootPath, HL_ROOT_ASSIGN) != 0) {
        return err;
    }

    tok->t_RootPath[0] = 0;
    Strncpy(tok->t_RootPath, HL_ROOT_FALLBACK, sizeof(tok->t_RootPath) - 1);
    tok->t_RootPath[sizeof(tok->t_RootPath) - 1] = 0;
    if (HLEnsureRootDirs(pb) != NOERROR) {
        return IOERROR;
    }
    if (HLPassUsersPath(pb, path, (LONG)sizeof(path)) != NOERROR) {
        return IOERROR;
    }
    return HLPassEnsureUsersAt(path);
}

static int
HLPassParseLine(char *line, char *name, char *pwdOut, LONG pwdLen)
{
    char *colon;

    colon = strchr(line, ':');
    if (colon == NULL) {
        return 0;
    }
    *colon = 0;
    if (line[0] == 0) {
        return 0;
    }
    Strncpy(name, line, 31);
    name[31] = 0;
    Strncpy(pwdOut, colon + 1, pwdLen - 1);
    pwdOut[pwdLen - 1] = 0;
    {
        LONG n;

        n = (LONG)strlen(pwdOut);
        while (n > 0 && (pwdOut[n - 1] == '\n' || pwdOut[n - 1] == '\r')) {
            pwdOut[n - 1] = 0;
            n--;
        }
    }
    return 1;
}

static int
HLPassFindLine(char *path, char *name, char *pwdOut, LONG pwdLen)
{
    BPTR fh;
    char line[128];
    char user[32];
    char pwd[64];
    char c;
    LONG got;
    int pos;
    int found;

    found = 0;
    fh = Open(path, MODE_OLDFILE);
    if (fh == 0) {
        return INVPARAM;
    }

    pos = 0;
    while ((got = Read(fh, &c, 1)) == 1) {
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                line[pos] = 0;
                if (line[0] != '#') {
                    if (HLPassParseLine(line, user, pwd, (LONG)sizeof(pwd))) {
                        if (Stricmp(user, name) == 0) {
                            Strncpy(pwdOut, pwd, pwdLen - 1);
                            pwdOut[pwdLen - 1] = 0;
                            found = 1;
                            break;
                        }
                    }
                }
                pos = 0;
            }
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    if (!found && pos > 0) {
        line[pos] = 0;
        if (line[0] != '#') {
            if (HLPassParseLine(line, user, pwd, (LONG)sizeof(pwd))) {
                if (Stricmp(user, name) == 0) {
                    Strncpy(pwdOut, pwd, pwdLen - 1);
                    pwdOut[pwdLen - 1] = 0;
                    found = 1;
                }
            }
        }
    }
    Close(fh);
    return found ? NOERROR : INVPARAM;
}

int
HLPassVerify(struct HLPrivateBase *pb, char *name, char *password)
{
    char path[280];
    char stored[64];
    char *pwd;

    if (name == NULL || name[0] == 0) {
        return INVPARAM;
    }
    if (HLPassEnsureUsers(pb) != NOERROR) {
        return IOERROR;
    }
    if (HLPassUsersPath(pb, path, (LONG)sizeof(path)) != NOERROR) {
        return IOERROR;
    }
    if (HLPassFindLine(path, name, stored, (LONG)sizeof(stored)) != NOERROR) {
        return INVPARAM;
    }
    pwd = password;
    if (pwd == NULL) {
        pwd = "";
    }
    if (strcmp(stored, pwd) != 0) {
        return INVPARAM;
    }
    return NOERROR;
}

int
HLPassIsSuperUser(struct HLPrivateBase *pb)
{
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL || !tok->t_UserValid) {
        return 0;
    }
    return (Stricmp(tok->t_UserName, HL_SUPERUSER_NAME) == 0);
}

static int
HLPassRewriteUsers(char *path, char *name, char *newpwd)
{
    BPTR fh;
    BPTR out;
    char line[128];
    char outpath[280];
    char user[32];
    char pwd[64];
    char c;
    LONG got;
    int pos;
    int replaced;

    Strncpy(outpath, path, (LONG)sizeof(outpath) - 1);
    AddPart(outpath, ".new", (LONG)sizeof(outpath));

    fh = Open(path, MODE_OLDFILE);
    if (fh == 0) {
        return IOERROR;
    }
    out = Open(outpath, MODE_NEWFILE);
    if (out == 0) {
        Close(fh);
        return NOMEMORY;
    }

    replaced = 0;
    pos = 0;
    while ((got = Read(fh, &c, 1)) == 1) {
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                line[pos] = 0;
                if (line[0] != '#' &&
                    HLPassParseLine(line, user, pwd, (LONG)sizeof(pwd)) &&
                    Stricmp(user, name) == 0)
                {
                    Write(out, name, (LONG)strlen(name));
                    Write(out, ":", 1);
                    Write(out, newpwd, (LONG)strlen(newpwd));
                    Write(out, "\n", 1);
                    replaced = 1;
                } else {
                    Write(out, line, (LONG)strlen(line));
                    Write(out, "\n", 1);
                }
                pos = 0;
            }
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    Close(fh);

    if (!replaced) {
        Write(out, name, (LONG)strlen(name));
        Write(out, ":", 1);
        Write(out, newpwd, (LONG)strlen(newpwd));
        Write(out, "\n", 1);
    }
    Close(out);

    if (DeleteFile(path) == 0) {
        DeleteFile(outpath);
        return IOERROR;
    }
    if (Rename(outpath, path) == 0) {
        return IOERROR;
    }
    return NOERROR;
}

int
HLPassChange(struct HLPrivateBase *pb, ULONG handle, char *name,
    char *oldpwd, char *newpwd)
{
    char path[280];
    struct HLToken *tok;
    int super;
    int self;

    (void)handle;
    tok = HLTOK(pb);
    if (tok == NULL || name == NULL || name[0] == 0 || newpwd == NULL) {
        return INVPARAM;
    }

    super = HLPassIsSuperUser(pb);
    self = (tok->t_UserValid && Stricmp(tok->t_UserName, name) == 0);

    if (!super && !self) {
        return NOPRIV;
    }

    if (super && !self && Stricmp(name, HL_SUPERUSER_NAME) != 0) {
        (void)oldpwd;
    } else {
        if (oldpwd == NULL) {
            return INVPARAM;
        }
        if (HLPassVerify(pb, name, oldpwd) != NOERROR) {
            return INVPARAM;
        }
    }

    if (HLPassEnsureUsers(pb) != NOERROR) {
        return IOERROR;
    }
    if (HLPassUsersPath(pb, path, (LONG)sizeof(path)) != NOERROR) {
        return IOERROR;
    }
    if (HLPassRewriteUsers(path, name, newpwd) != NOERROR) {
        return NOMEMORY;
    }
    return NOERROR;
}
