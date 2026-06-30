/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hl_req.c - Intuition/gadtools edition and login requesters
 *
 * Uses SAS/C gadtools.library calling convention:
 *   CreateGadget(kind, previous, &ng, tags..., TAG_END)
 *   CreateContext(&glist), GetVisualInfo(), AddGList(), FreeGadgets(glist)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <string.h>

#include "private/hl_bases.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/utility.h>

#include <libraries/hotlinks.h>
#include "private/hl_internal.h"
#include "private/hl_pass.h"
#include "private/hl_req.h"

#define GID_LIST        100
#define GID_OK          101
#define GID_CANCEL      102
#define GID_NAME        110
#define GID_DESC        111
#define GID_ACCESS      112
#define GID_USER        120
#define GID_OLDPWD      121
#define GID_NEWPWD      122
#define GID_CONFIRMPWD  123

#define HLREQ_MAXLIST   256

struct HLPickItem {
    struct HLCatalogEntry *ce;
};

static struct TextAttr HLReqFont = { "topaz.font", 8, 0, 0 };

static void
HLReqTypeName(ULONG type, char *buf, LONG len)
{
    if (type == DTXT) {
        Strncpy(buf, "DTXT", len);
    } else if (type == ILBM) {
        Strncpy(buf, "ILBM", len);
    } else {
        Strncpy(buf, "????", len);
    }
}

static struct Window *
HLReqOpenWindow(struct HLRegistration *reg, LONG w, LONG h, STRPTR title,
    ULONG idcmp)
{
    struct NewWindow nw;
    struct Window *win;

    nw.LeftEdge = 80;
    nw.TopEdge = 40;
    nw.Width = (UWORD)w;
    nw.Height = (UWORD)h;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = idcmp;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = title;
    nw.Screen = NULL;
    nw.BitMap = NULL;
    nw.MinWidth = (UWORD)w;
    nw.MinHeight = (UWORD)h;
    nw.MaxWidth = (UWORD)w;
    nw.MaxHeight = (UWORD)h;
    nw.Flags = WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DEPTHGADGET |
        WFLG_DRAGBAR | WFLG_CLOSEGADGET;

    if (reg != NULL && reg->screen != NULL) {
        nw.Type = CUSTOMSCREEN;
        nw.Screen = reg->screen;
    } else {
        nw.Type = WBENCHSCREEN;
    }

    win = OpenWindow(&nw);
    return win;
}

static void
HLReqCloseWindow(struct Window *win, struct Gadget *glist, APTR vi)
{
    if (win != NULL) {
        if (glist != NULL) {
            RemoveGList(win, glist, -1);
            FreeGadgets(glist);
        }
        CloseWindow(win);
    }
    if (vi != NULL) {
        FreeVisualInfo(vi);
    }
}

static struct Gadget *
HLReqFindGadget(struct Window *win, UWORD id)
{
    struct Gadget *gad;

    gad = (struct Gadget *)win->FirstGadget;
    while (gad != NULL) {
        if (gad->GadgetID == id) {
            return gad;
        }
        gad = gad->NextGadget;
    }
    return NULL;
}

static void
HLReqApplyAccessCycle(ULONG sel, struct PubRecord *rec)
{
    switch (sel) {
    case 1:
        rec->Access = ACC_OREAD | ACC_OWRITE;
        break;
    case 2:
        rec->Access = ACC_OREAD | ACC_OWRITE | ACC_GREAD;
        break;
    case 3:
        rec->Access = ACC_OREAD | ACC_OWRITE | ACC_GREAD | ACC_GWRITE;
        break;
    default:
        rec->Access = ACC_DEFAULT | ACC_AREAD | ACC_AWRITE;
        break;
    }
}

static ULONG
HLReqAccessFromRec(struct PubRecord *rec)
{
    if ((rec->Access & ACC_AREAD) && !(rec->Access & ACC_GREAD)) {
        return 3;
    }
    if ((rec->Access & ACC_GREAD) && !(rec->Access & ACC_AWRITE)) {
        return 2;
    }
    if (!(rec->Access & ACC_AREAD) && !(rec->Access & ACC_AWRITE)) {
        return 1;
    }
    return 0;
}

static struct Gadget *
HLReqAddLabel(struct Gadget *gad, struct NewGadget *ng, STRPTR text,
    WORD left, WORD top, WORD width)
{
    ng->ng_LeftEdge = left;
    ng->ng_TopEdge = top;
    ng->ng_Width = width;
    ng->ng_Height = 14;
    ng->ng_GadgetText = text;
    ng->ng_GadgetID = 0;
    ng->ng_Flags = 0;
    return CreateGadget(TEXT_KIND, gad, ng, TAG_END);
}

static BOOL
HLReqInstallGadgets(struct Window *win, struct Gadget *glist, APTR vi)
{
    (void)vi;
    if (glist == NULL) {
        return FALSE;
    }
    AddGList(win, glist, (UWORD)~0, -1, NULL);
    RefreshGList(glist, win, NULL, -1);
    GT_RefreshWindow(win, NULL);
    ActivateWindow(win);
    return TRUE;
}

int
HLReqLoginUser(struct HLPrivateBase *pb, struct HLRegistration *reg,
    char *nameHint)
{
    struct Window *win;
    struct Gadget *glist;
    struct Gadget *gad;
    APTR vi;
    struct NewGadget ng;
    struct IntuiMessage *imsg;
    ULONG done;
    ULONG rc;
    char name[32];
    char password[32];
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    name[0] = 0;
    password[0] = 0;
    if (nameHint != NULL) {
        Strncpy(name, nameHint, sizeof(name) - 1);
    }

    win = HLReqOpenWindow(reg, 320, 120, (STRPTR)"HotLinks Login",
        IDCMP_GADGETUP | IDCMP_CLOSEWINDOW);
    if (win == NULL) {
        return NOMEMORY;
    }

    vi = GetVisualInfo(win->WScreen, TAG_END);
    if (vi == NULL) {
        CloseWindow(win);
        return NOMEMORY;
    }

    glist = NULL;
    gad = CreateContext(&glist);
    if (gad == NULL) {
        HLReqCloseWindow(win, NULL, vi);
        return NOMEMORY;
    }

    ng.ng_TextAttr = &HLReqFont;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"User:", 20, 18, 70);
    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Password:", 20, 34, 70);

    ng.ng_LeftEdge = 100;
    ng.ng_TopEdge = 16;
    ng.ng_Width = 180;
    ng.ng_Height = 18;
    ng.ng_GadgetText = name;
    ng.ng_GadgetID = GID_USER;
    ng.ng_Flags = 0;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    ng.ng_TopEdge = 32;
    ng.ng_GadgetText = password;
    ng.ng_GadgetID = GID_OLDPWD;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    ng.ng_LeftEdge = 60;
    ng.ng_TopEdge = 72;
    ng.ng_Width = 80;
    ng.ng_Height = 18;
    ng.ng_GadgetText = (STRPTR)"Login";
    ng.ng_GadgetID = GID_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    ng.ng_LeftEdge = 170;
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = GID_CANCEL;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    if (!HLReqInstallGadgets(win, glist, vi)) {
        HLReqCloseWindow(win, glist, vi);
        return NOMEMORY;
    }

    if (nameHint == NULL) {
        ActivateGadget(HLReqFindGadget(win, GID_USER), win, NULL);
    } else {
        ActivateGadget(HLReqFindGadget(win, GID_OLDPWD), win, NULL);
    }

    done = 0;
    rc = IOERROR;
    while (!done) {
        Wait(1L << win->UserPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)) != NULL) {
            if (imsg->Class == IDCMP_CLOSEWINDOW) {
                done = 1;
                rc = IOERROR;
            } else if (imsg->Class == IDCMP_GADGETUP) {
                gad = (struct Gadget *)imsg->IAddress;
                if (gad->GadgetID == GID_OK) {
                    done = 1;
                    if (HLPassVerify(pb, name, password) == NOERROR) {
                        Strncpy(tok->t_UserName, name, 31);
                        tok->t_UserName[31] = 0;
                        if (Stricmp(name, HL_SUPERUSER_NAME) == 0) {
                            Strncpy(tok->t_UserGroup, "admin", 31);
                        } else {
                            Strncpy(tok->t_UserGroup, "users", 31);
                        }
                        tok->t_UserGroup[31] = 0;
                        tok->t_UserValid = TRUE;
                        tok->t_LoginRequired = FALSE;
                        rc = NOERROR;
                    } else {
                        rc = INVPARAM;
                    }
                } else if (gad->GadgetID == GID_CANCEL) {
                    done = 1;
                    rc = IOERROR;
                }
            }
            GT_ReplyIMsg(imsg);
        }
    }

    HLReqCloseWindow(win, glist, vi);
    return (int)rc;
}

int
HLReqEnsureLogin(struct HLPrivateBase *pb, struct HLRegistration *reg)
{
    struct HLToken *tok;

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }
    if (tok->t_UserValid) {
        return NOERROR;
    }
    if (!tok->t_LoginRequired) {
        return NOERROR;
    }
    return HLReqLoginUser(pb, reg, NULL);
}

int
HLReqGetPub(struct HLPrivateBase *pb, struct PubBlock *pblock,
    struct HLPBCtx *ctx, int (*filterproc)(struct PubBlock *))
{
    struct HLToken *tok;
    struct HLCatalogEntry *ce;
    struct MinNode *mn;
    struct Window *win;
    struct Gadget *glist;
    struct Gadget *gad;
    struct Gadget *listgad;
    APTR vi;
    struct NewGadget ng;
    struct IntuiMessage *imsg;
    STRPTR labels[HLREQ_MAXLIST + 1];
    char labelbuf[HLREQ_MAXLIST][72];
    struct HLPickItem items[HLREQ_MAXLIST];
    ULONG count;
    ULONG sel;
    ULONG done;
    int accept;
    int rc;
    char typ[8];

    tok = HLTOK(pb);
    if (tok == NULL) {
        return INVPARAM;
    }

    count = 0;
    for (mn = tok->t_Catalog.mlh_Head;
         mn != (struct MinNode *)&tok->t_Catalog.mlh_Tail;
         mn = mn->mln_Succ)
    {
        ce = (struct HLCatalogEntry *)((BYTE *)mn -
            (BYTE *)&((struct HLCatalogEntry *)0)->hn);
        if (HLCheckAccess(pb, &ce->rec, 0) != NOERROR) {
            continue;
        }
        HLCopyPubRecord(&pblock->PRec, &ce->rec);
        ctx->entry = ce;
        if (filterproc != NULL) {
            accept = filterproc(pblock);
            if (accept == NOACCEPT) {
                continue;
            }
        }
        if (count >= HLREQ_MAXLIST) {
            break;
        }
        items[count].ce = ce;
        HLReqTypeName(ce->rec.Type, typ, (LONG)sizeof(typ));
        labels[count] = labelbuf[count];
        Strncpy(labelbuf[count], ce->rec.Name, sizeof(labelbuf[count]) - 1);
        labelbuf[count][sizeof(labelbuf[count]) - 1] = 0;
        strcat(labelbuf[count], " (");
        strcat(labelbuf[count], typ);
        strcat(labelbuf[count], " v");
        {
            char vbuf[12];
            int n;
            ULONG v;

            v = ce->rec.Version;
            n = 0;
            if (v >= 100) {
                vbuf[n++] = (char)('0' + (v / 100) % 10);
            }
            if (v >= 10) {
                vbuf[n++] = (char)('0' + (v / 10) % 10);
            }
            vbuf[n++] = (char)('0' + v % 10);
            vbuf[n] = 0;
            strcat(labelbuf[count], vbuf);
        }
        strcat(labelbuf[count], ")");
        count++;
    }
    labels[count] = NULL;

    if (count == 0) {
        return NOMOREBLOCKS;
    }

    win = HLReqOpenWindow(ctx->reg, 360, 200, (STRPTR)"Select Edition",
        IDCMP_GADGETUP | IDCMP_CLOSEWINDOW);
    if (win == NULL) {
        return NOMEMORY;
    }

    vi = GetVisualInfo(win->WScreen, TAG_END);
    if (vi == NULL) {
        CloseWindow(win);
        return NOMEMORY;
    }

    glist = NULL;
    gad = CreateContext(&glist);
    if (gad == NULL) {
        HLReqCloseWindow(win, NULL, vi);
        return NOMEMORY;
    }

    ng.ng_TextAttr = &HLReqFont;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;
    ng.ng_LeftEdge = 16;
    ng.ng_TopEdge = 16;
    ng.ng_Width = 328;
    ng.ng_Height = 120;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = GID_LIST;
    ng.ng_Flags = 0;

    listgad = CreateGadget(LISTVIEW_KIND, gad, &ng,
        GTLV_Labels, (ULONG)labels,
        GTLV_ShowSelected, 0,
        TAG_END);

    ng.ng_LeftEdge = 60;
    ng.ng_TopEdge = 152;
    ng.ng_Width = 80;
    ng.ng_Height = 18;
    ng.ng_GadgetText = (STRPTR)"OK";
    ng.ng_GadgetID = GID_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, listgad, &ng, TAG_END);

    ng.ng_LeftEdge = 200;
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = GID_CANCEL;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    if (!HLReqInstallGadgets(win, glist, vi)) {
        HLReqCloseWindow(win, glist, vi);
        return NOMEMORY;
    }

    sel = 0;
    done = 0;
    rc = IOERROR;
    while (!done) {
        Wait(1L << win->UserPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)) != NULL) {
            if (imsg->Class == IDCMP_CLOSEWINDOW) {
                done = 1;
                rc = IOERROR;
            } else if (imsg->Class == IDCMP_GADGETUP) {
                gad = (struct Gadget *)imsg->IAddress;
                if (gad->GadgetID == GID_OK) {
                    GT_GetGadgetAttrs(listgad, win, NULL,
                        GTLV_Selected, &sel, TAG_END);
                    if (sel < count) {
                        ce = items[sel].ce;
                        HLCopyPubRecord(&pblock->PRec, &ce->rec);
                        ctx->entry = ce;
                        rc = NOERROR;
                    }
                    done = 1;
                } else if (gad->GadgetID == GID_CANCEL) {
                    done = 1;
                    rc = IOERROR;
                }
            }
            GT_ReplyIMsg(imsg);
        }
    }

    HLReqCloseWindow(win, glist, vi);
    return rc;
}

static int
HLReqEditionForm(struct HLPrivateBase *pb, struct PubBlock *pblock,
    struct HLRegistration *reg, STRPTR title)
{
    struct Window *win;
    struct Gadget *glist;
    struct Gadget *gad;
    APTR vi;
    struct NewGadget ng;
    struct IntuiMessage *imsg;
    ULONG done;
    int rc;
    char name[32];
    char desc[256];
    ULONG accessSel;
    static STRPTR accessLabels[] = {
        "Default + Public",
        "Owner only",
        "Owner + Group read",
        "Owner + Group",
        NULL
    };

    (void)pb;
    Strncpy(name, pblock->PRec.Name, sizeof(name) - 1);
    Strncpy(desc, pblock->PRec.Desc, sizeof(desc) - 1);
    accessSel = HLReqAccessFromRec(&pblock->PRec);

    win = HLReqOpenWindow(reg, 360, 220, title,
        IDCMP_GADGETUP | IDCMP_CLOSEWINDOW);
    if (win == NULL) {
        return NOMEMORY;
    }

    vi = GetVisualInfo(win->WScreen, TAG_END);
    if (vi == NULL) {
        CloseWindow(win);
        return NOMEMORY;
    }

    glist = NULL;
    gad = CreateContext(&glist);
    if (gad == NULL) {
        HLReqCloseWindow(win, NULL, vi);
        return NOMEMORY;
    }

    ng.ng_TextAttr = &HLReqFont;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Name:", 16, 16, 60);
    ng.ng_LeftEdge = 80;
    ng.ng_TopEdge = 14;
    ng.ng_Width = 260;
    ng.ng_Height = 18;
    ng.ng_GadgetText = name;
    ng.ng_GadgetID = GID_NAME;
    ng.ng_Flags = 0;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Desc:", 16, 44, 60);
    ng.ng_TopEdge = 42;
    ng.ng_GadgetText = desc;
    ng.ng_GadgetID = GID_DESC;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 255, TAG_END);

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Access:", 16, 72, 60);
    ng.ng_TopEdge = 68;
    ng.ng_GadgetText = (STRPTR)accessLabels;
    ng.ng_GadgetID = GID_ACCESS;
    ng.ng_Flags = 0;
    gad = CreateGadget(CYCLE_KIND, gad, &ng,
        GTCY_Labels, (ULONG)accessLabels,
        GTCY_Active, accessSel,
        TAG_END);

    ng.ng_LeftEdge = 60;
    ng.ng_TopEdge = 160;
    ng.ng_Width = 80;
    ng.ng_Height = 18;
    ng.ng_GadgetText = (STRPTR)"OK";
    ng.ng_GadgetID = GID_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    ng.ng_LeftEdge = 200;
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = GID_CANCEL;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    if (!HLReqInstallGadgets(win, glist, vi)) {
        HLReqCloseWindow(win, glist, vi);
        return NOMEMORY;
    }

    ActivateGadget(HLReqFindGadget(win, GID_NAME), win, NULL);

    done = 0;
    rc = IOERROR;
    while (!done) {
        Wait(1L << win->UserPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)) != NULL) {
            if (imsg->Class == IDCMP_CLOSEWINDOW) {
                done = 1;
                rc = IOERROR;
            } else if (imsg->Class == IDCMP_GADGETUP) {
                gad = (struct Gadget *)imsg->IAddress;
                if (gad->GadgetID == GID_OK) {
                    if (name[0] == 0) {
                        rc = INVPARAM;
                    } else {
                        Strncpy(pblock->PRec.Name, name,
                            sizeof(pblock->PRec.Name) - 1);
                        Strncpy(pblock->PRec.Desc, desc,
                            sizeof(pblock->PRec.Desc) - 1);
                        GT_GetGadgetAttrs(HLReqFindGadget(win, GID_ACCESS),
                            win, NULL, GTCY_Active, &accessSel, TAG_END);
                        HLReqApplyAccessCycle(accessSel, &pblock->PRec);
                        rc = NOERROR;
                    }
                    done = 1;
                } else if (gad->GadgetID == GID_CANCEL) {
                    done = 1;
                    rc = IOERROR;
                }
            }
            GT_ReplyIMsg(imsg);
        }
    }

    HLReqCloseWindow(win, glist, vi);
    return rc;
}

int
HLReqPutPub(struct HLPrivateBase *pb, struct PubBlock *pblock)
{
    struct HLPBCtx *ctx;

    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }
    (void)pb;
    return HLReqEditionForm(pb, pblock, ctx->reg, (STRPTR)"New Edition");
}

int
HLReqPubInfo(struct HLPrivateBase *pb, struct PubBlock *pblock,
    struct HLCatalogEntry *ce)
{
    struct HLPBCtx *ctx;

    (void)ce;
    ctx = HLPBCtxFromBlock(pblock);
    if (ctx == NULL) {
        return INVPARAM;
    }
    (void)pb;
    return HLReqEditionForm(pb, pblock, ctx->reg,
        (STRPTR)"Edition Information");
}

int
HLReqNewPassword(struct HLPrivateBase *pb, struct HLRegistration *reg)
{
    struct Window *win;
    struct Gadget *glist;
    struct Gadget *gad;
    APTR vi;
    struct NewGadget ng;
    struct IntuiMessage *imsg;
    ULONG done;
    int rc;
    char name[32];
    char oldpwd[32];
    char newpwd[32];
    char confirm[32];

    name[0] = 0;
    oldpwd[0] = 0;
    newpwd[0] = 0;
    confirm[0] = 0;

    win = HLReqOpenWindow(reg, 320, 180, (STRPTR)"Change Password",
        IDCMP_GADGETUP | IDCMP_CLOSEWINDOW);
    if (win == NULL) {
        return NOMEMORY;
    }

    vi = GetVisualInfo(win->WScreen, TAG_END);
    if (vi == NULL) {
        CloseWindow(win);
        return NOMEMORY;
    }

    glist = NULL;
    gad = CreateContext(&glist);
    if (gad == NULL) {
        HLReqCloseWindow(win, NULL, vi);
        return NOMEMORY;
    }

    ng.ng_TextAttr = &HLReqFont;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"User:", 16, 16, 100);
    ng.ng_LeftEdge = 120;
    ng.ng_TopEdge = 14;
    ng.ng_Width = 180;
    ng.ng_Height = 18;
    ng.ng_GadgetText = name;
    ng.ng_GadgetID = GID_USER;
    ng.ng_Flags = 0;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Old password:", 16, 40, 100);
    ng.ng_TopEdge = 38;
    ng.ng_GadgetText = oldpwd;
    ng.ng_GadgetID = GID_OLDPWD;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"New password:", 16, 64, 100);
    ng.ng_TopEdge = 62;
    ng.ng_GadgetText = newpwd;
    ng.ng_GadgetID = GID_NEWPWD;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    gad = HLReqAddLabel(gad, &ng, (STRPTR)"Confirm:", 16, 88, 100);
    ng.ng_TopEdge = 86;
    ng.ng_GadgetText = confirm;
    ng.ng_GadgetID = GID_CONFIRMPWD;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_MaxChars, 31, TAG_END);

    ng.ng_LeftEdge = 60;
    ng.ng_TopEdge = 130;
    ng.ng_Width = 80;
    ng.ng_Height = 18;
    ng.ng_GadgetText = (STRPTR)"OK";
    ng.ng_GadgetID = GID_OK;
    ng.ng_Flags = PLACETEXT_IN;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    ng.ng_LeftEdge = 170;
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = GID_CANCEL;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    if (!HLReqInstallGadgets(win, glist, vi)) {
        HLReqCloseWindow(win, glist, vi);
        return NOMEMORY;
    }

    ActivateGadget(HLReqFindGadget(win, GID_USER), win, NULL);

    done = 0;
    rc = IOERROR;
    while (!done) {
        Wait(1L << win->UserPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)) != NULL) {
            if (imsg->Class == IDCMP_CLOSEWINDOW) {
                done = 1;
                rc = IOERROR;
            } else if (imsg->Class == IDCMP_GADGETUP) {
                gad = (struct Gadget *)imsg->IAddress;
                if (gad->GadgetID == GID_OK) {
                    if (name[0] == 0 || newpwd[0] == 0) {
                        rc = INVPARAM;
                    } else if (strcmp(newpwd, confirm) != 0) {
                        rc = INVPARAM;
                    } else {
                        rc = HLPassChange(pb, 0, name, oldpwd, newpwd);
                    }
                    done = 1;
                } else if (gad->GadgetID == GID_CANCEL) {
                    done = 1;
                    rc = IOERROR;
                }
            }
            GT_ReplyIMsg(imsg);
        }
    }

    HLReqCloseWindow(win, glist, vi);
    return rc;
}
