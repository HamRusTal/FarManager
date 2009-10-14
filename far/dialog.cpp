/*
dialog.cpp

����� �������

*/

#include "headers.hpp"
#pragma hdrstop

#include "farconst.hpp"
#include "dialog.hpp"
#include "lang.hpp"
#include "fn.hpp"
#include "global.hpp"
#include "macroopcode.hpp"
#include "keys.hpp"
#include "ctrlobj.hpp"
#include "chgprior.hpp"
#include "vmenu.hpp"
#include "dlgedit.hpp"
#include "help.hpp"
#include "scrbuf.hpp"
#include "manager.hpp"
#include "savescr.hpp"
#include "constitle.hpp"
#include "lockscrn.hpp"
#include "TPreRedrawFunc.hpp"
#include "TaskBar.hpp"

#define VTEXT_ADN_SEPARATORS  1

static char HisLocked[16]="Locked", *PHisLocked=NULL;
static char HisLine[16]  ="Line", *PHisLine=NULL;
static char fmtSavedDialogHistory[]="SavedDialogHistory\\%s";

//////////////////////////////////////////////////////////////////////////
/* Public:
   ����������� ������ Dialog
*/
Dialog::Dialog(struct DialogItem *Item,    // ����� ��������� �������
               int ItemCount,              // ���������� ���������
               FARWINDOWPROC DlgProc,      // ���������� ���������
               LONG_PTR InitParam)         // �������������� � �������� ������
{
  _DIALOG(CleverSysLog CL("Create Dialog"));
  _DIALOG(SysLog("Item=%p, ItemCount=%d, DlgProc=%p, Param2=0x%08X",Item,ItemCount,DlgProc,InitParam));
  _tran(SysLog("[%p] Dialog::Dialog()",this));

  if(!PHisLocked) // ���� ��������� �������� �� ���������������� - ������� ��� ������
  {
    PHisLocked=HisLocked+strlen(HisLocked);
    PHisLine=HisLine+strlen(HisLine);
  }
  SetDynamicallyBorn(FALSE); // $OT: �� ��������� ��� ������� ��������� ����������
  /* $ 17.05.2001 DJ */
  CanLoseFocus = FALSE;
  HelpTopic = NULL;
  /* DJ $ */
  /* $ 29.08.2000 SVS
    ����� �������, ���������� ������ (-1 = Main)
  */
  PluginNumber=-1;
  /* SVS $ */
  /* $ 11.08.2000 SVS
    + ������, ������������� ��� ����������� ���������� �������
  */
  Dialog::DataDialog=InitParam;
  /* SVS $ */
  /* $ 10.08.2000 SVS
     ���������� ������� ����� �������
  */
  DialogMode.Set(DMODE_ISCANMOVE);
  /* SVS $ */
  /* $ 23.06.2001 KM */
  SetDropDownOpened(FALSE);
  /* KM $ */
  /* $ 18.08.2000 SVS
  */
  /*
    + ���� IsEnableRedraw - �����������/����������� ����������� �������
      =0 - ���������, ������ �������� - �� ��������������
        ����� ���������� ��������� DMSG_ENABLEREDRAW, �� ���� ����
        ��� Param1=TRUE �������������, ��� Param1 = FALSE - �����������
  */
  IsEnableRedraw=0;

  FocusPos=-1;
  PrevFocusPos=-1;

  if(!DlgProc || IsBadCodePtr((FARPROC)DlgProc)) // ������� ������ ���� ������!!!
  {
    DlgProc=(FARWINDOWPROC)Dialog::DefDlgProc;
    // ����� ������ � ������ ����� - ����� ���� ����!
    DialogMode.Set(DMODE_OLDSTYLE);
  }
  Dialog::RealDlgProc=DlgProc;

  Dialog::Item=Item;
  Dialog::ItemCount=ItemCount;

  if (CtrlObject!=NULL)
  {
    // �������� ����. ����� �����.
    PrevMacroMode=CtrlObject->Macro.GetMode();
    // ��������� ����� � �������� :-)
    CtrlObject->Macro.SetMode(MACRO_DIALOG);
  }
//_SVS(SysLog("Dialog =%d",CtrlObject->Macro.GetMode()));

  // ���������� ���������� ��������� �������
  OldTitle=new ConsoleTitle;
}


//////////////////////////////////////////////////////////////////////////
/* Public, Virtual:
   ���������� ������ Dialog
*/
Dialog::~Dialog()
{
  _tran(SysLog("[%p] Dialog::~Dialog()",this));

  GetDialogObjectsData();
  DeleteDialogObjects();

  if (CtrlObject!=NULL)
    CtrlObject->Macro.SetMode(PrevMacroMode);

  Hide();
  ScrBuf.Flush();

  /* $ 17.05.2001 DJ */
  if (HelpTopic)
    delete [] HelpTopic;
  /* DJ $ */

  /* $ 19.05.2001 DJ
     ���� �� ������� ��������, ������� ��
  */
  if (DialogMode.Check(DMODE_OWNSITEMS))
    delete [] Item;
  /* DJ $ */

  INPUT_RECORD rec;
  PeekInputRecord(&rec);
  delete OldTitle;


  _DIALOG(CleverSysLog CL("Destroy Dialog"));
}

void Dialog::CheckDialogCoord(void)
{
  CriticalSectionLock Lock(CS);

  if ( X1 >= 0 )
    X2 = X1+RealWidth-1;

  if ( Y1 >= 0 )
    Y2 = Y1+RealHeight-1;

  if(X2 > ScrX)
  {
    if(X1 != -1 && X2-X1+1 < ScrX) // ���� �� ��� �� ��������� � �������, ��
    {                              // ���������� ������� ����� �������...
      int D=X2-ScrX;
      X1-=D;
      X2-=D;
    }
  }

  if (X1 < 0) // ������ ������������� ������� �� �����������?
  {             //   X2 ��� ���� = ������ �������.
    X1=(ScrX - X2 + 1)/2;

    if (X1 <= 0) // ������ ������� ������ ������ ������?
    {
      X1=0;
    }
    else
      X2+=X1-1;
  }

  if (Y1 < 0) // ������ ������������� ������� �� ���������?
  {             //   Y2 ��� ���� = ������ �������.
    Y1=(ScrY-Y2+1)/2;

    if(!DialogMode.Check(DMODE_SMALLDIALOG)) //????
      if (Y1>5)
        Y1--;

    if ( Y1<0 )
    {
       Y1=0;
       Y2=ScrY+1;
    }
    else
      Y2+=Y1-1;
  }
}


void Dialog::InitDialog(void)
{
  CriticalSectionLock Lock(CS);

  if (!DialogMode.Check(DMODE_INITOBJECTS))      // ��������������� �������, �����
  {                      //  �������� ���������������� ��� ������ ������.
    /* $ 28.07.2000 SVS
       ������ ���������, ��� � ��� ��� ��!
    */
    CheckDialogCoord();
    int InitFocus=InitDialogObjects();
    int Result=(int)DlgProc((HANDLE)this,DN_INITDIALOG,InitFocus,DataDialog);
    if(ExitCode == -1)
    {
      if(Result)
      {
        // ��� �����, �.�. ������ ����� ���� ��������
        InitFocus=InitDialogObjects(); // InitFocus=????
      }
      SetFarTitle(GetDialogTitle());
    }
    // ��� ������� �������������������!
    DialogMode.Set(DMODE_INITOBJECTS);
    DlgProc((HANDLE)this,DN_GOTFOCUS,InitFocus,0);
  }

  CheckDialogCoord();
}

//////////////////////////////////////////////////////////////////////////
/* Public, Virtual:
   ������ �������� ��������� ���� ������� � ����� �������
   ScreenObject::Show() ��� ������ ������� �� �����.
*/
void Dialog::Show()
{
  CriticalSectionLock Lock(CS);
  _tran(SysLog("[%p] Dialog::Show()",this));

  if(!DialogMode.Check(DMODE_INITOBJECTS))
    return;


  DialogMode.Clear(DMODE_RESIZED);

  if ( Locked() )
    return;

  PreRedrawItem preRedrawItem=PreRedraw.Peek();
  if(preRedrawItem.PreRedrawFunc)
    preRedrawItem.PreRedrawFunc();

  DialogMode.Set(DMODE_SHOW);
  ScreenObject::Show();
}

/* $ 30.08.2000 SVS
  ���� ��������� ������ ������� - ���������� ����������...
*/
void Dialog::Hide()
{
  CriticalSectionLock Lock(CS);
  _tran(SysLog("[%p] Dialog::Hide()",this));
  if(!DialogMode.Check(DMODE_INITOBJECTS))
    return;

  DialogMode.Clear(DMODE_SHOW);
  ScreenObject::Hide();
}
/* SVS $*/

//////////////////////////////////////////////////////////////////////////
/* Private, Virtual:
   ������������� �������� � ����� ������� �� �����.
*/
void Dialog::DisplayObject()
{
  CriticalSectionLock Lock(CS);

  if(DialogMode.Check(DMODE_SHOW))
  {
    ChangePriority ChPriority(THREAD_PRIORITY_NORMAL);
    ShowDialog();          // "��������" ������.
  }
}

// ����������� ���������� ��� ��������� � DIF_CENTERGROUP
void Dialog::ProcessCenterGroup(void)
{
  CriticalSectionLock Lock(CS);

  int I, J;
  int Length,StartX;
  int Type;
  struct DialogItem *CurItem, *JCurItem;
  DWORD ItemFlags;

  for (I=0, CurItem=Item; I < ItemCount; I++, ++CurItem)
  {
    Type=CurItem->Type;
    ItemFlags=CurItem->Flags;

    // ��������������� ����������� �������� � ������ DIF_CENTERGROUP
    // � ���������� ������������ �������� ����� �������������� � �������.
    // �� ���������� X �� �����. ������ ������������ ��� �������������
    // ����� ������.
    if ((ItemFlags & DIF_CENTERGROUP) &&
        (I==0 ||
          (I > 0 &&
            ((Item[I-1].Flags & DIF_CENTERGROUP)==0 ||
             Item[I-1].Y1!=CurItem->Y1)
          )
        )
       )
    {
      Length=0;

      for (J=I, JCurItem=&Item[J]; J < ItemCount &&
                (JCurItem->Flags & DIF_CENTERGROUP) &&
                JCurItem->Y1==CurItem->Y1; J++, ++JCurItem)
      {
        Length+=LenStrItem(J);

//        if (JCurItem->Type==DI_BUTTON && *JCurItem->Data!=' ')
//          Length+=2;
        if (*JCurItem->Data!=' ')
          switch(JCurItem->Type)
          {
            case DI_BUTTON:
              Length+=2;
              break;
            case DI_CHECKBOX:
            case DI_RADIOBUTTON:
              Length+=5;
              break;
          }
      }

//      if (Type==DI_BUTTON && *CurItem->Data!=' ')
//        Length-=2;
      if (*CurItem->Data!=' ')
        switch(Type)
        {
          case DI_BUTTON:
            Length-=2;
            break;
          case DI_CHECKBOX:
          case DI_RADIOBUTTON:
//            Length-=5;
            break;
        }

      StartX=(X2-X1+1-Length)/2;

      if (StartX<0)
        StartX=0;

      for (J=I, JCurItem=&Item[J]; J < ItemCount &&
                (JCurItem->Flags & DIF_CENTERGROUP) &&
                JCurItem->Y1==CurItem->Y1; J++, ++JCurItem)
      {
        JCurItem->X1=StartX;
        StartX+=LenStrItem(J);

//        if (JCurItem->Type==DI_BUTTON && *JCurItem->Data!=' ')
//          StartX+=2;
        if (*JCurItem->Data!=' ')
          switch(JCurItem->Type)
          {
            case DI_BUTTON:
              StartX+=2;
              break;
            case DI_CHECKBOX:
            case DI_RADIOBUTTON:
              StartX+=5;
              break;
          }

        if (StartX == JCurItem->X1)
          JCurItem->X2=StartX;
        else
          JCurItem->X2=StartX-1;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////
/* Public:
   ������������� ��������� �������.
*/
/* $ 28.07.2000 SVS
   ������ InitDialogObjects ���������� ID ��������
   � ������� �����
*/
/* $ 24.08.2000 SVS
  InitDialogObjects ����� �������� - ��� ���������� ���������������
  ���������. ID = -1 - ������� ���� ��������
*/
/*
  TODO: ���������� ��������� ProcessRadioButton ��� �����������
        ������ ��� ������������ (� ����?)
*/
int Dialog::InitDialogObjects(int ID)
{
  CriticalSectionLock Lock(CS);

  int I, J;
  int Type;
  struct DialogItem *CurItem;
  int InitItemCount;
  DWORD ItemFlags;

  _DIALOG(CleverSysLog CL("Init Dialog"));

  if(ID+1 > ItemCount)
    return -1;

  if(ID == -1) // �������������� ���?
  {
    ID=0;
    InitItemCount=ItemCount;
  }
  else
  {
    InitItemCount=ID+1;
  }

  /* 04.01.2001 SVS
     ���� FocusPos � �������� � ������� ����������, �� ���� �������. */
  if(FocusPos >= 0 && FocusPos < ItemCount &&
     (Item[FocusPos].Flags&(DIF_DISABLE|DIF_NOFOCUS|DIF_HIDDEN)))
    FocusPos = -1; // ����� ������ �������!
  /* SVS $ */

  // ��������������� ���� �� ������ ������
  for(I=ID, CurItem=&Item[I]; I < InitItemCount; I++, ++CurItem)
  {
    ItemFlags=CurItem->Flags;
    Type=CurItem->Type;

    // ��� ������ �� ������ ����� "���������� ��������� ������ ��� ������"
    //  ������� ���� ����� ������
    if (Type==DI_BUTTON &&
        (ItemFlags & DIF_NOBRACKETS)==0 &&
        *CurItem->Data != '[')
    {
      char BracketedTitle[200];
      sprintf(BracketedTitle,"[ %.*s ]",sizeof(BracketedTitle)-5,CurItem->Data);
      strcpy(CurItem->Data,BracketedTitle);
    }

     // ��������������� ���� ������
     if(FocusPos == -1 &&
        IsFocused(Type) &&
        CurItem->Focus &&
        !(ItemFlags&(DIF_DISABLE|DIF_NOFOCUS|DIF_HIDDEN)))
       FocusPos=I; // �������� ������ �������� �������
     CurItem->Focus=0; // ������� ��� ����, ����� �� ���������,
                       //   ��� ������� - ��� � ������� ��������

     // ������� ���� DIF_CENTERGROUP ��� ����������
     switch(Type)
     {
       case DI_BUTTON:
       case DI_CHECKBOX:
       case DI_RADIOBUTTON:
       case DI_TEXT:
       case DI_VTEXT: //???
         break;
       default:
         if(ItemFlags&DIF_CENTERGROUP)
           CurItem->Flags&=~DIF_CENTERGROUP;
     }
  }

  // ����� ��� ����� ����� - ������, ���� "����" ������ ���������
  // ���� �� ����, �� ������ �� ������ ����������
  if(FocusPos == -1)
  {
    for (I=0, CurItem=Item; I < ItemCount; I++, ++CurItem) // �� ����!!!!
    {
      if(IsFocused(CurItem->Type) &&
         !(CurItem->Flags&(DIF_DISABLE|DIF_NOFOCUS|DIF_HIDDEN)))
      {
        FocusPos=I;
        break;
      }
    }
  }
  if(FocusPos == -1) // �� �� ����� ���� - ��� �� ������
  {                  //   �������� � ������������ ������
     FocusPos=0;     // ������, ����
  }

  // �� ��� � ��������� ��!
  Item[FocusPos].Focus=1;

  // � ������ ��� ������� � �� ������ ���������...
  ProcessCenterGroup(); // ������� ������������
  for (I=ID, CurItem=&Item[I]; I < InitItemCount; I++, ++CurItem)
  {
    Type=CurItem->Type;
    ItemFlags=CurItem->Flags;

    /* $ 01.08.2000 SVS
       ������� ListBox
    */
    if (Type==DI_LISTBOX)
    {
      if (!DialogMode.Check(DMODE_CREATEOBJECTS))
      {
        CurItem->ListPtr=new VMenu(NULL,NULL,0,CurItem->Y2-CurItem->Y1+1,
                        VMENU_ALWAYSSCROLLBAR|VMENU_LISTBOX,NULL,this);
      }

      if(CurItem->ListPtr)
      {
        VMenu *ListPtr=CurItem->ListPtr;
        ListPtr->SetVDialogItemID(I);
        /* $ 13.09.2000 SVS
           + ���� DIF_LISTNOAMPERSAND. �� ��������� ��� DI_LISTBOX &
             DI_COMBOBOX ������������ ���� MENU_SHOWAMPERSAND. ���� ����
             ��������� ����� ���������
        */
        /* $ 15.05.2001 KM
           ! ���������� ��������� � DI_LISTBOX
        */
        /* $ 03.06.2001 KM
           ! ���������� ��������� � DI_LISTBOX, ������ �� ����� ���� :)
             ��� ���� ������������ ���� DIF_LISTAUTOHIGHLIGHT.
        */
        CurItem->IFlags.Set(DLGIIF_LISTREACTIONFOCUS|DLGIIF_LISTREACTIONNOFOCUS); // ������!

        ListPtr->ChangeFlags(VMENU_DISABLED, ItemFlags&DIF_DISABLE);
        ListPtr->ChangeFlags(VMENU_SHOWAMPERSAND, !(ItemFlags&DIF_LISTNOAMPERSAND));
        ListPtr->ChangeFlags(VMENU_SHOWNOBOX, ItemFlags&DIF_LISTNOBOX);
        ListPtr->ChangeFlags(VMENU_WRAPMODE, ItemFlags&DIF_LISTWRAPMODE);
        ListPtr->ChangeFlags(VMENU_AUTOHIGHLIGHT, ItemFlags&DIF_LISTAUTOHIGHLIGHT);

        if(ItemFlags&DIF_LISTAUTOHIGHLIGHT)
          ListPtr->AssignHighlights(FALSE);

        ListPtr->SetDialogStyle(DialogMode.Check(DMODE_WARNINGSTYLE));
        ListPtr->SetPosition(X1+CurItem->X1,Y1+CurItem->Y1,
                             X1+CurItem->X2,Y1+CurItem->Y2);
        ListPtr->SetBoxType(SHORT_SINGLE_BOX);
        // ���� FarDialogItem.Data ��� DI_LISTBOX ������������ ��� ������� ��������� �����
        if(!(ItemFlags&DIF_LISTNOBOX) && !DialogMode.Check(DMODE_CREATEOBJECTS))
          ListPtr->SetTitle(CurItem->Data);
        // ������ ��� �����
        //ListBox->DeleteItems(); //???? � ���� �� ????
        if(CurItem->ListItems && !DialogMode.Check(DMODE_CREATEOBJECTS))
        {
          ListPtr->AddItem(CurItem->ListItems);
        }

        ListPtr->ChangeFlags (VMENU_LISTHASFOCUS, CurItem->Focus);
    /* DJ $ */
      }
    }
    /* SVS $*/
    // "���������" - �������� ������...
    else if (IsEdit(Type))
    {
      // ������� ���� DIF_EDITOR ��� ������ �����, �������� �� DI_EDIT,
      // DI_FIXEDIT � DI_PSWEDIT
      if(Type != DI_COMBOBOX)
        if((ItemFlags&DIF_EDITOR) && Type != DI_EDIT && Type != DI_FIXEDIT && Type != DI_PSWEDIT)
          ItemFlags&=~DIF_EDITOR;

      if (!DialogMode.Check(DMODE_CREATEOBJECTS))
      {
        CurItem->ObjPtr=new DlgEdit(this,Type == DI_MEMOEDIT?DLGEDIT_MULTILINE:DLGEDIT_SINGLELINE);
        if(Type == DI_COMBOBOX)
        {
          CurItem->ListPtr=new VMenu("",NULL,0,Opt.Dialogs.CBoxMaxHeight,VMENU_ALWAYSSCROLLBAR|VMENU_NOTCHANGE,NULL/*,Parent*/);
        }
        CurItem->SelStart=-1;
      }

      DlgEdit *DialogEdit=(DlgEdit *)CurItem->ObjPtr;
      //DialogEdit->SetDialogParent((Type != DI_COMBOBOX && (ItemFlags & DIF_EDITOR) || (CurItem->Type==DI_PSWEDIT || CurItem->Type==DI_FIXEDIT))?
      //                            FEDITLINE_PARENT_SINGLELINE:FEDITLINE_PARENT_MULTILINE);
      DialogEdit->SetDialogParent(Type == DI_MEMOEDIT?FEDITLINE_PARENT_MULTILINE:FEDITLINE_PARENT_SINGLELINE);
      DialogEdit->SetReadOnly(0);
      /* $ 30.11.200? SVS
         ������� �� ��� ������ ���� DIF_DROPDOWNLIST
      */
      if (Type == DI_COMBOBOX)
      {
        if(CurItem->ListPtr)
        {
          VMenu *ListPtr=CurItem->ListPtr;

          ListPtr->SetBoxType(SHORT_SINGLE_BOX);
          DialogEdit->SetDropDownBox(ItemFlags & DIF_DROPDOWNLIST);
          ListPtr->ChangeFlags(VMENU_WRAPMODE, ItemFlags&DIF_LISTWRAPMODE);
          ListPtr->ChangeFlags(VMENU_DISABLED, ItemFlags&DIF_DISABLE);
          ListPtr->ChangeFlags(VMENU_SHOWAMPERSAND, !(ItemFlags&DIF_LISTNOAMPERSAND));
          ListPtr->ChangeFlags(VMENU_AUTOHIGHLIGHT, ItemFlags&DIF_LISTAUTOHIGHLIGHT);

          if(ItemFlags&DIF_LISTAUTOHIGHLIGHT)
            ListPtr->AssignHighlights(FALSE);

          if(CurItem->ListItems && !DialogMode.Check(DMODE_CREATEOBJECTS))
            ListPtr->AddItem(CurItem->ListItems);
          /* $ 28.04.2002 KM
              ��������� ����, ������������ ������ ��� ���������.
          */
          ListPtr->SetFlags(VMENU_COMBOBOX);
          ListPtr->SetDialogStyle(DialogMode.Check(DMODE_WARNINGSTYLE));
        }
      }

      /* SVS $ */
      /* SVS $ */
      /* $ 15.10.2000 tran
        ������ ���������������� ������ ����� �������� � 511 �������� */
      // ���������� ������������ ������ � ��� ������, ���� �� ��� �� ���������
      if(DialogEdit->GetMaxLength() == -1)
      {
        if((CurItem->Type==DI_EDIT || CurItem->Type==DI_COMBOBOX) &&
           (ItemFlags&DIF_VAREDIT))
          DialogEdit->SetMaxLength(CurItem->Ptr.PtrLength+1);
        else
          DialogEdit->SetMaxLength(((ItemFlags&DIF_EDITPATH)?NM-1:511)+1);
      }
      /* tran $ */
      DialogEdit->SetPosition(X1+CurItem->X1,Y1+CurItem->Y1,
                              X1+CurItem->X2,Y1+CurItem->Y2);
//      DialogEdit->SetObjectColor(
//         FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
//             ((ItemFlags&DIF_DISABLE)?COL_WARNDIALOGEDITDISABLED:COL_WARNDIALOGEDIT):
//             ((ItemFlags&DIF_DISABLE)?COL_DIALOGEDITDISABLED:COL_DIALOGEDIT)),
//         FarColorToReal((ItemFlags&DIF_DISABLE)?COL_DIALOGEDITDISABLED:COL_DIALOGEDITSELECTED));
      if (CurItem->Type==DI_PSWEDIT)
      {
        DialogEdit->SetPasswordMode(TRUE);
        /* $ 01.08.2000 SVS
          ...��� �� ������ �������... � ��� ��������� ������, �.�.
        */
        ItemFlags&=~DIF_HISTORY;
        /* SVS $ */
      }

      if (Type==DI_FIXEDIT)
      {
        /* $ 21.08.2000 SVS
           DIF_HISTORY ����� ����� ������� ���������, ��� DIF_MASKEDIT
        */
        if(ItemFlags&DIF_HISTORY)
          ItemFlags&=~DIF_MASKEDIT;
        /* SVS $ */
        // ���� DI_FIXEDIT, �� ������ ����� �������� �� ������...
        //   ��-�� - ���� ������������������ :-)
        DialogEdit->SetMaxLength(CurItem->X2-CurItem->X1+1+(CurItem->X2==CurItem->X1 || !(ItemFlags&DIF_HISTORY)?0:1));
        DialogEdit->SetOvertypeMode(TRUE);
        /* $ 12.08.2000 KM
           ���� ��� ������ ����� DI_FIXEDIT � ���������� ���� DIF_MASKEDIT
           � �������� �������� CurItem->Mask, �� �������� ����� �������
           ��� ��������� ����� � ������ DlgEdit.
        */
        /* $ 18.09.2000 SVS
          ����� �� ������ ���� ������ (������ �� �������� �� �����������)!
        */
        if ((ItemFlags & DIF_MASKEDIT) && CurItem->Mask)
        {
          char *Ptr=CurItem->Mask;
          while(*Ptr && *Ptr == ' ') ++Ptr;
          if(*Ptr)
            DialogEdit->SetInputMask(CurItem->Mask);
          else
          {
            CurItem->Mask=NULL;
            ItemFlags&=~DIF_MASKEDIT;
          }
        }
        /* SVS $ */
        /* KM $ */
      }
      else
        // "����-��������"
        // ��������������� ������������ ���� ����� (edit controls),
        // ������� ���� ���� ������������ � �������� � ������������
        // ������� � �������� �����
        if (!(ItemFlags & DIF_EDITOR) && CurItem->Type != DI_COMBOBOX)
        {
          DialogEdit->SetEditBeyondEnd(FALSE);
          if (!DialogMode.Check(DMODE_INITOBJECTS))
            DialogEdit->SetClearFlag(1);
        }

        if(CurItem->Type == DI_COMBOBOX)
          DialogEdit->SetClearFlag(1);

      /* $ 01.08.2000 SVS
         ��� �� ����� ���� DIF_USELASTHISTORY � �������� ������ �����,
         �� ��������������� ������ �������� �� History
      */
      if(CurItem->Type==DI_EDIT &&
        (ItemFlags&(DIF_HISTORY|DIF_USELASTHISTORY)) == (DIF_HISTORY|DIF_USELASTHISTORY))
      {
        /* $ 09.12.2001 DJ
           ������� � ��������� �������
        */
        ProcessLastHistory (CurItem, -1);
        /* DJ $ */
      }
      /* SVS $ */
      if((ItemFlags&DIF_MANUALADDHISTORY) && !(ItemFlags&DIF_HISTORY))
        ItemFlags&=~DIF_MANUALADDHISTORY; // ������� �����.

      /* $ 18.03.2000 SVS
         ���� ��� ComBoBox � ������ �� �����������, �� ����� �� ������
         ��� �������, ��� ���� ���� �� ������� ����� Selected != 0
      */
      if (Type==DI_COMBOBOX &&
          (!(ItemFlags&DIF_VAREDIT) && CurItem->Data[0] == 0 ||
            (ItemFlags&DIF_VAREDIT) && *(char*)CurItem->Ptr.PtrData == 0) &&
          CurItem->ListItems)
      {
        struct FarListItem *ListItems=CurItem->ListItems->Items;
        int Length=CurItem->ListItems->ItemsNumber;

        //CurItem->ListPtr->AddItem(CurItem->ListItems);

        for (J=0; J < Length; J++)
        {
          if(ListItems[J].Flags & LIF_SELECTED)
          {
            // ����� ������ ������ ����� ��� ������� ��������������
            if(ItemFlags&DIF_VAREDIT)
            {
              if(ItemFlags & (DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND))
                HiText2Str((char *)CurItem->Ptr.PtrData, CurItem->Ptr.PtrLength-1, ListItems[J].Text);
              else
                xstrncpy((char *)CurItem->Ptr.PtrData, ListItems[J].Text,CurItem->Ptr.PtrLength-1);
            }
            else
            {
              if(ItemFlags & (DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND))
                HiText2Str(CurItem->Data, sizeof(CurItem->Data)-1, ListItems[J].Text);
              else
                xstrncpy(CurItem->Data, ListItems[J].Text,sizeof(CurItem->Data)-1);
            }
            break;
          }
        }
      }

      if((CurItem->Type==DI_EDIT || CurItem->Type==DI_COMBOBOX) && (ItemFlags&DIF_VAREDIT))
        DialogEdit->SetString((char *)CurItem->Ptr.PtrData);
      else
        DialogEdit->SetString(CurItem->Data);

      if (Type==DI_FIXEDIT)
        DialogEdit->SetCurPos(0);

      /* $ 30.08.2001 VVM
        + ��� ������� ����� ������� ���������� ����� */
      if (!(ItemFlags&DIF_EDITOR))
        DialogEdit->SetPersistentBlocks(Opt.Dialogs.EditBlock);
      /*  VVM $ */
      DialogEdit->SetDelRemovesBlocks(Opt.Dialogs.DelRemovesBlocks);
      if(ItemFlags&DIF_READONLY)
        DialogEdit->SetReadOnly(1);

    }
    else if (Type == DI_USERCONTROL)
    {
      if (!DialogMode.Check(DMODE_CREATEOBJECTS))
        CurItem->UCData=new DlgUserControl;
    }

    CurItem->Flags=ItemFlags;
  }
  // ���� ����� ��������, �� ����������� ����� �������.
  SelectOnEntry(FocusPos,TRUE);

  // ��� ������� �������!
  DialogMode.Set(DMODE_CREATEOBJECTS);
  return FocusPos;
}
/* 24.08.2000 SVS $ */


/* $ 19.05.2001 DJ
   ����������� Title �������� � ��������� �������
*/

const char *Dialog::GetDialogTitle()
{
  CriticalSectionLock Lock(CS);

  struct DialogItem *CurItem, *CurItemList=NULL;
  int I;

  for(CurItem=Item,I=0; I < ItemCount; I++, CurItem++)
  {
    // �� ������� ����������� "������" ��������� ��������� �������!
    if ((CurItem->Type==DI_TEXT ||
          CurItem->Type==DI_DOUBLEBOX ||
          CurItem->Type==DI_SINGLEBOX))
    {
      char *Ptr=CurItem->Data;
      for (; *Ptr; Ptr++)
        if (LocalIsalphanum(*Ptr))
          return(Ptr);
    }
    else if(CurItem->Type==DI_LISTBOX && !I)
      CurItemList=CurItem;
  }

  if(CurItemList)
    return (const char *)CurItemList->ListPtr->GetPtrTitle();

  return NULL; //""
}

/* DJ $ */

/* $ 09.12.2001 DJ
   ��������� DIF_USELASTHISTORY �������� � ��������� �������
*/

void Dialog::ProcessLastHistory (struct DialogItem *CurItem, int MsgIndex)
{
  CriticalSectionLock Lock(CS);

  char RegKey[NM];
  char *PtrData;
  int PtrLength;
  if(CurItem->Flags&DIF_VAREDIT)
  {
    PtrData  =(char *)CurItem->Ptr.PtrData;
    PtrLength=CurItem->Ptr.PtrLength;
  }
  else
  {
    PtrData  =CurItem->Data;
    PtrLength=sizeof(CurItem->Data);
  }
  if(!PtrData[0])
  {
    DWORD UseFlags;
    sprintf(RegKey,fmtSavedDialogHistory,(char*)CurItem->History);
    UseFlags=GetRegKey(RegKey,"Flags",1);
    if(UseFlags)
    {
      GetRegKey(RegKey,"Line0",PtrData,"",PtrLength);
      if (MsgIndex != -1)
      {
        // ��������� DM_SETHISTORY => ���� ���������� ��������� ������ �����
        // ���������� �������
        Dialog::SendDlgMessage(this,DM_SETTEXTPTR,MsgIndex,(LONG_PTR)PtrData);
      }
    }
  }
}

/* DJ $ */

/* $ 30.05.2001 KM
   ��������� ��������� �/��� �������� ����� �������.
*/
BOOL Dialog::SetItemRect(int ID,SMALL_RECT *Rect)
{
  CriticalSectionLock Lock(CS);

  if (ID >= ItemCount)
    return FALSE;

  DialogItem *CurItem=&Item[ID];
  int Type=CurItem->Type;

  /* $ 10.08.2001 KM
    - �������� ����������� X1 � 0 ��� Rect->Left=-1 (������������ �����).
  */
  CurItem->X1=(short)Rect->Left;
  /* KM $ */
  CurItem->Y1=(Rect->Top<0)?0:Rect->Top;

  if (IsEdit(Type))
  {
      DlgEdit *DialogEdit=(DlgEdit *)CurItem->ObjPtr;
      CurItem->X2=(short)Rect->Right;
      CurItem->Y2=(short)(Type == DI_MEMOEDIT?Rect->Bottom:0);
      DialogEdit->SetPosition(X1+Rect->Left, Y1+Rect->Top,
                                   X1+Rect->Right,Y1+Rect->Top);
  }
  else if (Type==DI_LISTBOX)
  {
      CurItem->X2=(short)Rect->Right;
      CurItem->Y2=(short)Rect->Bottom;
      CurItem->ListPtr->SetPosition(X1+Rect->Left, Y1+Rect->Top,
                                    X1+Rect->Right,Y1+Rect->Bottom);
      CurItem->ListPtr->SetMaxHeight(CurItem->Y2-CurItem->Y1+1);
  }
  switch(Type)
  {
    case DI_TEXT:
      CurItem->X2=(short)Rect->Right;
      CurItem->Y2=0;                    // ???
      break;

    case DI_VTEXT:
      CurItem->X2=0;                    // ???
      CurItem->Y2=(short)Rect->Bottom;
      break;

    case DI_DOUBLEBOX:
    case DI_SINGLEBOX:
    case DI_USERCONTROL:
      CurItem->X2=(short)Rect->Right;
      CurItem->Y2=(short)Rect->Bottom;
      break;
  }

  if(DialogMode.Check(DMODE_SHOW))
  {
    ShowDialog(-1);
    ScrBuf.Flush();
  }
  return TRUE;
}
/* KM $ */

BOOL Dialog::GetItemRect(int I,RECT& Rect)
{
  CriticalSectionLock Lock(CS);

  if(I >= ItemCount)
    return FALSE;

  struct DialogItem *CurItem=&Item[I];
  DWORD ItemFlags=CurItem->Flags;
  int Type=CurItem->Type;
  int Len=0;

  Rect.left=(int)CurItem->X1;
  Rect.top=(int)CurItem->Y1;
  Rect.right=(int)CurItem->X2;
  Rect.bottom=(int)CurItem->Y2;

  switch(Type)
  {
    case DI_COMBOBOX:
    case DI_EDIT:
    case DI_FIXEDIT:
    case DI_PSWEDIT:
    case DI_LISTBOX:
    case DI_MEMOEDIT:
      break;
    default:
      Len=((ItemFlags & DIF_SHOWAMPERSAND)?(int)strlen(CurItem->Data):HiStrlen(CurItem->Data));
      break;
  }

  switch(Type)
  {
    case DI_TEXT:
      if (CurItem->X1==(short)-1)
        Rect.left=(X2-X1+1-Len)/2;
      if(Rect.left < 0)
        Rect.left=0;

      if (CurItem->Y1==(short)-1)
        Rect.top=(Y2-Y1+1)/2;
      if(Rect.top < 0)
        Rect.top=0;

      Rect.bottom=Rect.top;

      if ( Rect.right == 0 || Rect.right == Rect.left)
        Rect.right=Rect.left+Len-(Len==0?0:1);

      if (ItemFlags & (DIF_SEPARATOR|DIF_SEPARATOR2))
      {
        Rect.bottom=Rect.top;
        Rect.left=(!DialogMode.Check(DMODE_SMALLDIALOG)?3:0); //???
        Rect.right=X2-X1-(!DialogMode.Check(DMODE_SMALLDIALOG)?5:0); //???
      }
      break;

    case DI_VTEXT:
      if (CurItem->X1==(short)-1)
        Rect.left=(X2-X1+1)/2;
      if(Rect.left < 0)
        Rect.left=0;

      if (CurItem->Y1==(short)-1)
        Rect.top=(Y2-Y1+1-Len)/2;
      if(Rect.top < 0)
        Rect.top=0;

      Rect.right=Rect.left;
      //Rect.bottom=Rect.top+Len;

      if ( Rect.bottom == 0 || Rect.bottom == Rect.top)
        Rect.bottom=Rect.top+Len-(Len==0?0:1);

#if defined(VTEXT_ADN_SEPARATORS)
      if (ItemFlags & (DIF_SEPARATOR|DIF_SEPARATOR2))
      {
        Rect.right=Rect.left;
        Rect.top=(!DialogMode.Check(DMODE_SMALLDIALOG)?1:0); //???
        Rect.bottom=Y2-Y1-(!DialogMode.Check(DMODE_SMALLDIALOG)?3:0); //???
        break;
      }
#endif
      break;

    case DI_BUTTON:
      Rect.bottom=Rect.top;
      Rect.right=Rect.left+Len;
      break;

    case DI_CHECKBOX:
    case DI_RADIOBUTTON:
      Rect.bottom=Rect.top;
      Rect.right=Rect.left+Len+((Type == DI_CHECKBOX)?4:
                                 (ItemFlags & DIF_MOVESELECT?3:4)
                               );
      break;

    case DI_COMBOBOX:
    case DI_EDIT:
    case DI_FIXEDIT:
    case DI_PSWEDIT:
      Rect.bottom=Rect.top;
      break;
  }
  return TRUE;
}


//////////////////////////////////////////////////////////////////////////
/* Private:
   ��������� ������ � �������� "����������"
*/
void Dialog::DeleteDialogObjects()
{
  CriticalSectionLock Lock(CS);

  int I;
  struct DialogItem *CurItem;

  for (I=0, CurItem=Item; I < ItemCount; I++, ++CurItem)
  {
    switch(CurItem->Type)
    {
      case DI_MEMOEDIT:
      case DI_EDIT:
      case DI_FIXEDIT:
      case DI_PSWEDIT:
      case DI_COMBOBOX:
        if(CurItem->ObjPtr)
          delete (DlgEdit *)(CurItem->ObjPtr);
      case DI_LISTBOX:
        if((CurItem->Type == DI_COMBOBOX || CurItem->Type == DI_LISTBOX) &&
            CurItem->ListPtr)
           delete CurItem->ListPtr;
        break;
      case DI_USERCONTROL:
        if(CurItem->UCData)
          delete CurItem->UCData;
        break;
    }
    if(CurItem->Flags&DIF_AUTOMATION)
      if(CurItem->AutoPtr)
        xf_free(CurItem->AutoPtr);
  }
}


//////////////////////////////////////////////////////////////////////////
/* Public:
   ��������� �������� �� ����� ��������������.
   ��� ������������� ����� DIF_HISTORY, ��������� ������ � �������.
*/
void Dialog::GetDialogObjectsData()
{
  CriticalSectionLock Lock(CS);

  int I, Type;
  struct DialogItem *CurItem;

  for (I=0,CurItem=Item; I < ItemCount; I++, ++CurItem)
  {
    DWORD IFlags=CurItem->Flags;
    switch(Type=CurItem->Type)
    {
      case DI_MEMOEDIT:
        break; //????
      case DI_EDIT:
      case DI_FIXEDIT:
      case DI_PSWEDIT:
      case DI_COMBOBOX:
      {
        if(CurItem->ObjPtr)
        {
          char *PtrData;
          int PtrLength;
          DlgEdit *EditPtr=(DlgEdit *)(CurItem->ObjPtr);
          // ���������� ������
          if((Type==DI_EDIT || Type==DI_COMBOBOX) && (IFlags&DIF_VAREDIT))
          {
            PtrData  =(char *)CurItem->Ptr.PtrData;
            PtrLength=CurItem->Ptr.PtrLength;
          }
          else
          {
            PtrData  =CurItem->Data;
            PtrLength=sizeof(CurItem->Data);
          }

          // ������� ������
          EditPtr->GetString(PtrData,PtrLength);

          if (ExitCode >=0 &&
              (IFlags & DIF_HISTORY) &&
              !(IFlags & DIF_MANUALADDHISTORY) && // ��� ������� �� ���������
              CurItem->History &&
              Opt.Dialogs.EditHistory)
            AddToEditHistory(PtrData,CurItem->History);

          /* $ 01.08.2000 SVS
             ! � History ������ ��������� �������� (��� DIF_EXPAND...) �����
              ����������� �����!
          */
          /*$ 05.07.2000 SVS $
          �������� - ���� ������� ������������ ���������� ���������� �����?
          �.�. ������� GetDialogObjectsData() ����� ���������� ��������������
          �� ���� ���������!*/
          /* $ 04.12.2000 SVS
            ! ��� DI_PSWEDIT � DI_FIXEDIT ��������� DIF_EDITEXPAND �� �����
             (DI_FIXEDIT ����������� ��� ������ ���� ���� �����)
          */
          if((IFlags&DIF_EDITEXPAND) && Type != DI_PSWEDIT && Type != DI_FIXEDIT)
             ExpandEnvironmentStr(PtrData, PtrData,PtrLength-1);
          /* SVS $ */
          /* SVS $ */
          /* 01.08.2000 SVS $ */
        }
        break;
      }

      case DI_LISTBOX:
      /*
        if(CurItem->ListPtr)
        {
          CurItem->ListPos=CurItem->ListPtr->GetSelectPos();
          break;
        }
      */
        break;
      /**/
    }
#if 0
    if((Type == DI_COMBOBOX || Type == DI_LISTBOX) &&
       CurItem->ListPtr && CurItem->ListItems &&
       DlgProc == Dialog::DefDlgProc)
    {
      int ListPos=CurItem->ListPtr->GetSelectPos();
      if(ListPos < CurItem->ListItems->ItemsNumber)
      {
        for(int J=0; J < CurItem->ListItems->ItemsNumber; ++J)
          CurItem->ListItems->Items[J].Flags&=~LIF_SELECTED;
        CurItem->ListItems->Items[ListPos].Flags|=LIF_SELECTED;
      }
    }
#else
    if((Type == DI_COMBOBOX || Type == DI_LISTBOX))
    {
      CurItem->ListPos=CurItem->ListPtr?CurItem->ListPtr->GetSelectPos():0;
    }
#endif
  }
}


// ������� ������������ � ������� ������.
LONG_PTR Dialog::CtlColorDlgItem(int ItemPos,int Type,int Focus,DWORD Flags)
{
  CriticalSectionLock Lock(CS);

  BOOL DisabledItem=Flags&DIF_DISABLE?TRUE:FALSE;
  DWORD Attr=0;

  switch(Type)
  {
    case DI_SINGLEBOX:
    case DI_DOUBLEBOX:
    {
      if (Flags & DIF_SETCOLOR)
        Attr=Flags & DIF_COLORMASK;
      else
      {
        Attr=DialogMode.Check(DMODE_WARNINGSTYLE) ?
                    (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBOX):
                    (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBOX);
      }

      Attr=MAKELONG(
          MAKEWORD(FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                      (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBOXTITLE):
                      (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBOXTITLE)
                   ), // Title LOBYTE
                   FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                      (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGHIGHLIGHTBOXTITLE):
                      (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGHIGHLIGHTBOXTITLE)
                   )
          ),// HiText HIBYTE
          MAKEWORD(FarColorToReal(Attr), // Box LOBYTE
                   0)                     // HIBYTE
      );
      break;
    }

#if defined(VTEXT_ADN_SEPARATORS)
    case DI_VTEXT:
#endif
    case DI_TEXT:
    {
      if (Flags & DIF_SETCOLOR)
        Attr=Flags & DIF_COLORMASK;
      else
      {
        if (Flags & DIF_BOXCOLOR)
          Attr=DialogMode.Check(DMODE_WARNINGSTYLE) ?
                (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBOX):
                (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBOX);
        else
          Attr=DialogMode.Check(DMODE_WARNINGSTYLE) ?
                (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGTEXT):
                (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGTEXT);
      }

      Attr=MAKELONG(
         MAKEWORD(FarColorToReal(Attr),
                 FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                    (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGHIGHLIGHTTEXT):
                    (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGHIGHLIGHTTEXT))), // HIBYTE HiText
           ((Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2))?
             (MAKEWORD(FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBOX):
                (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBOX)), // Box LOBYTE
               0))
             :
             0));
      break;
    }

#if !defined(VTEXT_ADN_SEPARATORS)
    case DI_VTEXT:
    {
      if (Flags & DIF_BOXCOLOR)
        Attr=DialogMode.Check(DMODE_WARNINGSTYLE) ?
                 (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBOX):
                 (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBOX);
      else if (Flags & DIF_SETCOLOR)
        Attr=(Flags & DIF_COLORMASK);
      else
        Attr=(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                 (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGTEXT):
                 (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGTEXT));
      Attr=MAKEWORD(MAKEWORD(FarColorToReal(Attr),0),MAKEWORD(0,0));
      break;
    }
#endif

    case DI_CHECKBOX:
    case DI_RADIOBUTTON:
    {
      if (Flags & DIF_SETCOLOR)
        Attr=(Flags & DIF_COLORMASK);
      else
        Attr=(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGTEXT):
                (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGTEXT));

      Attr=MAKEWORD(FarColorToReal(Attr),
           FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                 (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGHIGHLIGHTTEXT):
                 (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGHIGHLIGHTTEXT))); // HiText
      break;
    }

    case DI_BUTTON:
    {
      if (Focus)
      {
        SetCursorType(0,10);
        Attr=MAKEWORD(
           (Flags & DIF_SETCOLOR)?(Flags & DIF_COLORMASK):
             FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                 (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGSELECTEDBUTTON):
                 (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGSELECTEDBUTTON)), // TEXT
           FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                 (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGHIGHLIGHTSELECTEDBUTTON):
                 (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGHIGHLIGHTSELECTEDBUTTON))); // HiText
      }
      else
      {
        Attr=MAKEWORD(
           (Flags & DIF_SETCOLOR)?(Flags & DIF_COLORMASK):
             FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                    (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGBUTTON):
                    (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGBUTTON)), // TEXT
           FarColorToReal(DialogMode.Check(DMODE_WARNINGSTYLE) ?
                    (DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGHIGHLIGHTBUTTON):
                    (DisabledItem?COL_DIALOGDISABLED:COL_DIALOGHIGHLIGHTBUTTON))); // HiText
      }
      break;
    }

    case DI_EDIT:
    case DI_FIXEDIT:
    case DI_PSWEDIT:
    case DI_COMBOBOX:
    case DI_MEMOEDIT:
    {
      /* $ 15.08.2000 SVS
         ! ��� DropDownList ����� ������������ �� �����
      */
      if(Type == DI_COMBOBOX && (Flags & DIF_DROPDOWNLIST))
      {
        if(DialogMode.Check(DMODE_WARNINGSTYLE))
          Attr=MAKELONG(
            MAKEWORD( //LOWORD
              // LOLO (Text)
              FarColorToReal(DisabledItem?COL_WARNDIALOGEDITDISABLED:COL_WARNDIALOGEDIT),
              // LOHI (Select)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:COL_DIALOGEDITSELECTED)
            ),
            MAKEWORD( //HIWORD
              // HILO (Unchanged)
              FarColorToReal(DisabledItem?COL_WARNDIALOGEDITDISABLED:COL_DIALOGEDITUNCHANGED), //???
              // HIHI (History)
              FarColorToReal(DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGTEXT)
            )
          );
        else
          Attr=MAKELONG(
            MAKEWORD( //LOWORD
              // LOLO (Text)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:(!Focus?COL_DIALOGEDIT:COL_DIALOGEDITSELECTED)),
              // LOHI (Select)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:(!Focus?COL_DIALOGEDIT:COL_DIALOGEDITSELECTED))
            ),
            MAKEWORD( //HIWORD
              // HILO (Unchanged)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:COL_DIALOGEDITUNCHANGED), //???
              // HIHI (History)
              FarColorToReal(DisabledItem?COL_DIALOGDISABLED:COL_DIALOGTEXT)
            )
          );
      }
      else
      {
        if(DialogMode.Check(DMODE_WARNINGSTYLE))
          Attr=MAKELONG(
            MAKEWORD( //LOWORD
              // LOLO (Text)
              FarColorToReal(DisabledItem?COL_WARNDIALOGEDITDISABLED:
                (Flags&DIF_NOFOCUS?COL_DIALOGEDITUNCHANGED:COL_WARNDIALOGEDIT)),
              // LOHI (Select)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:COL_DIALOGEDITSELECTED)
            ),
            MAKEWORD( //HIWORD
              // HILO (Unchanged)
              FarColorToReal(DisabledItem?COL_WARNDIALOGEDITDISABLED:COL_DIALOGEDITUNCHANGED), //???
              // HIHI (History)
              FarColorToReal(DisabledItem?COL_WARNDIALOGDISABLED:COL_WARNDIALOGTEXT)
            )
          );
        else
          Attr=MAKELONG(
            MAKEWORD( //LOWORD
              // LOLO (Text)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:
                (Flags&DIF_NOFOCUS?COL_DIALOGEDITUNCHANGED:COL_DIALOGEDIT)),
              // LOHI (Select)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:COL_DIALOGEDITSELECTED)
            ),
            MAKEWORD( //HIWORD
              // HILO (Unchanged)
              FarColorToReal(DisabledItem?COL_DIALOGEDITDISABLED:COL_DIALOGEDITUNCHANGED), //???
              // HIHI (History)
              FarColorToReal(DisabledItem?COL_DIALOGDISABLED:COL_DIALOGTEXT)
            )
          );
      }
      /* SVS $ */
      /* SVS $ */
      break;
    }

    case DI_LISTBOX:
    {
      Item[ItemPos].ListPtr->SetColors(NULL);
      return 0;
    }

    default:
    {
      return 0;
    }
  }
  return DlgProc((HANDLE)this,DN_CTLCOLORDLGITEM,ItemPos,Attr);
}


//////////////////////////////////////////////////////////////////////////
/* $ 22.08.2000 SVS
  ! ShowDialog - �������������� �������� - ����� ������� ������������
*/
/* Private:
   ��������� ��������� ������� �� ������.
*/
void Dialog::ShowDialog(int ID)
{
  CriticalSectionLock Lock(CS);

  if ( Locked () )
    return;

  char Str[1024];
  struct DialogItem *CurItem;
  int X,Y;
  int I,DrawItemCount;
  unsigned long Attr;

  //   ���� �� ��������� ���������, �� ����������.
  if(IsEnableRedraw ||                 // ��������� ���������� ?
     (ID+1 > ItemCount) ||             // � ����� � ������ ������������?
     DialogMode.Check(DMODE_DRAWING) || // ������ ��������?
     !DialogMode.Check(DMODE_SHOW) ||   // ���� �� �����, �� � �� ������������.
     !DialogMode.Check(DMODE_INITOBJECTS))
    return;

  DialogMode.Set(DMODE_DRAWING);  // ������ ��������!!!

  ChangePriority ChPriority(THREAD_PRIORITY_NORMAL);

  if(ID == -1) // ������ ���?
  {
    //   ����� ����������� ������� �������� ��������� � ����������
    if(!DlgProc((HANDLE)this,DN_DRAWDIALOG,0,0))
    {
      DialogMode.Clear(DMODE_DRAWING);  // ����� ��������� �������!!!
      return;
    }

    //   ����� ����������� �������� ���� �������...
    if(!DialogMode.Check(DMODE_NODRAWSHADOW))
      Shadow();              // ... "�������" ����

    if(!DialogMode.Check(DMODE_NODRAWPANEL))
    {
      Attr=(DWORD)DlgProc((HANDLE)this,DN_CTLCOLORDIALOG,0,
          DialogMode.Check(DMODE_WARNINGSTYLE) ? COL_WARNDIALOGTEXT:COL_DIALOGTEXT);
      SetScreen(X1,Y1,X2,Y2,' ',Attr);
    }

    ID=0;
    DrawItemCount=ItemCount;
  }
  else
  {
    DrawItemCount=ID+1;
  }

  //IFlags.Set(DIMODE_REDRAW)
  /* TODO:
     ���� �������� ������� � �� Z-order`� �� ������������ �
     ������ ��������� (�� �����������), �� ��� "��������"
     �������� ���� ����� ����������.
  */
  {
    int CursorVisible=0,CursorSize=0;
    if(ID != -1 && FocusPos != ID)
    {
      if(Item[FocusPos].Type == DI_USERCONTROL && Item[FocusPos].UCData->CursorPos.X != -1 && Item[FocusPos].UCData->CursorPos.Y != -1)
      {
        CursorVisible=Item[FocusPos].UCData->CursorVisible;
        CursorSize=Item[FocusPos].UCData->CursorSize;
      }
    }
    SetCursorType(CursorVisible,CursorSize);
  }

  for (I=ID,CurItem=&Item[I]; I < DrawItemCount; I++, ++CurItem)
  {
    if(CurItem->Flags&DIF_HIDDEN)
      continue;

    /* $ 28.07.2000 SVS
       ����� ����������� ������� �������� �������� ���������
       ����������� ������� SendDlgMessage - � ��� �������� ���!
    */
    if(!Dialog::SendDlgMessage((HANDLE)this,DN_DRAWDLGITEM,I,0))
       continue;

    int LenText;
    short CX1=CurItem->X1;
    short CY1=CurItem->Y1;
    short CX2=CurItem->X2;
    short CY2=CurItem->Y2;

    if ( CX2 > X2-X1 )
      CX2 = X2-X1;

    if ( CY2 > Y2-Y1 )
      CY2 = Y2-Y1;

    short CW=CX2-CX1+1;
    short CH=CY2-CY1+1;
    BOOL DisabledItem=CurItem->Flags&DIF_DISABLE?TRUE:FALSE;

    Attr=(unsigned long)CtlColorDlgItem(I,CurItem->Type,CurItem->Focus,CurItem->Flags);

#if 0
    // TODO: ������ ��� ��� ������ ���������... ����� ��������� _���_ ������� �� ������� X2, Y2. !!!
    if ( ((CX1 > -1) && (CX2 > 0) && (CX2 > CX1)) &&
       ((CY1 > -1) && (CY2 > 0) && (CY2 > CY1)) )
      SetScreen(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,' ',Attr&0xFF);
#endif

    switch(CurItem->Type)
    {
/* ***************************************************************** */
      case DI_SINGLEBOX:
      case DI_DOUBLEBOX:
      {
        BOOL IsDrawTitle=TRUE;
        GotoXY(X1+CX1,Y1+CY1);
        SetColor(LOBYTE(HIWORD(Attr)));
        if(CY1 == CY2)
          DrawLine(CX2-CX1+1,CurItem->Type==DI_SINGLEBOX?8:9); //???
        else if(CX1 == CX2)
        {
          DrawLine(CY2-CY1+1,CurItem->Type==DI_SINGLEBOX?10:11);
          IsDrawTitle=FALSE;
        }
        else
          Box(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,
             LOBYTE(HIWORD(Attr)),
             (CurItem->Type==DI_SINGLEBOX) ? SINGLE_BOX:DOUBLE_BOX);

        if (*CurItem->Data && IsDrawTitle)
        {
          /* $ 17.12.2001 KM
            ! ����� ������ ��� ��������� � ������ ������������ ���������.
          */
          xstrncpy(Str,CurItem->Data,sizeof(Str)-3);
          TruncStrFromEnd(Str,CW-2); // 5 ???
          LenText=LenStrItem(I,Str);
          if(LenText < CW-2)
          {
            memmove(Str+1,Str,strlen(Str)+1);
            LenText=(int)strlen(Str);
            *Str=Str[LenText]=' ';
            Str[LenText+1]=0;
            LenText=LenStrItem(I,Str);
          }
          X=X1+CX1+(CW-LenText)/2;

          if ((CurItem->Flags & DIF_LEFTTEXT) && X1+CX1+1 < X)
            X=X1+CX1+1;

          SetColor(Attr&0xFF);
          GotoXY(X,Y1+CY1);
          if (CurItem->Flags & DIF_SHOWAMPERSAND)
            Text(Str);
          else
            HiText(Str,HIBYTE(LOWORD(Attr)));
        }
        break;
      }

/* ***************************************************************** */
      case DI_TEXT:
      {
        xstrncpy(Str,CurItem->Data,sizeof(Str)-1);
        LenText=LenStrItem(I,Str);
        if (!(CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2)) && (CurItem->Flags & DIF_CENTERTEXT) && CX1!=-1)
          LenText=LenStrItem(I,CenterStr(Str,Str,CX2-CX1+1));

        X=(CX1==-1 || (CurItem->Flags & (DIF_SEPARATOR|DIF_SEPARATOR2)))?(X2-X1+1-LenText)/2:CX1;
        Y=(CY1==-1)?(Y2-Y1+1)/2:CY1;

        if(X < 0)
          X=0;

        if ( (CX2 <= 0) || (CX2 < CX1) )
          CW = LenText;

        if(X1+X+LenText > X2)
        {
          int tmpCW=ObjWidth;
          if(CW < ObjWidth)
            tmpCW=CW+1;
          Str[tmpCW-1]=0;
        }

        // ����� ���
        //SetScreen(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,' ',Attr&0xFF);
        // ������ �����:
        if(CX1 > -1 && CX2 > CX1 && !(CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2))) //������������ �������
        {
          int CntChr=CX2-CX1+1;
          SetColor(Attr&0xFF);
          GotoXY(X1+X,Y1+Y);
          if(X1+X+CntChr-1 > X2)
            CntChr=X2-(X1+X)+1;
          mprintf("%*s",CntChr,"");
          if (CntChr < LenText)
            Str[CntChr]=0;
        }

        if (CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2))
        {
          SetColor(LOBYTE(HIWORD(Attr)));
          GotoXY(X1+((CurItem->Flags&DIF_SEPARATORUSER)?X:(!DialogMode.Check(DMODE_SMALLDIALOG)?3:0)),Y1+Y); //????

          ShowUserSeparator((CurItem->Flags&DIF_SEPARATORUSER)?X2-X1+1:RealWidth-(!DialogMode.Check(DMODE_SMALLDIALOG)?6:0/* -1 */),
                            (CurItem->Flags&DIF_SEPARATORUSER)?12:(CurItem->Flags&DIF_SEPARATOR2?3:1),
                             CurItem->Mask
                           );
        }

        SetColor(Attr&0xFF);
        GotoXY(X1+X,Y1+Y);

        if (CurItem->Flags & DIF_SHOWAMPERSAND)
          Text(Str);
        else
          HiText(Str,HIBYTE(LOWORD(Attr)));
        break;
      }

/* ***************************************************************** */
      case DI_VTEXT:
      {
        xstrncpy(Str,CurItem->Data,sizeof(Str)-1);
        LenText=LenStrItem(I,Str); // strlen(Str); ???
        if (!(CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2)) && (CurItem->Flags & DIF_CENTERTEXT) && CY1!=-1)
          LenText=(int)strlen(CenterStr(Str,Str,CY2-CY1+1));

        X=(CX1==-1)?(X2-X1+1)/2:CX1;
        Y=(CY1==-1 || (CurItem->Flags & (DIF_SEPARATOR|DIF_SEPARATOR2)))?(Y2-Y1+1-LenText)/2:CY1;

        if(Y < 0)
          Y=0;

        if ( (CY2 <= 0) || (CY2 < CY1) )
          CH = LenStrItem(I,Str);

        if(Y1+Y+LenText > Y2)
        {
          int tmpCH=ObjHeight;
          if(CH < ObjHeight)
            tmpCH=CH+1;
          Str[tmpCH-1]=0;
        }

        // ����� ���
        //SetScreen(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,' ',Attr&0xFF);
        // ������ �����:
        if(CY1 > -1 && CY2 > 0 && CY2 > CY1 && !(CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2))) //������������ �������
        {
          int CntChr=CY2-CY1+1;
          SetColor(Attr&0xFF);
          GotoXY(X1+X,Y1+Y);
          if(Y1+Y+CntChr-1 > Y2)
            CntChr=Y2-(Y1+Y)+1;
          vmprintf("%*s",CntChr,"");
        }


#if defined(VTEXT_ADN_SEPARATORS)
        if (CurItem->Flags & (DIF_SEPARATORUSER|DIF_SEPARATOR|DIF_SEPARATOR2))
        {
          SetColor(LOBYTE(HIWORD(Attr)));
          GotoXY(X1+X,Y1+ ((CurItem->Flags&DIF_SEPARATORUSER)?Y:(!DialogMode.Check(DMODE_SMALLDIALOG)?1:0)) ); //????

          ShowUserSeparator((CurItem->Flags&DIF_SEPARATORUSER)?Y2-Y1+1:RealHeight-(!DialogMode.Check(DMODE_SMALLDIALOG)?2:0),
                        (CurItem->Flags&DIF_SEPARATORUSER)?13:(CurItem->Flags&DIF_SEPARATOR2?7:5),
                        CurItem->Mask
                       );
        }
#endif

        SetColor(Attr&0xFF);
        GotoXY(X1+X,Y1+Y);
        if (CurItem->Flags & DIF_SHOWAMPERSAND)
          VText(Str);
        else
          HiVText(Str,HIBYTE(LOWORD(Attr)));
        break;
      }

/* ***************************************************************** */
      case DI_CHECKBOX:
      case DI_RADIOBUTTON:
      {
        SetColor(Attr&0xFF);

        GotoXY(X1+CX1,Y1+CY1);
        const char *AddSpace=strlen(CurItem->Data) > 0?" ":"";

        if (CurItem->Type==DI_CHECKBOX)
        {
          char *Chk3State=MSG(MCheckBox2State);
          sprintf(Str,"[%c]%s",(CurItem->Selected ?
             (((CurItem->Flags&DIF_3STATE) && CurItem->Selected == 2)?
                *Chk3State:'x'):' '),AddSpace);
        }
        else
        {
          if (CurItem->Flags & DIF_MOVESELECT)
            sprintf(Str," %c ",CurItem->Selected ? '\07':' ');
          else
            sprintf(Str,"(%c)%s",(CurItem->Selected ? '\07':' '),AddSpace);
        }

        xstrncat(Str,CurItem->Data,sizeof(Str)-1);
        LenText=LenStrItem(I,Str);
        if(X1+CX1+LenText > X2)
          Str[ObjWidth-1]=0;

        if (CurItem->Flags & DIF_SHOWAMPERSAND)
          Text(Str);
        else
          HiText(Str,HIBYTE(LOWORD(Attr)));

        if (CurItem->Focus)
        {
          /* $ 09.08.2000 KM
             ���������� ��������� ������� ��� ����������� �������
          */
          if (!DialogMode.Check(DMODE_DRAGGED))
            SetCursorType(1,-1);
          MoveCursor(X1+CX1+1,Y1+CY1);
          /* KM $ */
        }

        break;
      }

/* ***************************************************************** */
      case DI_BUTTON:
      {
        xstrncpy(Str,CurItem->Data,sizeof(Str)-1);
        //LenText=LenStrItem(I,Str);
        /*
           �����, ��� ���������, ���������� ������ ���� DIF_NOBRACKETS
           �.�. � ������ DIF_NOBRACKETS ������ �������� ��� "%s", ���
           ����� ����� - "[ %s ]"
        */

        SetColor(Attr&0xFF);
        GotoXY(X1+CX1,Y1+CY1);

        if (CurItem->Flags & DIF_SHOWAMPERSAND)
          Text(Str);
        else
          HiText(Str,HIBYTE(LOWORD(Attr)));
        break;
      }

/* ***************************************************************** */
      /* $ 18.07.2000 SVS
         + ��������� �������� DI_COMBOBOX
      */
      case DI_EDIT:
      case DI_FIXEDIT:
      case DI_PSWEDIT:
      case DI_MEMOEDIT:
      case DI_COMBOBOX:
      {
        DlgEdit *EditPtr=(DlgEdit *)(CurItem->ObjPtr);
        if(!EditPtr)
          break;

        EditPtr->SetObjectColor(Attr&0xFF,HIBYTE(LOWORD(Attr)),LOBYTE(HIWORD(Attr)));

        if (CurItem->Focus)
        {
          /* $ 09.08.2000 KM
             ���������� ��������� ������� ��� ����������� �������
          */
          if (!DialogMode.Check(DMODE_DRAGGED))
            SetCursorType(1,-1);
          EditPtr->Show();
        }
        else
        {
          EditPtr->FastShow();
          EditPtr->SetLeftPos(0);
        }

        /* $ 09.08.2000 KM
           ���������� ��������� ������� ��� ����������� �������
        */
        if (DialogMode.Check(DMODE_DRAGGED))
          SetCursorType(0,0);
        /* KM $ */

        if ((CurItem->History && (CurItem->Flags & DIF_HISTORY) && Opt.Dialogs.EditHistory) ||
            (CurItem->Type == DI_COMBOBOX && CurItem->ListPtr && CurItem->ListPtr->GetItemCount() > 0))
        {
          int EditX1,EditY1,EditX2,EditY2;

          EditPtr->GetPosition(EditX1,EditY1,EditX2,EditY2);
          //Text((CurItem->Type == DI_COMBOBOX?"\x1F":"\x19"));
          Text(EditX2+1,EditY1,HIBYTE(HIWORD(Attr)),"\x19");
        }

        if (CurItem->Type == DI_COMBOBOX && GetDropDownOpened() && CurItem->ListPtr->IsVisible()) // need redraw VMenu?
        {
          CurItem->ListPtr->Hide();
          //if(CurItem->ListPtr->GetItemCount() > 0)
            CurItem->ListPtr->Show();
        }
        break;
        /* SVS $ */
      }

/* ***************************************************************** */
      /* $ 01.08.2000 SVS
         ������� ListBox
      */
      case DI_LISTBOX:
      {
        if(CurItem->ListPtr)
        {
          /* $ 21.08.2000 SVS
             ����� ���������� ������� �� ��������� �������� ���������
          */
          BYTE RealColors[VMENU_COLOR_COUNT];
          struct FarListColors ListColors={0};
          ListColors.ColorCount=VMENU_COLOR_COUNT;
          ListColors.Colors=RealColors;

          CurItem->ListPtr->GetColors(&ListColors);
          if(DlgProc((HANDLE)this,DN_CTLCOLORDLGLIST,I,(LONG_PTR)&ListColors))
            CurItem->ListPtr->SetColors(&ListColors);
          /* SVS $ */
          // ������ ����������...
          int CurSorVisible,CurSorSize;
          GetCursorType(CurSorVisible,CurSorSize);
          /* $ 23.02.2002 DJ
             ������ � ��� ���� ���� => Show() ������ ������ ���������� ���������
          */
          CurItem->ListPtr->Show();
          /* DJ $ */
          // .. � ������ �����������!
          if(FocusPos != I)
            SetCursorType(CurSorVisible,CurSorSize);
        }
        break;
      }
      /* 01.08.2000 SVS $ */


/* ***************************************************************** */
      case DI_USERCONTROL:
        if(CurItem->VBuf)
        {
#if defined(USE_WFUNC)
          if (!(CurItem->Flags & DIF_NOTCVTUSERCONTROL))
            PutTextA(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,CurItem->VBuf);
          else
#endif
            PutText(X1+CX1,Y1+CY1,X1+CX2,Y1+CY2,CurItem->VBuf);
          // �� ������� ����������� ������, ���� �� ��������������.
          if(FocusPos == I)
          {
            if(CurItem->UCData->CursorPos.X != -1 &&
               CurItem->UCData->CursorPos.Y != -1)
            {
              MoveCursor(CurItem->UCData->CursorPos.X+CX1+X1,CurItem->UCData->CursorPos.Y+CY1+Y1);
              SetCursorType(CurItem->UCData->CursorVisible,CurItem->UCData->CursorSize);
            }
            else
              SetCursorType(0,-1);
          }
        }
        break; //��� ����������� :-)))

/* ***************************************************************** */
    } // end switch(...
  } // end for (I=...

  // �������!
  // �� �������� ;-)
  for (I=0; I < ItemCount; I++)
  {
    CurItem=&Item[I];
    if(CurItem->ListPtr && GetDropDownOpened() && CurItem->ListPtr->IsVisible())
    {
      if((CurItem->Type == DI_COMBOBOX) ||
         ((CurItem->Type == DI_EDIT || CurItem->Type == DI_FIXEDIT) &&
         !(CurItem->Flags&DIF_HIDDEN) &&
         (CurItem->Flags&DIF_HISTORY)))
      {
        CurItem->ListPtr->Show();
      }
    }
  }
  /* $ 31.07.2000 SVS
     ������� ��������� �����������...
  */
  if (!DialogMode.Check(DMODE_DRAGGED)) // ���� ������ ���������
  {
    /* $ 03.06.2001 KM
       + ��� ������ ����������� �������, ����� ������ �����������, �������������
         ��������� �������, � ��������� ������ �� �� ������ ����������������.
    */
    SetFarTitle(GetDialogTitle());
    /* KM $ */
  }

  DialogMode.Clear(DMODE_DRAWING);  // ����� ��������� �������!!!
  DialogMode.Set(DMODE_SHOW); // ������ �� ������!

  if (DialogMode.Check(DMODE_DRAGGED))
  {
    /*
    - BugZ#813 - DM_RESIZEDIALOG � DN_DRAWDIALOG -> ��������: Ctrl-F5 - ��������� ������ ��������.
      ������� ����� ���������� �����������.
    */
    //DlgProc((HANDLE)this,DN_DRAWDIALOGDONE,1,0);
    Dialog::DefDlgProc((HANDLE)this,DN_DRAWDIALOGDONE,1,0);
  }
  else
    DlgProc((HANDLE)this,DN_DRAWDIALOGDONE,0,0);
}
/* SVS 22.08.2000 $ */

int Dialog::LenStrItem(int ID,char *Str)
{
  CriticalSectionLock Lock(CS);

  if(!Str)
    Str=Item[ID].Data;
  return (Item[ID].Flags & DIF_SHOWAMPERSAND)?(int)strlen(Str):HiStrlen(Str);
}


int Dialog::ProcessMoveDialog(DWORD Key)
{
  CriticalSectionLock Lock(CS);

  int I;
  /* $ 31.07.2000 tran
     + ����������� ������� �� ������ */
  if (DialogMode.Check(DMODE_DRAGGED)) // ���� ������ ���������
  {
    // TODO: ����� ��������� "��� �����" � �� ������ ������ ��������
    //       �.�., ���� ������ End, �� ��� ��������� End ������� ������ ������! - �������� ���������� !!!
    int rr=1;
    /* $ 15.12.2000 SVS
       ��� ����������� ������� ��������� ��������� "�����������" ����.
    */
    switch (Key)
    {
        case KEY_CTRLLEFT:  case KEY_CTRLNUMPAD4:
        case KEY_CTRLHOME:  case KEY_CTRLNUMPAD7:
        case KEY_HOME:      case KEY_NUMPAD7:
            rr=Key == KEY_CTRLLEFT || Key == KEY_CTRLNUMPAD4?10:X1;
        case KEY_LEFT:      case KEY_NUMPAD4:
            Hide();
            for ( I=0; I<rr; I++ )
                if ( X2>0 )
                {
                    X1--;
                    X2--;
                    AdjustEditPos(-1,0);
                }
            if(!DialogMode.Check(DMODE_ALTDRAGGED)) Show();
            break;
        case KEY_CTRLRIGHT: case KEY_CTRLNUMPAD6:
        case KEY_CTRLEND:   case KEY_CTRLNUMPAD1:
        case KEY_END:       case KEY_NUMPAD1:
            rr=Key == KEY_CTRLRIGHT || Key == KEY_CTRLNUMPAD6?10:Max(0,ScrX-X2);
        case KEY_RIGHT:     case KEY_NUMPAD6:
            Hide();
            for ( I=0; I<rr; I++ )
                if ( X1<ScrX )
                {
                    X1++;
                    X2++;
                    AdjustEditPos(1,0);
                }
            if(!DialogMode.Check(DMODE_ALTDRAGGED)) Show();
            break;
        case KEY_PGUP:      case KEY_NUMPAD9:
        case KEY_CTRLPGUP:  case KEY_CTRLNUMPAD9:
        case KEY_CTRLUP:    case KEY_CTRLNUMPAD8:
            rr=Key == KEY_CTRLUP || Key == KEY_CTRLNUMPAD8?5:Y1;
        case KEY_UP:        case KEY_NUMPAD8:
            Hide();
            for ( I=0; I<rr; I++ )
                if ( Y2>0 )
                {
                    Y1--;
                    Y2--;
                    AdjustEditPos(0,-1);
                }
            if(!DialogMode.Check(DMODE_ALTDRAGGED)) Show();
            break;
        case KEY_CTRLDOWN:  case KEY_CTRLNUMPAD2:
        case KEY_CTRLPGDN:  case KEY_CTRLNUMPAD3:
        case KEY_PGDN:      case KEY_NUMPAD3:
            rr=Key == KEY_CTRLDOWN || Key == KEY_CTRLNUMPAD2? 5:Max(0,ScrY-Y2);
        case KEY_DOWN:      case KEY_NUMPAD2:
            Hide();
            for ( I=0; I<rr; I++ )
                if ( Y1<ScrY )
                {
                    Y1++;
                    Y2++;
                    AdjustEditPos(0,1);
                }
            if(!DialogMode.Check(DMODE_ALTDRAGGED)) Show();
            break;
        case KEY_NUMENTER:
        case KEY_ENTER:
        case KEY_CTRLF5:
            DialogMode.Clear(DMODE_DRAGGED); // �������� ��������!
            if(!DialogMode.Check(DMODE_ALTDRAGGED))
            {
              DlgProc((HANDLE)this,DN_DRAGGED,1,0);
              Show();
            }
            break;
        case KEY_ESC:
            Hide();
            AdjustEditPos(OldX1-X1,OldY1-Y1);
            X1=OldX1;
            X2=OldX2;
            Y1=OldY1;
            Y2=OldY2;
            DialogMode.Clear(DMODE_DRAGGED);
            if(!DialogMode.Check(DMODE_ALTDRAGGED))
            {
              DlgProc((HANDLE)this,DN_DRAGGED,1,TRUE);
              Show();
            }
            break;
    }
    /* SVS $ */
    if(DialogMode.Check(DMODE_ALTDRAGGED))
    {
      DialogMode.Clear(DMODE_DRAGGED|DMODE_ALTDRAGGED);
      DlgProc((HANDLE)this,DN_DRAGGED,1,0);
      Show();
    }
    return (TRUE);
  }
  /* $ 10.08.2000 SVS
     �������, ���� ���������! (IsCanMove)
  */
  if (Key == KEY_CTRLF5 && DialogMode.Check(DMODE_ISCANMOVE))
  {
    if(DlgProc((HANDLE)this,DN_DRAGGED,0,0)) // ���� ��������� ����������!
    {
      // �������� ���� � ���������� ����������
      DialogMode.Set(DMODE_DRAGGED);
      OldX1=X1; OldX2=X2; OldY1=Y1; OldY2=Y2;
      //# GetText(0,0,3,0,LV);
      Show();
    }
    return (TRUE);
  }
  /* tran 31.07.2000 $ */
  return (FALSE);
}


__int64 Dialog::VMProcess(int OpCode,void *vParam,__int64 iParam)
{
  switch(OpCode)
  {
    case MCODE_F_MENU_CHECKHOTKEY:
    {
      const char *str = (const char *)vParam;
      if ( GetDropDownOpened() || Item[FocusPos].Type == DI_LISTBOX)
      {
        if(Item[FocusPos].ListPtr)
          return Item[FocusPos].ListPtr->VMProcess(OpCode,vParam,iParam);
      }
      else
        return (__int64)(CheckHighlights(*str,(int)iParam)+1);
      return _i64(0);
    }
  }
  switch(OpCode)
  {
    case MCODE_C_EOF:
    case MCODE_C_BOF:
    case MCODE_C_SELECTED:
    case MCODE_C_EMPTY:
    {
      if (IsEdit(Item[FocusPos].Type))
        return ((DlgEdit *)(Item[FocusPos].ObjPtr))->VMProcess(OpCode,vParam,iParam);
      else if(Item[FocusPos].Type == DI_LISTBOX && OpCode != MCODE_C_SELECTED)
        return Item[FocusPos].ListPtr->VMProcess(OpCode,vParam,iParam);
      return _i64(0);
    }

    case MCODE_V_DLGITEMTYPE: // Dlg.ItemType
    {
      switch(Item[FocusPos].Type)
      {
        case DI_BUTTON:      return _i64(7); // ������ (Push Button).
        case DI_CHECKBOX:    return _i64(8); // ����������� ������������� (Check Box).
        case DI_COMBOBOX:    return (__int64)(DropDownOpened?0x800A:10); // ��������������� ������.
        case DI_DOUBLEBOX:   return _i64(3); // ������� �����.
        case DI_EDIT:        return (__int64)(DropDownOpened?0x8004:4); // ���� �����.
        case DI_FIXEDIT:     return _i64(6); // ���� ����� �������������� �������.
        case DI_LISTBOX:     return _i64(11); // ���� ������.
        case DI_PSWEDIT:     return _i64(5); // ���� ����� ������.
        case DI_RADIOBUTTON: return _i64(9); // ����������� ������ (Radio Button).
        case DI_SINGLEBOX:   return _i64(2); // ��������� �����.
        case DI_TEXT:        return _i64(0); // ��������� ������.
        case DI_USERCONTROL: return _i64(255); // ������� ����������, ������������ �������������.
        case DI_VTEXT:       return _i64(1); // ������������ ��������� ������.
      }
      return _i64(-1);
    }

    case MCODE_V_DLGITEMCOUNT: // Dlg.ItemCount
    {
      return (__int64)ItemCount;
    }

    case MCODE_V_DLGCURPOS:    // Dlg.CurPos
    {
      return (__int64)(FocusPos+1);
    }

    case MCODE_V_ITEMCOUNT:
    case MCODE_V_CURPOS:
    {
      switch(Item[FocusPos].Type)
      {
        case DI_COMBOBOX:
           if(DropDownOpened || (Item[FocusPos].Flags & DIF_DROPDOWNLIST))
             return Item[FocusPos].ListPtr->VMProcess(OpCode,vParam,iParam);
        case DI_EDIT:
        case DI_PSWEDIT:
        case DI_FIXEDIT:
           return ((DlgEdit *)(Item[FocusPos].ObjPtr))->VMProcess(OpCode,vParam,iParam);
        case DI_LISTBOX:
          return Item[FocusPos].ListPtr->VMProcess(OpCode,vParam,iParam);

        case DI_USERCONTROL:
          if(OpCode == MCODE_V_CURPOS)
            return (__int64)(Item[FocusPos].UCData->CursorPos.X);
        case DI_BUTTON:
        case DI_CHECKBOX:
        case DI_RADIOBUTTON:
          return _i64(0);
      }
      return _i64(0);
    }

    case MCODE_F_EDITOR_SEL:
    {
      if (IsEdit(Item[FocusPos].Type) || (Item[FocusPos].Type==DI_COMBOBOX && !(DropDownOpened || (Item[FocusPos].Flags & DIF_DROPDOWNLIST))))
      {
        return ((DlgEdit *)(Item[FocusPos].ObjPtr))->VMProcess(OpCode,vParam,iParam);
      }
      return _i64(0);
    }

  }
  return _i64(0);
}

//////////////////////////////////////////////////////////////////////////
/* Public, Virtual:
   ��������� ������ �� ����������.
   ����������� BaseInput::ProcessKey.
*/
int Dialog::ProcessKey(int Key)
{
  CriticalSectionLock Lock(CS);
  _DIALOG(CleverSysLog CL("Dialog::ProcessKey"));
  _DIALOG(SysLog("Param: Key=%s",_FARKEY_ToName(Key)));

  int I;
  char Str[1024];

  if (Key==KEY_NONE || Key==KEY_IDLE)
  {
    DlgProc((HANDLE)this,DN_ENTERIDLE,0,0); // $ 28.07.2000 SVS ��������� ���� ���� � ���������� :-)
    return(FALSE);
  }

  if(Key == KEY_KILLFOCUS || Key == KEY_GOTFOCUS)
  {
    DlgProc((HANDLE)this,DN_ACTIVATEAPP,Key == KEY_KILLFOCUS?FALSE:TRUE,0);
    return(FALSE);
  }

  if(ProcessMoveDialog(Key))
    return TRUE;

  // BugZ#488 - Shift=enter
  if(ShiftPressed && (Key == KEY_ENTER||Key==KEY_NUMENTER) && !CtrlObject->Macro.IsExecuting() && Item[FocusPos].Type != DI_BUTTON)
  {
    Key=Key == KEY_ENTER?KEY_SHIFTENTER:KEY_SHIFTNUMENTER;
  }

  if(!(/*Key>=KEY_MACRO_BASE && Key <=KEY_MACRO_ENDBASE || */ Key>=KEY_OP_BASE && Key <=KEY_OP_ENDBASE) && !DialogMode.Check(DMODE_KEY))
    if(DlgProc((HANDLE)this,DN_KEY,FocusPos,Key))
      return TRUE;

  if(!DialogMode.Check(DMODE_SHOW))
      return TRUE;

  // � ��, ����� � ���� ������ ���������� ��������� ��������!
  if(Item[FocusPos].Flags&DIF_HIDDEN)
    return TRUE;

  // ��������� �����������
  if(Item[FocusPos].Type==DI_CHECKBOX)
  {
    if(!(Item[FocusPos].Flags&DIF_3STATE))
    {
      if(Key == KEY_MULTIPLY) // � CheckBox 2-state Gray* �� ��������!
        Key = KEY_NONE;
      if((Key == KEY_ADD      && !Item[FocusPos].Selected) ||
         (Key == KEY_SUBTRACT &&  Item[FocusPos].Selected))
       Key=KEY_SPACE;
    }
    /*
      ���� else �� �����, �.�. ���� ������� ����� ����������...
    */
  }
  else if(Key == KEY_ADD)
    Key='+';
  else if(Key == KEY_SUBTRACT)
    Key='-';
  else if(Key == KEY_MULTIPLY)
    Key='*';

  if (Item[FocusPos].Type==DI_BUTTON && Key == KEY_SPACE)
    Key=KEY_ENTER;

  if(Item[FocusPos].Type == DI_LISTBOX)
  {
    switch(Key)
    {
      case KEY_HOME:     case KEY_NUMPAD7:
      case KEY_LEFT:     case KEY_NUMPAD4:
      case KEY_END:      case KEY_NUMPAD1:
      case KEY_RIGHT:    case KEY_NUMPAD6:
      case KEY_UP:       case KEY_NUMPAD8:
      case KEY_DOWN:     case KEY_NUMPAD2:
      case KEY_PGUP:     case KEY_NUMPAD9:
      case KEY_PGDN:     case KEY_NUMPAD3:
      case KEY_MSWHEEL_UP:
      case KEY_MSWHEEL_DOWN:
      case KEY_MSWHEEL_LEFT:
      case KEY_MSWHEEL_RIGHT:
      case KEY_ENTER:
      case KEY_NUMENTER:
        VMenu *List=Item[FocusPos].ListPtr;
        int CurListPos=List->GetSelectPos();
        int CheckedListItem=List->GetSelection(-1);

        List->ProcessKey(Key);

        int NewListPos=List->GetSelectPos();
        if(NewListPos != CurListPos && !DlgProc((HANDLE)this,DN_LISTCHANGE,FocusPos,NewListPos))
        {
          if(!DialogMode.Check(DMODE_SHOW))
              return TRUE;

          List->SetSelection(CheckedListItem,CurListPos);
          if(DialogMode.Check(DMODE_SHOW) && !(Item[FocusPos].Flags&DIF_HIDDEN))
            ShowDialog(FocusPos); // FocusPos
        }

        if(!(Key == KEY_ENTER || Key == KEY_NUMENTER) || (Item[FocusPos].Flags&DIF_LISTNOCLOSE))
          return(TRUE);

    }
  }

  switch(Key)
  {
//    case KEY_CTRLTAB:
//      DLGIIF_EDITPATH
    case KEY_F1:
      // ����� ������� ������� �������� ��������� � ����������
      //   � ���� ������� ��� ����, �� ������� ���������
      if(Help::MkTopic(PluginNumber,
                 (char*)DlgProc((HANDLE)this,DN_HELP,FocusPos,
                                (HelpTopic?(LONG_PTR)HelpTopic:0)),
                 Str))
      {
        Help Hlp (Str);
      }
      return(TRUE);

    case KEY_ESC:
    case KEY_BREAK:
    case KEY_F10:
      ExitCode=(Key==KEY_BREAK) ? -2:-1;
      CloseDialog();
      return(TRUE);


    case KEY_HOME: case KEY_NUMPAD7:
      if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
        return TRUE;
      return Do_ProcessFirstCtrl();

    case KEY_TAB:
    case KEY_SHIFTTAB:
      return Do_ProcessTab(Key==KEY_TAB);

    case KEY_SPACE:
      return Do_ProcessSpace();


    case KEY_CTRLNUMENTER:
    case KEY_CTRLENTER:
    {
      for (I=0;I<ItemCount;I++)
        if (Item[I].DefaultButton)
        {
          if(Item[I].Flags&DIF_DISABLE)
          {
             // ProcessKey(KEY_DOWN); // �� ���� ���� :-)
             return TRUE;
          }
          if (!IsEdit(Item[I].Type))
            Item[I].Selected=1;
          ExitCode=I;
          /* $ 18.05.2001 DJ */
          CloseDialog();
          /* DJ $ */
          return(TRUE);
        }
      if(!DialogMode.Check(DMODE_OLDSTYLE))
      {
        DialogMode.Clear(DMODE_ENDLOOP); // ������ ���� ����
        return TRUE; // ������ ������ �� ����
      }
    }

    case KEY_NUMENTER:
    case KEY_ENTER:
    {
      if (Item[FocusPos].Type != DI_COMBOBOX && IsEdit(Item[FocusPos].Type) &&  (Item[FocusPos].Flags & DIF_EDITOR) && !(Item[FocusPos].Flags & DIF_READONLY))
      {
        int EditorLastPos;
        for (EditorLastPos=I=FocusPos;I<ItemCount;I++)
          if (IsEdit(Item[I].Type) && (Item[I].Flags & DIF_EDITOR))
            EditorLastPos=I;
          else
            break;
        if (((DlgEdit *)(Item[EditorLastPos].ObjPtr))->GetLength()!=0)
          return(TRUE);
        for (I=EditorLastPos;I>FocusPos;I--)
        {
          int CurPos;
          if (I==FocusPos+1)
            CurPos=((DlgEdit *)(Item[I-1].ObjPtr))->GetCurPos();
          else
            CurPos=0;
          ((DlgEdit *)(Item[I-1].ObjPtr))->GetString(Str,sizeof(Str));
          int Length=(int)strlen(Str);
          ((DlgEdit *)(Item[I].ObjPtr))->SetString(CurPos>=Length ? "":Str+CurPos);
          if (CurPos<Length)
            Str[CurPos]=0;
          ((DlgEdit *)(Item[I].ObjPtr))->SetCurPos(0);
          ((DlgEdit *)(Item[I-1].ObjPtr))->SetString(Str);
          /* $ 28.07.2000 SVS
            ��� ��������� ��������� ������� �������� �������� ���������
            ����������� ������� SendDlgMessage - � ��� �������� ���!
          */
          Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,I-1,0);
          Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,I,0);
          /* SVS $ */
        }
        if (EditorLastPos > FocusPos)
        {
          ((DlgEdit *)(Item[FocusPos].ObjPtr))->SetCurPos(0);
          Do_ProcessNextCtrl(FALSE,FALSE);
        }
        ShowDialog();
        return(TRUE);
      }
      else if (Item[FocusPos].Type==DI_BUTTON)
      {
        Item[FocusPos].Selected=1;
        // ��������� - "������ ��������"
        if(Dialog::SendDlgMessage((HANDLE)this,DN_BTNCLICK,FocusPos,0))
          return TRUE;

        if(Item[FocusPos].Flags&DIF_BTNNOCLOSE)
          return(TRUE);

        ExitCode=FocusPos;
        CloseDialog();
        return TRUE;
      }
      else
      {
        ExitCode=-1;
        for (I=0;I<ItemCount;I++)
        {
          if (Item[I].DefaultButton && !(Item[I].Flags&DIF_BTNNOCLOSE))
          {
            if(Item[I].Flags&DIF_DISABLE)
            {
               // ProcessKey(KEY_DOWN); // �� ���� ���� :-)
               return TRUE;
            }
//            if (!(IsEdit(Item[I].Type) || Item[I].Type == DI_CHECKBOX || Item[I].Type == DI_RADIOBUTTON))
//              Item[I].Selected=1;
            ExitCode=I;
            break;
          }
        }
      }

      if (ExitCode==-1)
        ExitCode=FocusPos;

      CloseDialog();
      return(TRUE);
    }

    /* $ 04.12.2000 SVS
       3-� ��������� ���������
       ��� �������� ���� ������� ������ � ������, ���� �������
       ����� ���� DIF_3STATE
    */
    case KEY_ADD:
    case KEY_SUBTRACT:
    case KEY_MULTIPLY:
      if (Item[FocusPos].Type==DI_CHECKBOX)
      {
        unsigned int CHKState=
           (Key == KEY_ADD?1:
            (Key == KEY_SUBTRACT?0:
             ((Key == KEY_MULTIPLY)?2:
              Item[FocusPos].Selected)));
        if(Item[FocusPos].Selected != (int)CHKState)
          if(Dialog::SendDlgMessage((HANDLE)this,DN_BTNCLICK,FocusPos,CHKState))
          {
             Item[FocusPos].Selected=CHKState;
             ShowDialog();
          }
      }
      return(TRUE);
    /* SVS 22.11.2000 $ */

    case KEY_LEFT:  case KEY_NUMPAD4: case KEY_MSWHEEL_LEFT:
    case KEY_RIGHT: case KEY_NUMPAD6: case KEY_MSWHEEL_RIGHT:
    {
      if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
        return TRUE;
      if (IsEdit(Item[FocusPos].Type))
      {
        ((DlgEdit *)(Item[FocusPos].ObjPtr))->ProcessKey(Key);
        return(TRUE);
      }
      else
      {
        int MinDist=1000,MinPos=0;
        for (I=0;I<ItemCount;I++)
        {
          if (I!=FocusPos &&
              (IsEdit(Item[I].Type) ||
               Item[I].Type==DI_CHECKBOX ||
               Item[I].Type==DI_RADIOBUTTON) &&
              Item[I].Y1==Item[FocusPos].Y1)
          {
            int Dist=Item[I].X1-Item[FocusPos].X1;
            if ((Key==KEY_LEFT||Key==KEY_SHIFTNUMPAD4) && Dist<0 || (Key==KEY_RIGHT||Key==KEY_SHIFTNUMPAD6) && Dist>0)
              if (abs(Dist)<MinDist)
              {
                MinDist=abs(Dist);
                MinPos=I;
              }
          }
        }
        if (MinDist<1000)
        {
          ChangeFocus2(FocusPos,MinPos);
          if (Item[MinPos].Flags & DIF_MOVESELECT)
          {
            Do_ProcessSpace();
          }
          else
          {
            ShowDialog();
          }
          return(TRUE);
        }
      }
    }

    case KEY_UP:    case KEY_NUMPAD8:
    case KEY_DOWN:  case KEY_NUMPAD2:
      if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
        return TRUE;
      return Do_ProcessNextCtrl(Key==KEY_LEFT || Key==KEY_UP || Key == KEY_NUMPAD4 || Key == KEY_NUMPAD8);

    // $ 27.04.2001 VVM - ��������� ������ �����
    case KEY_MSWHEEL_UP:
    case KEY_MSWHEEL_DOWN:
    case KEY_CTRLUP:      case KEY_CTRLNUMPAD8:
    case KEY_CTRLDOWN:    case KEY_CTRLNUMPAD2:
      return ProcessOpenComboBox(Item[FocusPos].Type,(Item+FocusPos),FocusPos);

    // ��� ����� default �������������!!!
    case KEY_END:  case KEY_NUMPAD1:
      if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
        return TRUE;
      if (IsEdit(Item[FocusPos].Type))
      {
        ((DlgEdit *)(Item[FocusPos].ObjPtr))->ProcessKey(Key);
        return(TRUE);
      }
    // ��� ����� default ���������!!!
    case KEY_PGDN:   case KEY_NUMPAD3:
      if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
        return TRUE;
      if (!(Item[FocusPos].Flags & DIF_EDITOR))
      {
        for (I=0;I<ItemCount;I++)
          if (Item[I].DefaultButton)
          {
            ChangeFocus2(FocusPos,I);
            ShowDialog();
            return(TRUE);
          }
        return(TRUE);
      }
      // ��� DIF_EDITOR ����� ���������� ����

    default:
    {
      //if(Item[FocusPos].Type == DI_USERCONTROL) // ��� user-���� ����������
      //  return TRUE;

      if(Item[FocusPos].Type == DI_LISTBOX)
      {
        VMenu *List=Item[FocusPos].ListPtr;
        int CurListPos=List->GetSelectPos();
        int CheckedListItem=List->GetSelection(-1);

        List->ProcessKey(Key);
        int NewListPos=List->GetSelectPos();
        if(NewListPos != CurListPos && !DlgProc((HANDLE)this,DN_LISTCHANGE,FocusPos,NewListPos))
        {
          if(!DialogMode.Check(DMODE_SHOW))
              return TRUE;
          List->SetSelection(CheckedListItem,CurListPos);
          if(DialogMode.Check(DMODE_SHOW) && !(Item[FocusPos].Flags&DIF_HIDDEN))
            ShowDialog(FocusPos); // FocusPos
        }
        return(TRUE);
      }

      /* $ 21.08.2000 SVS
         Autocomplete ��� ���������� ������ � ������� ����������� ;-)
      */
      if (IsEdit(Item[FocusPos].Type))
      {
        DlgEdit *edt=(DlgEdit *)Item[FocusPos].ObjPtr;
        int SelStart, SelEnd;

        if(Key == KEY_CTRLL) // �������� ����� ������ RO ��� ���� ����� � ����������
        {
          return TRUE;
        }

        /* $ 11.09.2000 SVS
           Ctrl-U � ������� ����� ������� ������� �����
        */
        else if(Key == KEY_CTRLU)
        {
          edt->SetClearFlag(0);
          edt->Select(-1,0);
          edt->Show();
          return TRUE;
        }
        /* SVS $ */

        else if((Item[FocusPos].Flags & DIF_EDITOR) && !(Item[FocusPos].Flags & DIF_READONLY))
        {
          switch(Key)
          {
            /* $ 12.09.2000 SVS
              ���������� �������� � BackSpace � DIF_EDITOR
            */
            case KEY_BS:
            {
              int CurPos=edt->GetCurPos();
              /* $ 21.11.2000 SVS
                 �� ��������� ��������� ������ � ������������� ���������
              */
              // � ������ ������????
              if(!edt->GetCurPos())
              {
                // � "����" ���� DIF_EDITOR?
                if(FocusPos > 0 && (Item[FocusPos-1].Flags&DIF_EDITOR))
                {
                  // ��������� � ����������� �...
                  DlgEdit *edt_1=(DlgEdit *)Item[FocusPos-1].ObjPtr;
                  edt_1->GetString(Str,sizeof(Str));
                  CurPos=(int)strlen(Str);
                  edt->GetString(Str+CurPos,sizeof(Str)-CurPos);
                  edt_1->SetString(Str);

                  for (I=FocusPos+1;I<ItemCount;I++)
                  {
                    if (Item[I].Flags & DIF_EDITOR)
                    {
                      if (I>FocusPos)
                      {
                        ((DlgEdit *)(Item[I].ObjPtr))->GetString(Str,sizeof(Str));
                        ((DlgEdit *)(Item[I-1].ObjPtr))->SetString(Str);
                      }
                      ((DlgEdit *)(Item[I].ObjPtr))->SetString("");
                    }
                    else // ���, ������  FocusPos ��� ���� ��������� �� DIF_EDITOR
                    {
                      ((DlgEdit *)(Item[I-1].ObjPtr))->SetString("");
                      break;
                    }
                  }
                  Do_ProcessNextCtrl(TRUE);
                  edt_1->SetCurPos(CurPos);
                }
              }
              /* SVS $ */
              else
              {
                edt->ProcessKey(Key);
              }
              Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
              ShowDialog();
              return(TRUE);
            }
            /* SVS $ */

            case KEY_CTRLY:
            {
              for (I=FocusPos;I<ItemCount;I++)
                if (Item[I].Flags & DIF_EDITOR)
                {
                  if (I>FocusPos)
                  {
                    ((DlgEdit *)(Item[I].ObjPtr))->GetString(Str,sizeof(Str));
                    ((DlgEdit *)(Item[I-1].ObjPtr))->SetString(Str);
                  }
                  ((DlgEdit *)(Item[I].ObjPtr))->SetString("");
                }
                else
                  break;

              Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
              ShowDialog();
              return(TRUE);
            }

            case KEY_NUMDEL:
            case KEY_DEL:
            {
              /* $ 19.07.2000 SVS
                 ! "...� ��������� ������ ���� ������� home shift+end del
                   ���� �� ���������..."
                   DEL � ������, ������� DIF_EDITOR, ������� ��� �����
                   ���������...
              */
              if (FocusPos<ItemCount+1 && (Item[FocusPos+1].Flags & DIF_EDITOR))
              {
                int CurPos=edt->GetCurPos();
                int Length=edt->GetLength();
                int SelStart, SelEnd;

                edt->GetSelection(SelStart, SelEnd);
                edt->GetString(Str,sizeof(Str));
                int LengthStr=(int)strlen(Str);
                if(SelStart > -1)
                {
                  memmove(&Str[SelStart],&Str[SelEnd],Length-SelEnd+1);
                  edt->SetString(Str);
                  edt->SetCurPos(SelStart);
                  /* $ 28.07.2000 SVS
                    ��� ��������� ��������� ������� �������� �������� ���������
                    ����������� ������� SendDlgMessage - � ��� �������� ���!
                  */
                  Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
                  /* SVS $ */
                  ShowDialog();
                  return(TRUE);
                }
                else if (CurPos>=Length)
                {
                  DlgEdit *edt_1=(DlgEdit *)Item[FocusPos+1].ObjPtr;
                  /* $ 12.09.2000 SVS
                     ������ ��������, ���� Del ������ � �������
                     �������, ��� ����� ������
                  */
                  if (CurPos > Length)
                  {
                    LengthStr=CurPos;
                    memset(Str+Length,' ',CurPos-Length);
                  }
                  /* SVS $*/
                  edt_1->GetString(Str+LengthStr,sizeof(Str)-LengthStr);
                  edt_1->SetString(Str);
                  ProcessKey(KEY_CTRLY);
                  edt->SetCurPos(CurPos);
                  ShowDialog();
                  return(TRUE);
                }
              }
              break;
            }

            case KEY_PGDN:  case KEY_NUMPAD3:
            case KEY_PGUP:  case KEY_NUMPAD9:
            {
              I=FocusPos;
              while (Item[I].Flags & DIF_EDITOR)
                I=ChangeFocus(I,(Key == KEY_PGUP || Key == KEY_NUMPAD9)?-1:1,FALSE);
              if(!(Item[I].Flags & DIF_EDITOR))
                I=ChangeFocus(I,(Key == KEY_PGUP || Key == KEY_NUMPAD9)?1:-1,FALSE);

              int oldFocus=FocusPos;
              ChangeFocus2(FocusPos,I);
              if(oldFocus != I)
              {
                ShowDialog(oldFocus);
                ShowDialog(FocusPos); // ?? I ??
              }
              return(TRUE);
            }
          }
        }

        /* $ 24.09.2000 SVS
           ����� ������� Xlat
        */
        if(Key == KEY_OP_XLAT && !(Item[FocusPos].Flags & DIF_READONLY))
        {
          edt->SetClearFlag(0);
          edt->Xlat();
          Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
          Redraw(); // ����������� ������ ���� ����� DN_EDITCHANGE (imho)
          return TRUE;
        }
        /* SVS $ */
        /* SVS $ */
        if(!(Item[FocusPos].Flags & DIF_READONLY) ||
            (Item[FocusPos].Flags & DIF_READONLY) && IsNavKey(Key))
        {
          // "������ ��� ���������� � �������� ��������� � ����"?
          if((Opt.Dialogs.EditLine&DLGEDITLINE_NEWSELONGOTFOCUS) && Item[FocusPos].SelStart != -1 && PrevFocusPos != FocusPos)// && Item[FocusPos].SelEnd)
          {
            edt->Flags().Clear(FEDITLINE_MARKINGBLOCK);
            PrevFocusPos=FocusPos;
          }

          if (edt->ProcessKey(Key))
          {
            if(Item[FocusPos].Flags & DIF_READONLY)
              return TRUE;

            //int RedrawNeed=FALSE;
            /* $ 26.07.2000 SVS
               AutoComplite: ���� ���������� DIF_HISTORY
                   � ��������� ��������������!.
            */
            /* $ 04.12.2000 SVS
              �������������� - ����� �� �������� �� ����� ������������ ��������.
              GetCurRecord() ������ 0 ��� ������, ���� ��� �� ������ �� ���������.
            */
            if(!(Item[FocusPos].Flags & DIF_NOAUTOCOMPLETE))
            if(CtrlObject->Macro.GetCurRecord(NULL,NULL) == MACROMODE_NOMACRO &&
               ((Item[FocusPos].Flags & DIF_HISTORY) || Item[FocusPos].Type == DI_COMBOBOX))
            if((Opt.Dialogs.AutoComplete && Key && Key < 256 && Key != KEY_BS && !(Key == KEY_DEL||Key == KEY_NUMDEL)) ||
               (!Opt.Dialogs.AutoComplete && (Key == KEY_CTRLEND || Key == KEY_CTRLNUMPAD1))
              )
            {
              // (Opt.Dialogs.EditLine&DLGEDITLINE_AUTOCOMPLETECTRLEND)
              /* $ 05.12.2000 IS
                 ��� ������ � ������� ������ ;)
              */
              int MaxLen=sizeof(Item[FocusPos].Data);
              char *PStr=Str;
              if(Item[FocusPos].Flags & DIF_VAREDIT)
              {
                MaxLen=Item[FocusPos].Ptr.PtrLength;
                if((PStr=(char*)xf_malloc(MaxLen+1)) == NULL)
                  return TRUE; //???
              }
              int DoAutoComplete=TRUE;
              int CurPos=edt->GetCurPos();
              edt->GetString(PStr,MaxLen);
              int len=(int)strlen(PStr);
              edt->GetSelection(SelStart,SelEnd);
              if(SelStart < 0 || SelStart==SelEnd)
                  SelStart=len;
              else
                  SelStart++;

              if(CurPos<SelStart) DoAutoComplete=FALSE;
              if(SelStart<SelEnd && SelEnd<len) DoAutoComplete=FALSE;

              if(Opt.Dialogs.EditBlock)
              {
                if(DoAutoComplete && CurPos <= SelEnd)
                {
                  PStr[CurPos]=0;
                  edt->Select(CurPos,edt->GetLength()); //select the appropriate text
                  edt->DeleteBlock();
                  edt->FastShow();
                }
              }
              /* IS $ */

              SelEnd=(int)strlen(PStr);

              //find the string in the list
              /* $ 03.12.2000 IS
                   ��������� ���� DoAutoComplete
              */
              if (DoAutoComplete &&
                  FindInEditForAC(Item[FocusPos].Type == DI_COMBOBOX,(void *)Item[FocusPos].History,PStr,MaxLen))
              /* IS $ */
              {
  //_D(SysLog("Coplete: Str=%s SelStart=%d SelEnd=%d CurPos=%d",Str,SelStart,SelEnd, CurPos));
                edt->SetString(PStr);
                edt->Select(SelEnd,edt->GetLength()); //select the appropriate text
                //edt->Select(CurPos,sizeof(Str)); //select the appropriate text
                /* $ 01.08.2000 SVS
                   ��������� ������ � AutoComplete
                */
                edt->SetCurPos(CurPos); // SelEnd
                //RedrawNeed=TRUE;
              }
              if(Item[FocusPos].Flags & DIF_VAREDIT)
                xf_free(PStr);
            }
            /* SVS 03.12.2000 $ */
            if(!IsNavKey(Key)) //???????????????????????????????????????????
              Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
            /* SVS $ */
  //          if(RedrawNeed)
            Redraw(); // ����������� ������ ���� ����� DN_EDITCHANGE (imho)
            return(TRUE);
          }
        }
        else if(!(Key&(KEY_ALT|KEY_RALT)))
          return TRUE;
        /* SVS 21.08.2000 $ */
      }

      if (ProcessHighlighting(Key,FocusPos,FALSE))
        return(TRUE);

      return(ProcessHighlighting(Key,FocusPos,TRUE));
    }
  }
}


//////////////////////////////////////////////////////////////////////////
/* Public, Virtual:
   ��������� ������ �� "����".
   ����������� BaseInput::ProcessMouse.
*/
// $ 18.08.2000 SVS + DN_MOUSECLICK
int Dialog::ProcessMouse(MOUSE_EVENT_RECORD *MouseEvent)
{
  CriticalSectionLock Lock(CS);

  int I;
  int MsX,MsY;
  int Type;
  RECT Rect;

  // $ 11.06.2001 KM - ������� ���������� ������ ���� � DI_LISTBOX.
  if(!DialogMode.Check(DMODE_SHOW))
    return FALSE;

  if(DialogMode.Check(DMODE_MOUSEEVENT))
  {
    if(!DlgProc((HANDLE)this,DN_MOUSEEVENT,0,(LONG_PTR)MouseEvent))
      return TRUE;
  }

  MsX=MouseEvent->dwMousePosition.X;
  MsY=MouseEvent->dwMousePosition.Y;

  //for (I=0;I<ItemCount;I++)
  for (I=ItemCount-1;I>=0;I--)
  {
    if(Item[I].Flags&(DIF_DISABLE|DIF_HIDDEN))
      continue;
    Type=Item[I].Type;
    if (Type == DI_LISTBOX &&
        MsY >= Y1+Item[I].Y1 && MsY <= Y1+Item[I].Y2 &&
        MsX >= X1+Item[I].X1 && MsX <= X1+Item[I].X2)
    {
      VMenu *List=Item[I].ListPtr;
      int Pos=List->GetSelectPos();
      int CheckedListItem=List->GetSelection(-1);
      if((MouseEvent->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED))
      {
        if(FocusPos != I)
        {
          ChangeFocus2(FocusPos,I);
          ShowDialog();
        }
        if(MouseEvent->dwEventFlags!=DOUBLE_CLICK && (Item[I].IFlags.Flags&(DLGIIF_LISTREACTIONFOCUS|DLGIIF_LISTREACTIONNOFOCUS)) == 0)
        {
          List->ProcessMouse(MouseEvent);
          int NewListPos=List->GetSelectPos();

          if(NewListPos != Pos && !SendDlgMessage((HANDLE)this,DN_LISTCHANGE,I,(long)NewListPos))
          {
            List->SetSelection(CheckedListItem,Pos);
            if(DialogMode.Check(DMODE_SHOW) && !(Item[I].Flags&DIF_HIDDEN))
              ShowDialog(I); // FocusPos
          }
          else
          {
            Pos=NewListPos;
          }
        }
        else if (!SendDlgMessage((HANDLE)this,DN_MOUSECLICK,I,(LONG_PTR)MouseEvent))
        {
#if 1
          List->ProcessMouse(MouseEvent);
          int NewListPos=List->GetSelectPos();
          int InScroolBar=(MsX==X1+Item[I].X2 && MsY >= Y1+Item[I].Y1 && MsY <= Y1+Item[I].Y2) &&
                          (List->CheckFlags(VMENU_LISTBOX|VMENU_ALWAYSSCROLLBAR) || Opt.ShowMenuScrollbar);
          if(!InScroolBar       &&                                                                 // ��� ���������� �
              NewListPos != Pos &&                                                                 // ������� ���������� �
              !SendDlgMessage((HANDLE)this,DN_LISTCHANGE,I,(LONG_PTR)NewListPos))                 // � ������ ������ � ����
          {
            List->SetSelection(CheckedListItem,Pos);
            if(DialogMode.Check(DMODE_SHOW) && !(Item[I].Flags&DIF_HIDDEN))
              ShowDialog(I); // FocusPos
          }
          else
          {
            Pos=NewListPos;
            if(!InScroolBar && !(Item[I].Flags&DIF_LISTNOCLOSE))
            {
              ExitCode=I;
              CloseDialog();
              return TRUE;
            }
          }
#else
          if (SendDlgMessage((HANDLE)this,DN_LISTCHANGE,I,(LONG_PTR)Pos))
          {
            if (MsX==X1+Item[I].X2 && MsY >= Y1+Item[I].Y1 && MsY <= Y1+Item[I].Y2)
              List->ProcessMouse(MouseEvent); // ����� ��������� �� ���� �� �������� (KM)
            else
              ProcessKey(KEY_ENTER);
          }
#endif
        }
        return TRUE;
      }
      else
      {
        if( !MouseEvent->dwButtonState || SendDlgMessage((HANDLE)this,DN_MOUSECLICK,I,(LONG_PTR)MouseEvent) )
        {
          if(I == FocusPos && (Item[I].IFlags.Flags&DLGIIF_LISTREACTIONFOCUS)
              ||
             I != FocusPos && (Item[I].IFlags.Flags&DLGIIF_LISTREACTIONNOFOCUS)
            )
          {
            List->ProcessMouse(MouseEvent);
            int NewListPos=List->GetSelectPos();
            if(NewListPos != Pos && !SendDlgMessage((HANDLE)this,DN_LISTCHANGE,I,(LONG_PTR)NewListPos))
            {
              List->SetSelection(CheckedListItem,Pos);
              if(DialogMode.Check(DMODE_SHOW) && !(Item[I].Flags&DIF_HIDDEN))
                ShowDialog(I); // FocusPos
            }
            else
              Pos=NewListPos;
          }
        }
      }
      return(TRUE);
    }
  }

  if ((MsX<X1 || MsY<Y1 || MsX>X2 || MsY>Y2) && MouseEventFlags != MOUSE_MOVED)
  {
    if(DialogMode.Check(DMODE_CLICKOUTSIDE) && !DlgProc((HANDLE)this,DN_MOUSECLICK,-1,(LONG_PTR)MouseEvent))
    {
      if(!DialogMode.Check(DMODE_SHOW))
        return TRUE;

//      if (!(MouseEvent->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) && PrevLButtonPressed && ScreenObject::CaptureMouseObject)
      if (!(MouseEvent->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) && PrevLButtonPressed && (Opt.Dialogs.MouseButton&DMOUSEBUTTON_LEFT))
        ProcessKey(KEY_ESC);
//      else if (!(MouseEvent->dwButtonState & RIGHTMOST_BUTTON_PRESSED) && PrevRButtonPressed && ScreenObject::CaptureMouseObject)
      else if (!(MouseEvent->dwButtonState & RIGHTMOST_BUTTON_PRESSED) && PrevRButtonPressed && (Opt.Dialogs.MouseButton&DMOUSEBUTTON_RIGHT))
        ProcessKey(KEY_ENTER);
    }

    if (MouseEvent->dwButtonState)
      DialogMode.Set(DMODE_CLICKOUTSIDE);
      //ScreenObject::SetCapture(this);

    return(TRUE);
  }

  if (MouseEvent->dwButtonState==0)
  {
    DialogMode.Clear(DMODE_CLICKOUTSIDE);
//    ScreenObject::SetCapture(NULL);
    return(FALSE);
  }

  if (MouseEvent->dwEventFlags==0 || MouseEvent->dwEventFlags==DOUBLE_CLICK)
  {
    // ������ ���� - ��� �� ����������� �����.
    //for (I=0; I < ItemCount;I++)
    for (I=ItemCount-1;I>=0;I--)
    {
      if(Item[I].Flags&(DIF_DISABLE|DIF_HIDDEN))
        continue;

      GetItemRect(I,Rect);
      Rect.left+=X1;  Rect.top+=Y1;
      Rect.right+=X1; Rect.bottom+=Y1;
//_D(SysLog("? %2d) Rect (%2d,%2d) (%2d,%2d) '%s'",I,Rect.left,Rect.top,Rect.right,Rect.bottom,Item[I].Data));

      if(MsX >= Rect.left && MsY >= Rect.top && MsX <= Rect.right && MsY <= Rect.bottom)
      {
        // ��� ���������� :-)
        if(Item[I].Type == DI_SINGLEBOX || Item[I].Type == DI_DOUBLEBOX)
        {
          // ���� �� �����, ��...
          if(((MsX == Rect.left || MsX == Rect.right) && MsY >= Rect.top && MsY <= Rect.bottom) || // vert
             ((MsY == Rect.top  || MsY == Rect.bottom) && MsX >= Rect.left && MsX <= Rect.right) )   // hor
          {
            if(DlgProc((HANDLE)this,DN_MOUSECLICK,I,(LONG_PTR)MouseEvent))
              return TRUE;
            if(!DialogMode.Check(DMODE_SHOW))
              return TRUE;
          }
          else
            continue;
        }

        if(Item[I].Type == DI_USERCONTROL)
        {
          // ��� user-���� ���������� ���������� ����
          MouseEvent->dwMousePosition.X-=(short)Rect.left;
          MouseEvent->dwMousePosition.Y-=(short)Rect.top;
        }

//_SVS(SysLog("+ %2d) Rect (%2d,%2d) (%2d,%2d) '%s' Dbl=%d",I,Rect.left,Rect.top,Rect.right,Rect.bottom,Item[I].Data,MouseEvent->dwEventFlags==DOUBLE_CLICK));
        if(DlgProc((HANDLE)this,DN_MOUSECLICK,I,(LONG_PTR)MouseEvent))
          return TRUE;

        if(!DialogMode.Check(DMODE_SHOW))
           return TRUE;

        if(Item[I].Type == DI_USERCONTROL)
        {
           ChangeFocus2(FocusPos,I);
           ShowDialog();
           return(TRUE);
        }
        break;
      }
    }

    if((MouseEvent->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED))
    {
      //for (I=0;I<ItemCount;I++)
      int OldFocusPos=FocusPos;
      for (I=ItemCount-1;I>=0;I--)
      {
        /* $ 04.12.2000 SVS
           ��������� �� ������ ����������� � ���� ����������� ��������
        */
        if(Item[I].Flags&(DIF_DISABLE|DIF_HIDDEN))
          continue;
        /* SVS $ */
        Type=Item[I].Type;
        if (MsX>=X1+Item[I].X1)
        {
          /* ********************************************************** */
          if (IsEdit(Type))
          {
            /* $ 15.08.2000 SVS
               + ������� ���, ����� ����� ������ � DropDownList
                 ������ ����������� ���.
               ���� ��������� ���������� - ����� ������ ������� � ��
               ����� ������������ �� ������ �������, �� ������ �����������
               �� �������� ��������� �� ��������� ������� ������� �� ����������
            */
            int EditX1,EditY1,EditX2,EditY2;
            DlgEdit *EditLine=(DlgEdit *)(Item[I].ObjPtr);
            EditLine->GetPosition(EditX1,EditY1,EditX2,EditY2);

            if(MsY==EditY1 && Type == DI_COMBOBOX &&
               (Item[I].Flags & DIF_DROPDOWNLIST) &&
               MsX >= EditX1 && MsX <= EditX2+1)
            {
              EditLine->SetClearFlag(0);
              if(!(Item[I].Flags&DIF_NOFOCUS))
                ChangeFocus2(FocusPos,I);
              else
              {
                Item[FocusPos].Focus=0; //??
                FocusPos=I;
              }
              ShowDialog();
              ProcessOpenComboBox(Item[FocusPos].Type,Item+FocusPos,FocusPos);
              //ProcessKey(KEY_CTRLDOWN);
              if(Item[I].Flags&DIF_NOFOCUS) //???
                FocusPos=OldFocusPos;       //???
              return(TRUE);
            }

            if (EditLine->ProcessMouse(MouseEvent))
            {
              EditLine->SetClearFlag(0); // � ����� ��� ������ � ����� edit?
              if(!(Item[I].Flags&DIF_NOFOCUS)) //??? !!!
                ChangeFocus2(FocusPos,I);      //??? !!!
              else
              {
                Item[FocusPos].Focus=0; //??
                FocusPos=I;
              }
              /* $ 23.06.2001 KM
                 ! ��������� ����� �������������� ���� ������ �����
                   �� �������� ������� ���������� � ���������� � ������� ������.
              */
              ShowDialog(); // ����� �� ������ ���� ������� ��� ���� ������?
              /* KM $ */
              return(TRUE);
            }
            else
            {
              // �������� �� DI_COMBOBOX ����� ������. ������ (KM).
              if (MsX==EditX2+1 && MsY==EditY1 &&
                  (Item[I].History ||
                    (Type == DI_COMBOBOX && Item[I].ListPtr && Item[I].ListPtr->GetItemCount())
                  ) &&
                  ((Item[I].Flags & DIF_HISTORY) && Opt.Dialogs.EditHistory
                   || Type == DI_COMBOBOX))
//                  ((Item[I].Flags & DIF_HISTORY) && Opt.Dialogs.EditHistory))
              {
                EditLine->SetClearFlag(0); // ��� �� ���������� ��, �� �...
                if(!(Item[I].Flags&DIF_NOFOCUS))
                  ChangeFocus2(FocusPos,I);
                else
                {
                  Item[FocusPos].Focus=0; //??
                  FocusPos=I;
                }
                if(!(Item[I].Flags&DIF_HIDDEN))
                  ShowDialog(I);
                ProcessOpenComboBox(Item[FocusPos].Type,Item+FocusPos,FocusPos);
                //ProcessKey(KEY_CTRLDOWN);
                if(Item[I].Flags&DIF_NOFOCUS) //???
                   FocusPos=OldFocusPos;      //???
                return(TRUE);
              }
            }
          }

          /* ********************************************************** */
          if (Type==DI_BUTTON &&
              MsY==Y1+Item[I].Y1 &&
              MsX < X1+Item[I].X1+HiStrlen(Item[I].Data))
          {
            if(!(Item[I].Flags&DIF_NOFOCUS))
            {
              ChangeFocus2(FocusPos,I);
              ShowDialog();
            }
            else
            {
              Item[FocusPos].Focus=0;
              FocusPos=I;
            }
            while (IsMouseButtonPressed())
              ;
            if (MouseX <  X1 ||
                MouseX >  X1+Item[I].X1+HiStrlen(Item[I].Data)+4 ||
                MouseY != Y1+Item[I].Y1)
            {
              if(!(Item[I].Flags&DIF_NOFOCUS))
              {
                ChangeFocus2(FocusPos,I);
                ShowDialog();
              }
              return(TRUE);
            }
            ProcessKey(KEY_ENTER);
            return(TRUE);
          }

          /* ********************************************************** */
          if ((Type == DI_CHECKBOX ||
               Type == DI_RADIOBUTTON) &&
              MsY==Y1+Item[I].Y1 &&
              MsX < (X1+Item[I].X1+HiStrlen(Item[I].Data)+4-((Item[I].Flags & DIF_MOVESELECT)!=0)))
          {
            if(!(Item[I].Flags&DIF_NOFOCUS))
              ChangeFocus2(FocusPos,I);
            else
            {
              Item[FocusPos].Focus=0; //??
              FocusPos=I;
            }
            ProcessKey(KEY_SPACE);
//            if(Item[I].Flags&DIF_NOFOCUS)
//              FocusPos=OldFocusPos;
            return(TRUE);
          }
        }
      } // for (I=0;I<ItemCount;I++)
      // ��� MOUSE-�����������:
      //   ���� �������� � ��� ������, ���� ���� �� ������ �� �������� ��������
      //
      if (DialogMode.Check(DMODE_ISCANMOVE)) // �������, ���� ���������! (IsCanMove)
      {
        /* $ 03.08.2000 tran
           �� ��� �������� - �� ����� ���������� */
        //DialogMode.Set(DMODE_DRAGGED);
        OldX1=X1; OldX2=X2; OldY1=Y1; OldY2=Y2;
        // �������� delta ����� �������� � Left-Top ����������� ����
        MsX=abs(X1-MouseX);
        MsY=abs(Y1-MouseY);

        int NeedSendMsg=0;

        while (1)
        {
          int mb=IsMouseButtonPressed();
          /* $ 15.12.2000 SVS
             ����� ������ �������� �����������
          */
          int mx,my,X0,Y0;
          if ( mb==1 ) // left key, still dragging
          {
            int AdjX=0,AdjY=0;
            int OX1=X1;
            int OY1=Y1;
            int NX1=X0=X1;
            int NX2=X2;
            int NY1=Y0=Y1;
            int NY2=Y2;

            if(MouseX==PrevMouseX)
              mx=X1;
            else
              mx=MouseX-MsX;
            if(MouseY==PrevMouseY)
              my=Y1;
            else
              my=MouseY-MsY;

            NX2=mx+(X2-X1);
            NX1=mx;
            AdjX=NX1-X0;

            NY2=my+(Y2-Y1);
            NY1=my;
            AdjY=NY1-Y0;

            // "� ��� �� �������?" (��� �������� ���)
            if(OX1 != NX1 || OY1 != NY1)
            {
              if(!NeedSendMsg) // ����, � ��� ������� ������ � ���������� ���������?
              {
                NeedSendMsg++;
                if(!DlgProc((HANDLE)this,DN_DRAGGED,0,0)) // � ����� ��� ��������?
                  break;  // ����� ������...������ ������ - � ���� �����������
                if(!DialogMode.Check(DMODE_SHOW))
                  break;
              }

              // ��, ������� ���. ������...
              {
                LockScreen LckScr;
                Hide();
                X1=NX1; X2=NX2; Y1=NY1; Y2=NY2;
                if(AdjX || AdjY)
                  AdjustEditPos(AdjX,AdjY); //?
                Show();
              }
            }
          }
          else if (mb==2) // right key, abort
          {
            LockScreen LckScr;
            Hide();
            AdjustEditPos(OldX1-X1,OldY1-Y1);
            X1=OldX1;
            X2=OldX2;
            Y1=OldY1;
            Y2=OldY2;
            DialogMode.Clear(DMODE_DRAGGED);
            DlgProc((HANDLE)this,DN_DRAGGED,1,TRUE);
            if(DialogMode.Check(DMODE_SHOW))
              Show();
            break;
          }
          else  // release key, drop dialog
          {
            if(OldX1!=X1 || OldX2!=X2 || OldY1!=Y1 || OldY2!=Y2)
            {
              LockScreen LckScr;
              DialogMode.Clear(DMODE_DRAGGED);
              DlgProc((HANDLE)this,DN_DRAGGED,1,0);
              if(DialogMode.Check(DMODE_SHOW))
                Show();
            }
            break;
          }

        }// while (1)
      }
    }
  }
  return(FALSE);
}


int Dialog::ProcessOpenComboBox(int Type,struct DialogItem *CurItem, int CurFocusPos)
{
  CriticalSectionLock Lock(CS);

  char Str[1024];
  DlgEdit *CurEditLine;

  // ��� user-���� ����������
  if(Type == DI_USERCONTROL)
    return TRUE;

  CurEditLine=((DlgEdit *)(CurItem->ObjPtr));
  if (IsEdit(Type) &&
       (CurItem->Flags & DIF_HISTORY) &&
       Opt.Dialogs.EditHistory &&
       CurItem->History &&
       !(CurItem->Flags & DIF_READONLY))
  {
    // $ 26.07.2000 SVS - �������� ��, ��� � ������ ����� � ������� ������ �� ������� ��� ��������� ������� ������ � �������.
    char *PStr=Str;
    int MaxLen=sizeof(CurItem->Data);
    if(CurItem->Flags&DIF_VAREDIT)
    {
      MaxLen=CurItem->Ptr.PtrLength;
      if((PStr=(char*)xf_malloc(MaxLen+1)) == NULL)
        return TRUE;//???
    }
    CurEditLine->GetString(PStr,MaxLen);
    SelectFromEditHistory(CurItem,CurEditLine,CurItem->History,PStr,MaxLen);
    if(CurItem->Flags&DIF_VAREDIT)
      xf_free(PStr);
  }
  else if(Type == DI_COMBOBOX && CurItem->ListPtr &&
          !(CurItem->Flags & DIF_READONLY) &&
          CurItem->ListPtr->GetItemCount() > 0) //??
  {
    int MaxLen=(CurItem->Flags&DIF_VAREDIT)?
                 CurItem->Ptr.PtrLength:
                 sizeof(CurItem->Data);
    if(SelectFromComboBox(CurItem,CurEditLine,CurItem->ListPtr,MaxLen) != KEY_ESC)
      Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,CurFocusPos,0);
  }
  return(TRUE);
}

int Dialog::ProcessRadioButton(int CurRB)
{
  CriticalSectionLock Lock(CS);

  int PrevRB=CurRB, I, J;

  for (I=CurRB;;I--)
  {
    if(I==0)
      break;

    if (Item[I].Type==DI_RADIOBUTTON && (Item[I].Flags & DIF_GROUP))
      break;

    if(Item[I-1].Type!=DI_RADIOBUTTON)
      break;
  }

  do
  {
    /* $ 28.07.2000 SVS
      ��� ��������� ��������� ������� �������� �������� ���������
      ����������� ������� SendDlgMessage - � ��� �������� ���!
    */
    J=Item[I].Selected;
    Item[I].Selected=0;
    if(J)
    {
      PrevRB=I;
    }
    ++I;
    /* SVS $ */
  } while (I<ItemCount && Item[I].Type==DI_RADIOBUTTON &&
           (Item[I].Flags & DIF_GROUP)==0);

  Item[CurRB].Selected=1;
  /* $ 28.07.2000 SVS
    ��� ��������� ��������� ������� �������� �������� ���������
    ����������� ������� SendDlgMessage - � ��� �������� ���!
  */
  if(!Dialog::SendDlgMessage((HANDLE)this,DN_BTNCLICK,PrevRB,0) ||
     !Dialog::SendDlgMessage((HANDLE)this,DN_BTNCLICK,CurRB,1))
  {
     // ������ �����, ���� ������������ �� �������...
     Item[CurRB].Selected=0;
     Item[PrevRB].Selected=1;
     return PrevRB;
  }
  /* SVS $ */
  return CurRB;
}


int Dialog::Do_ProcessFirstCtrl()
{
  CriticalSectionLock Lock(CS);

  if (IsEdit(Item[FocusPos].Type))
  {
    ((DlgEdit *)(Item[FocusPos].ObjPtr))->ProcessKey(KEY_HOME);
    return(TRUE);
  }
  else
  {
    int I;
    for (I=0;I<ItemCount;I++)
      if (IsFocused(Item[I].Type))
      {
        int OldPos=FocusPos;
        ChangeFocus2(FocusPos,I);
        if(OldPos!=FocusPos)
        {
          ShowDialog(OldPos);
          ShowDialog(FocusPos);
        }
        break;
      }
  }
  return(TRUE);
}

int Dialog::Do_ProcessNextCtrl(int Up,BOOL IsRedraw)
{
  CriticalSectionLock Lock(CS);

  int OldPos=FocusPos;
  int PrevPos=0;

  if (IsEdit(Item[FocusPos].Type) && (Item[FocusPos].Flags & DIF_EDITOR))
    PrevPos=((DlgEdit *)(Item[FocusPos].ObjPtr))->GetCurPos();

  int I=ChangeFocus(FocusPos,Up? -1:1,FALSE);
  Item[FocusPos].Focus=0;
  Item[I].Focus=1;
  ChangeFocus2(FocusPos,I);

  if (IsEdit(Item[I].Type) && (Item[I].Flags & DIF_EDITOR))
    ((DlgEdit *)(Item[I].ObjPtr))->SetCurPos(PrevPos);

  if (Item[FocusPos].Type == DI_RADIOBUTTON && (Item[I].Flags & DIF_MOVESELECT))
    ProcessKey(KEY_SPACE);
  else if(IsRedraw)
  {
    ShowDialog(OldPos);
    ShowDialog(FocusPos);
  }

  return(TRUE);
}

int Dialog::Do_ProcessTab(int Next)
{
  CriticalSectionLock Lock(CS);

  int I;
  if(ItemCount > 1)
  {
    // ����� � ������� ������� �������!!!
    if (Item[FocusPos].Flags & DIF_EDITOR)
    {
      I=FocusPos;
      while (Item[I].Flags & DIF_EDITOR)
        I=ChangeFocus(I,Next ? 1:-1,TRUE);
    }
    else
    {
      I=ChangeFocus(FocusPos,Next ? 1:-1,TRUE);
      if (!Next)
        while (I>0 && (Item[I].Flags & DIF_EDITOR)!=0 &&
               (Item[I-1].Flags & DIF_EDITOR)!=0 &&
               ((DlgEdit *)Item[I].ObjPtr)->GetLength()==0)
          I--;
    }
  }
  else
    I=FocusPos;

  int oldFocus=FocusPos;
  ChangeFocus2(FocusPos,I);
  if(oldFocus != I)
  {
    ShowDialog(oldFocus);
    ShowDialog(FocusPos); // ?? I ??
  }
  return(TRUE);
}


int Dialog::Do_ProcessSpace()
{
  CriticalSectionLock Lock(CS);

  int OldFocusPos;
  if (Item[FocusPos].Type==DI_CHECKBOX)
  {
    /* $ 04.12.2000 SVS
       3-� ��������� ���������
    */
    int OldSelected=Item[FocusPos].Selected;

    if(Item[FocusPos].Flags&DIF_3STATE)
      (++Item[FocusPos].Selected)%=3;
    else
      Item[FocusPos].Selected = !Item[FocusPos].Selected;
    /* $ 28.07.2000 SVS
      ��� ��������� ��������� ������� �������� �������� ���������
       ����������� ������� SendDlgMessage - � ��� �������� ���!
    */
    OldFocusPos=FocusPos;
    if(!Dialog::SendDlgMessage((HANDLE)this,DN_BTNCLICK,FocusPos,Item[FocusPos].Selected))
      Item[OldFocusPos].Selected = OldSelected;
    /* SVS $ */
    /* SVS 04.12.2000 $ */
    ShowDialog();
    return(TRUE);
  }
  else if (Item[FocusPos].Type==DI_RADIOBUTTON)
  {
    FocusPos=ProcessRadioButton(FocusPos);
    ShowDialog();
    return(TRUE);
  }
  else if (IsEdit(Item[FocusPos].Type) && !(Item[FocusPos].Flags & DIF_READONLY))
  {
    /* $ 28.07.2000 SVS
      ��� ��������� ��������� ������� �������� �������� ���������
      ����������� ������� SendDlgMessage - � ��� �������� ���!
    */
    if(((DlgEdit *)(Item[FocusPos].ObjPtr))->ProcessKey(KEY_SPACE))
    {
      Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,FocusPos,0);
      Redraw(); // ����������� ������ ���� ����� DN_EDITCHANGE (imho)
    }
    return(TRUE);
  }
  return(TRUE);
}


//////////////////////////////////////////////////////////////////////////
/* Private:
   �������� ����� ����� (����������� ���������
     KEY_TAB, KEY_SHIFTTAB, KEY_UP, KEY_DOWN,
   � ��� �� Alt-HotKey)
*/
/* $ 28.07.2000 SVS
   ������� ��� ��������� DN_KILLFOCUS & DN_SETFOCUS
*/
/* $ 24.08.2000 SVS
   ������� ��� DI_USERCONTROL
*/
int Dialog::ChangeFocus(int CurFocusPos,int Step,int SkipGroup)
{
  CriticalSectionLock Lock(CS);

  int Type,OrigFocusPos=CurFocusPos;
//  int FucusPosNeed=-1;
  // � ������� ��������� ������� ����� �������� ���������,
  //   ��� ������� - LostFocus() - ������ ����� �����.
//  if(DialogMode.Check(DMODE_INITOBJECTS))
//    FucusPosNeed=DlgProc((HANDLE)this,DN_KILLFOCUS,FocusPos,0);
//  if(FucusPosNeed != -1 && IsFocused(Item[FucusPosNeed].Type))
//    FocusPos=FucusPosNeed;
//  else
  {
    while (1)
    {
      CurFocusPos+=Step;
      if (CurFocusPos>=ItemCount)
        CurFocusPos=0;
      if (CurFocusPos<0)
        CurFocusPos=ItemCount-1;

      Type=Item[CurFocusPos].Type;

      if(!(Item[CurFocusPos].Flags&(DIF_NOFOCUS|DIF_DISABLE|DIF_HIDDEN)))
      {
        if (Type==DI_LISTBOX || Type==DI_BUTTON || Type==DI_CHECKBOX || IsEdit(Type) || Type==DI_USERCONTROL)
          break;
        if (Type==DI_RADIOBUTTON && (!SkipGroup || Item[CurFocusPos].Selected))
          break;
      }
      // ������� ������������ � ����������� ����������� :-)
      if(OrigFocusPos == CurFocusPos)
        break;
    }
  }

//  Dialog::FocusPos=FocusPos;
  // � ������� ��������� ������� ����� �������� ���������,
  //   ��� ������� GotFocus() - ������� ����� �����.
  // ���������� ������������ �������� ������� ��������
//  if(DialogMode.Check(DMODE_INITOBJECTS))
//    DlgProc((HANDLE)this,DN_GOTFOCUS,FocusPos,0);
  return(CurFocusPos);
}
/* SVS $ */


//////////////////////////////////////////////////////////////////////////
/* $ 28.07.2000 SVS
   Private:
   �������� ����� ����� ����� ����� ����������.
   ������� �������� � ���, ����� ���������� DN_KILLFOCUS & DM_SETFOCUS
*/
int Dialog::ChangeFocus2(int KillFocusPos,int SetFocusPos)
{
  CriticalSectionLock Lock(CS);

  int FucusPosNeed=-1;
  if(!(Item[SetFocusPos].Flags&(DIF_NOFOCUS|DIF_DISABLE|DIF_HIDDEN)))
  {
    if(DialogMode.Check(DMODE_INITOBJECTS))
    {
      FucusPosNeed=(int)DlgProc((HANDLE)this,DN_KILLFOCUS,KillFocusPos,0);
      if(!DialogMode.Check(DMODE_SHOW))
         return SetFocusPos;
    }

    if(FucusPosNeed != -1 && IsFocused(Item[FucusPosNeed].Type))
      SetFocusPos=FucusPosNeed;

    if(Item[SetFocusPos].Flags&DIF_NOFOCUS)
       SetFocusPos=KillFocusPos;

    Item[KillFocusPos].Focus=0;

    // "������� ��������� ��� ������ ������?"
    if(IsEdit(Item[KillFocusPos].Type) &&
       !(Item[KillFocusPos].Type == DI_COMBOBOX && (Item[KillFocusPos].Flags & DIF_DROPDOWNLIST)))
    {
      DlgEdit *EditPtr=(DlgEdit*)Item[KillFocusPos].ObjPtr;
      EditPtr->GetSelection(Item[KillFocusPos].SelStart,Item[KillFocusPos].SelEnd);
      if((Opt.Dialogs.EditLine&DLGEDITLINE_CLEARSELONKILLFOCUS))
      {
        EditPtr->Select(-1,0);
      }
    }

    Item[SetFocusPos].Focus=1;

    // "�� ��������������� ��������� ��� ��������� ������?"
    if(IsEdit(Item[SetFocusPos].Type) &&
       !(Item[SetFocusPos].Type == DI_COMBOBOX && (Item[SetFocusPos].Flags & DIF_DROPDOWNLIST)))
    {
      DlgEdit *EditPtr=(DlgEdit*)Item[SetFocusPos].ObjPtr;
      if(!(Opt.Dialogs.EditLine&DLGEDITLINE_NOTSELONGOTFOCUS))
      {
        if(Opt.Dialogs.EditLine&DLGEDITLINE_SELALLGOTFOCUS)
          EditPtr->Select(0,EditPtr->GetStrSize());
        else
          EditPtr->Select(Item[SetFocusPos].SelStart,Item[SetFocusPos].SelEnd);
      }
      else
      {
        EditPtr->Select(-1,0);
      }

      // ��� ��������� ������ ����� ����������� ������ � ����� ������?
      if(Opt.Dialogs.EditLine&DLGEDITLINE_GOTOEOLGOTFOCUS)
      {
        EditPtr->SetCurPos(EditPtr->GetStrSize());
      }
    }

    /* $ 21.02.2002 DJ
       �������������� ��������, ���� �� � ���� �����
    */
    if (Item[KillFocusPos].Type == DI_LISTBOX)
      Item[KillFocusPos].ListPtr->ClearFlags (VMENU_LISTHASFOCUS);
    if (Item[SetFocusPos].Type == DI_LISTBOX)
      Item[SetFocusPos].ListPtr->SetFlags (VMENU_LISTHASFOCUS);
    /* DJ */

    Dialog::PrevFocusPos=Dialog::FocusPos;
    Dialog::FocusPos=SetFocusPos;
    if(DialogMode.Check(DMODE_INITOBJECTS))
      DlgProc((HANDLE)this,DN_GOTFOCUS,SetFocusPos,0);
  }
  else
    SetFocusPos=KillFocusPos;

  SelectOnEntry(KillFocusPos,FALSE);
  SelectOnEntry(SetFocusPos,TRUE);
  return(SetFocusPos);
}
/* SVS $ */

/* $ 08.09.2000 SVS
  ������� SelectOnEntry - ��������� ������ ��������������
  ��������� ����� DIF_SELECTONENTRY
*/
void Dialog::SelectOnEntry(int Pos,BOOL Selected)
{
//  if(!DialogMode.Check(DMODE_SHOW))
//     return;
  if(IsEdit(Item[Pos].Type) &&
     (Item[Pos].Flags&DIF_SELECTONENTRY)
//     && PrevFocusPos != -1 && PrevFocusPos != Pos
    )
  {
    DlgEdit *edt=(DlgEdit *)Item[Pos].ObjPtr;
    if(edt)
    {
      if(Selected)
        edt->Select(0,edt->GetLength());
      else
        edt->Select(-1,0);
      //_SVS(SysLog("Selected=%d edt->GetLength()=%d",Selected,edt->GetLength()));
    }
  }
}
/* SVS $ */

/* $ 04.12.2000 SVS
   ! ����������� ������� ConvertItem() � DataToItem() - � �����������
     ����� ���������� ���������� � ������� ��� (MSVC - ��� ��� ������ :-(
*/

//////////////////////////////////////////////////////////////////////////
/* $ 28.07.2000 SVS
   Public, Static:
   + ������� ConvertItem - �������� �������������� ��������� ������� ��
   ����������� ������������� �� �������
*/
void Dialog::ConvertItem(int FromPlugin,
                         struct FarDialogItem *Item,struct DialogItem *Data,
                         int Count,BOOL InternalCall)
{
  int I;
  if(!Item || !Data)
    return;

  char *PtrData;
  int PtrLength;
  DlgEdit *EditPtr;

  if(FromPlugin == CVTITEM_TOPLUGIN)
    for (I=0; I < Count; I++, ++Item, ++Data)
    {
      memcpy(Item,Data,sizeof(struct FarDialogItem));
      if(InternalCall)
      {
        if(Dialog::IsEdit(Data->Type) && (EditPtr=(DlgEdit *)(Data->ObjPtr)) != NULL)
        {
          // �������� ��������
          if((Data->Type==DI_EDIT || Data->Type==DI_COMBOBOX) &&
             (Data->Flags&DIF_VAREDIT))
          {
            PtrData  =(char *)Data->Ptr.PtrData;
            PtrLength=Data->Ptr.PtrLength;
          }
          else
          {
            PtrData  =Data->Data;
            PtrLength=sizeof(Data->Data);
          }
          EditPtr->GetString(PtrData,PtrLength);
        }
      }
      memmove(Item->Data.Data,Data->Data,sizeof(Item->Data.Data));
    }
  else
    for (I=0; I < Count; I++, ++Item, ++Data)
    {
      memcpy(Data,Item,sizeof(struct FarDialogItem));
      if(Data->X2 < Data->X1) Data->X2=Data->X1;
      if(Data->Y2 < Data->Y1) Data->Y2=Data->Y1;
      if((Data->Type == DI_COMBOBOX || Data->Type == DI_LISTBOX) && (DWORD_PTR)Item->Param.ListItems < 0x2000)
        Data->ListItems=NULL;
      memmove(Data->Data,Item->Data.Data,sizeof(Data->Data));
      /* ���� ����� ����� �������� ����� ���������� ��������.
      �� �������� ������ ������ � ����� �� DN_EDITCHANGE
      if(InternalCall)
      {
        if(Dialog::IsEdit(Data->Type) && (EditPtr=(DlgEdit *)(Data->ObjPtr)) != NULL)
        {
          // �������
          if((Data->Type==DI_EDIT || Data->Type==DI_COMBOBOX) &&
             (Data->Flags&DIF_VAREDIT))
          {
            PtrData  =(char *)Data->Ptr.PtrData;
            PtrLength=Data->Ptr.PtrLength;
          }
          else
          {
            PtrData  =Data->Data;
            PtrLength=sizeof(Data->Data);
          }
          EditPtr->SetString(PtrData);
        }
      }
      */
    }
}
/* SVS $ */

//////////////////////////////////////////////////////////////////////////
/* Public, Static:
   ����������� ������ �� ��������� ������� �� ����������
   �������������. ���������� ������� InitDialogItems (��. "Far PlugRinG
   Russian Help Encyclopedia of Developer")
*/
void Dialog::DataToItem(struct DialogData *Data,struct DialogItem *Item,int Count)
{
  int I;

  if(!Item || !Data)
    return;

  memset(Item,0,sizeof(struct DialogItem)*Count);
  for (I=0; I < Count; I++, ++Item, ++Data)
  {
    Item->ID=I;
    Item->Type=Data->Type;
    Item->X1=Data->X1;
    Item->Y1=Data->Y1;
    Item->X2=Data->X2;
    Item->Y2=Data->Y2;
    if(Item->X2 < Item->X1) Item->X2=Item->X1;
    if(Item->Y2 < Item->Y1) Item->Y2=Item->Y1;
    Item->Focus=Data->Focus;
    Item->History=Data->History;
    Item->Flags=Data->Flags;
    Item->DefaultButton=Data->DefaultButton;
    Item->SelStart=-1;
    if ((DWORD_PTR)Data->Data<MAX_MSG)
      xstrncpy(Item->Data,MSG((int)(DWORD_PTR)Data->Data),sizeof(Item->Data)-1);
    else
      memcpy(Item->Data,Data->Data,sizeof(Item->Data));
  }
}
/* SVS 04.12.2000 $ */

int Dialog::SetAutomation(WORD IDParent,WORD id,
                             DWORD UncheckedSet,DWORD UncheckedSkip,
                             DWORD CheckedSet,DWORD CheckedSkip,
                             DWORD Checked3Set,DWORD Checked3Skip)
{
  CriticalSectionLock Lock(CS);

  int Ret=FALSE;
  if(IDParent < ItemCount && (Item[IDParent].Flags&DIF_AUTOMATION) &&
     id < ItemCount && IDParent != id) // ���� ���� �� �����!
  {
    DialogItemAutomation *Auto;
    int AutoCount=Item[IDParent].AutoCount;
    if((Auto=(DialogItemAutomation*)xf_realloc(Item[IDParent].AutoPtr,sizeof(DialogItemAutomation)*(AutoCount+1))) != NULL)
    {
      Item[IDParent].AutoPtr=Auto;
      Auto=Item[IDParent].AutoPtr+AutoCount;
      Auto->ID=id;
      Auto->Flags[0][0]=UncheckedSet;
      Auto->Flags[0][1]=UncheckedSkip;
      Auto->Flags[1][0]=CheckedSet;
      Auto->Flags[1][1]=CheckedSkip;
      Auto->Flags[2][0]=Checked3Set;
      Auto->Flags[2][1]=Checked3Skip;
      Item[IDParent].AutoCount++;
      Ret=TRUE;
    }
  }
  return Ret;
}

//////////////////////////////////////////////////////////////////////////
/* Private:
   ��������� ��� �������� ������� �� ������� ������ �����
   (DI_EDIT, DI_FIXEDIT, DI_PSWEDIT) � � ������ ������ ���������� TRUE
*/
/* $ 18.07.2000 SVS
   ! ������� DI_COMBOBOX ��������� � ��������� ��������� ����������...
*/
int Dialog::IsEdit(int Type)
{
  return(Type == DI_EDIT ||
         Type == DI_FIXEDIT ||
         Type == DI_PSWEDIT ||
         Type == DI_MEMOEDIT ||
         Type == DI_COMBOBOX);
}
/* SVS $ */


//////////////////////////////////////////////////////////////////////////
/* $ 28.07.2000 SVS
   �������, ������������ - "����� �� ������� ������� ����� ����� �����"
*/
/* $ 24.08.2000 SVS
   ������� ��� DI_USERCONTROL
*/
int Dialog::IsFocused(int Type)
{
  return(Type==DI_EDIT ||
         Type==DI_FIXEDIT ||
         Type==DI_PSWEDIT ||
         Type==DI_COMBOBOX ||
         Type==DI_BUTTON ||
         Type==DI_CHECKBOX ||
         Type==DI_RADIOBUTTON ||
         Type==DI_LISTBOX ||
         Type==DI_MEMOEDIT ||
         Type==DI_USERCONTROL);
}
/* 24.08.2000 SVS $ */
/* SVS $ */


//////////////////////////////////////////////////////////////////////////
/* $ 26.07.2000 SVS
   AutoComplite: ����� ��������� ��������� � �������
*/
/* $ 28.07.2000 SVS
   ! �������� DlgEdit *EditLine ����� �������!
*/
//
int Dialog::FindInEditForAC(int TypeFind,void *HistoryName,char *FindStr,int MaxLen)
{
  CriticalSectionLock Lock(CS);

  char *Str;
  int I, LenFindStr=(int)strlen(FindStr);

  /* $ 26.07.2004 KM
     ������� �������� �� HistoryName==NULL, ������.
  */
  if (HistoryName==NULL)
    return FALSE;
  /* KM $ */

  if(!TypeFind)
  {
    char RegKey[NM];
    if((Str=(char*)xf_malloc(MaxLen+1)) == NULL)
      return FALSE;
    sprintf(RegKey,fmtSavedDialogHistory,(char*)HistoryName);
    // �������� ������� �������
    for (I=0; I < Opt.DialogsHistoryCount; I++)
    {
      itoa(I,PHisLine,10);
      GetRegKey(RegKey,HisLine,Str,"",MaxLen);
      if (!LocalStrnicmp(Str,FindStr,LenFindStr))
        break;
    }
    if (I == Opt.DialogsHistoryCount)
    {
      xf_free(Str);
      return FALSE;
    }
    /* $ 28.07.2000 SVS
       ��������� ������� �� �����������, � ��������� �����������.
    */
//_D(SysLog("FindInEditForAC()  FindStr=%s Str=%s",FindStr,&Str[strlen(FindStr)]));
    strncat(FindStr,&Str[LenFindStr],MaxLen-LenFindStr);
    /* SVS $ */
    xf_free(Str);
  }
  else
  {
    struct FarListItem *ListItems=((struct FarList *)HistoryName)->Items;
    int Count=((struct FarList *)HistoryName)->ItemsNumber;

    for (I=0; I < Count ;I++)
    {
      if (!LocalStrnicmp(ListItems[I].Text,FindStr,Min(LenFindStr,(int)sizeof(ListItems[I].Text))))
        break;
    }
    if (I  == Count)
      return FALSE;

    if(sizeof(ListItems[I].Text) < LenFindStr)
      strncat(FindStr,&ListItems[I].Text[LenFindStr],MaxLen-LenFindStr);
  }
  return TRUE;
}
/*  SVS $ */

//////////////////////////////////////////////////////////////////////////
/* Private:
   ��������� ���������� ������ ��� ComboBox
*/
/*
   $ 18.07.2000 SVS
   �������-���������� ������ �� ������ � ���������...
*/
int Dialog::SelectFromComboBox(
         struct DialogItem *CurItem,
         DlgEdit *EditLine,                   // ������ ��������������
         VMenu *ComboBox,    // ������ �����
         int MaxLen)
{
  CriticalSectionLock Lock(CS);

  char *Str;
  int EditX1,EditY1,EditX2,EditY2;
  int I,Dest, OriginalPos;
  int CurFocusPos=FocusPos;

  if((Str=(char*)xf_malloc(MaxLen)) != NULL)
  {
    EditLine->GetPosition(EditX1,EditY1,EditX2,EditY2);
    if (EditX2-EditX1<20)
      EditX2=EditX1+20;
    SetDropDownOpened(TRUE); // ��������� ���� "��������" ����������.
    SetComboBoxPos();
    // ����� ���������� ������� �� ��������� �������� ���������
    BYTE RealColors[VMENU_COLOR_COUNT];
    struct FarListColors ListColors={0};
    ListColors.ColorCount=VMENU_COLOR_COUNT;
    ListColors.Colors=RealColors;
    ComboBox->SetColors(NULL);
    ComboBox->GetColors(&ListColors);
    if(DlgProc((HANDLE)this,DN_CTLCOLORDLGLIST,CurItem->ID,(LONG_PTR)&ListColors))
      ComboBox->SetColors(&ListColors);

    if(!DialogMode.Check(DMODE_SHOW))
       return KEY_ESC;

    // �������� ��, ��� ���� � ������ �����!
    // if(EditLine->GetDropDownBox()) //???
    EditLine->GetString(Str,MaxLen);
    if(CurItem->Flags & (DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND))
       HiText2Str(Str, MaxLen, Str);
    ComboBox->SetSelectPos(ComboBox->FindItem(0,Str,LIFIND_EXACTMATCH),1);

    ComboBox->Show();

    OriginalPos=Dest=ComboBox->GetSelectPos();
    CurItem->IFlags.Set(DLGIIF_COMBOBOXNOREDRAWEDIT);
    while (!ComboBox->Done())
    {
      if (!GetDropDownOpened())
      {
        ComboBox->ProcessKey(KEY_ESC);
        continue;
      }
      INPUT_RECORD ReadRec;
      int Key=ComboBox->ReadInput(&ReadRec);

      if(CurItem->IFlags.Check(DLGIIF_COMBOBOXEVENTKEY) && ReadRec.EventType == KEY_EVENT)
      {
        if(DlgProc((HANDLE)this,DN_KEY,FocusPos,Key))
          continue;
      }
      else if(CurItem->IFlags.Check(DLGIIF_COMBOBOXEVENTMOUSE) && ReadRec.EventType == MOUSE_EVENT)
        if(!DlgProc((HANDLE)this,DN_MOUSEEVENT,0,(LONG_PTR)&ReadRec.Event.MouseEvent))
          continue;

      // ����� ����� �������� ���-�� ����, ��������,
      I=ComboBox->GetSelectPos();
      if (Key==KEY_TAB) // Tab � ������ - ������ Enter
      {
        ComboBox->ProcessKey(KEY_ENTER);
        continue; //??
      }
      if(I != Dest)
      {
        if(!DlgProc((HANDLE)this,DN_LISTCHANGE,CurFocusPos,I))
          ComboBox->SetSelectPos(Dest,Dest<I?-1:1); //????
        else
          Dest=I;

#if 0
        // �� ����� ��������� �� DropDown ����� - ��������� ��� ���� �
        // ��������� ������
        // ��������!!!
        //  ����� ��������� �������!
        if(EditLine->GetDropDownBox())
        {
          struct MenuItem *CurCBItem=ComboBox->GetItemPtr();
          EditLine->SetString(CurCBItem->Name);
          EditLine->Show();
          //EditLine->FastShow();
        }
#endif
      }
      // ��������� multiselect ComboBox
      // ...
      ComboBox->ProcessInput();
    }
    CurItem->IFlags.Clear(DLGIIF_COMBOBOXNOREDRAWEDIT);
    ComboBox->ClearDone();
    ComboBox->Hide();
    if (GetDropDownOpened()) // �������� �� ����������� ����?
      Dest=ComboBox->Modal::GetExitCode();
    else
      Dest=-1;

    if(Dest == -1)
      ComboBox->SetSelectPos(OriginalPos,0); //????

    SetDropDownOpened(FALSE); // ��������� ���� "��������" ����������.
    if (Dest<0)
    {
      Redraw();
      xf_free(Str);
      return KEY_ESC;
    }

    //ComboBox->GetUserData(Str,MaxLen,Dest);
    struct MenuItem *ItemPtr=ComboBox->GetItemPtr(Dest);
    if(CurItem->Flags & (DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND))
    {
       HiText2Str(Str, MaxLen, ItemPtr->PtrName());
       EditLine->SetString(Str);
    }
    else
      EditLine->SetString(ItemPtr->PtrName());
    EditLine->SetLeftPos(0);
    Redraw();
    xf_free(Str);
    return KEY_ENTER;
  }
  return KEY_ESC;
}
/* SVS $ */

//////////////////////////////////////////////////////////////////////////
/* Private:
   ��������� ���������� ������ �� �������
*/
/* $ 26.07.2000 SVS
  + �������������� �������� � SelectFromEditHistory ��� ���������
   ������ ������� � ������� (���� ��� ������������� ������ �����)
*/
BOOL Dialog::SelectFromEditHistory(struct DialogItem *CurItem,
                                   DlgEdit *EditLine,
                                   char *HistoryName,
                                   char *IStr,
                                   int MaxLen)
{
  CriticalSectionLock Lock(CS);

  if(!EditLine)
    return FALSE;

  char RegKey[NM],Str[4096];
  int I,Dest,Ret=FALSE;
  int Locked;
  int IsOk=FALSE, Done, IsUpdate;
  int ItemsCount;
  int LastSelected = 0;
  int IsDeleted=FALSE;
  int EditX1,EditY1,EditX2,EditY2;
  int CurFocusPos=FocusPos;

  sprintf(RegKey,fmtSavedDialogHistory,HistoryName);
  {
    // �������� ������� ������������� ����
    VMenu HistoryMenu("",NULL,0,Opt.Dialogs.CBoxMaxHeight,VMENU_ALWAYSSCROLLBAR|VMENU_COMBOBOX|VMENU_NOTCHANGE);

    EditLine->GetPosition(EditX1,EditY1,EditX2,EditY2);
    if (EditX2-EditX1<20)
      EditX2=EditX1+20;

    HistoryMenu.SetFlags(VMENU_SHOWAMPERSAND);
    HistoryMenu.SetBoxType(SHORT_SINGLE_BOX);

    SetDropDownOpened(TRUE); // ��������� ���� "��������" ����������.
    Done=FALSE;

    // �������� (��� ����������)
    CurItem->ListPtr=&HistoryMenu;

    while(!Done)
    {
      IsUpdate=FALSE;

      HistoryMenu.DeleteItems();

      // ���������� ������� ����
      for (ItemsCount=Dest=I=0; I < Opt.DialogsHistoryCount; I++)
      {
        itoa(I,PHisLine,10);
        GetRegKey(RegKey,HisLine,Str,"",sizeof(Str));
        if (*Str==0)
          continue;

        itoa(I,PHisLocked,10);
        GetRegKey(RegKey,HisLocked,Locked,0);

        int InsPos=HistoryMenu.AddItem(Str);
        struct MenuItem *HistoryItem=HistoryMenu.GetItemPtr(InsPos);
        HistoryItem->SetCheck(Locked);
        HistoryMenu.SetUserData(Str,0,InsPos);
        ItemsCount++;
      }
      if (ItemsCount==0)
        break;

      SetComboBoxPos();

      // �������� ��������
      if(!IsDeleted)
      {
        Dest=Opt.Dialogs.SelectFromHistory?HistoryMenu.FindItem(0,IStr,LIFIND_EXACTMATCH):-1;
        HistoryMenu.SetSelectPos(Dest!=-1?Dest:0, 1);
      }
      else
      {
        int D=1;
        IsDeleted=FALSE;
        if(LastSelected >= HistoryMenu.GetItemCount())
        {
          LastSelected=HistoryMenu.GetItemCount()-1;
          D=-1;
        }
        HistoryMenu.SetSelectPos(LastSelected,D);
      }

      //  ����� ���������� ������� �� ��������� �������� ���������
      BYTE RealColors[VMENU_COLOR_COUNT];
      struct FarListColors ListColors={0};
      ListColors.ColorCount=VMENU_COLOR_COUNT;
      ListColors.Colors=RealColors;
      HistoryMenu.GetColors(&ListColors);
      if(DlgProc((HANDLE)this,DN_CTLCOLORDLGLIST,CurItem->ID,(LONG_PTR)&ListColors))
        HistoryMenu.SetColors(&ListColors);
      HistoryMenu.Show();

      // �������� ���� ���������
      while (!HistoryMenu.Done())
      {
        if (!GetDropDownOpened())
        {
          Ret=FALSE;
          HistoryMenu.ProcessKey(KEY_ESC);
          Done=TRUE;
          continue;
        }

        int Key=HistoryMenu.ReadInput();

        if (Key==KEY_TAB) // Tab � ������ ������� - ������ Enter
        {
          HistoryMenu.ProcessKey(KEY_ENTER);
          Ret=TRUE;
          Done=TRUE;
          continue; //??
        }
        else if (Key==KEY_INS || Key==KEY_NUMPAD0) // Ins �������� ����� ������� �� ��������.
        {
          itoa(HistoryMenu.GetSelectPos(),PHisLocked,10);
          if (!HistoryMenu.GetSelection())
          {
            HistoryMenu.SetSelection(TRUE);
            SetRegKey(RegKey,HisLocked,1);
          }
          else
          {
            HistoryMenu.SetSelection(FALSE);
            DeleteRegValue(RegKey,HisLocked);
          }
          HistoryMenu.SetUpdateRequired(TRUE);
          HistoryMenu.Redraw();
          continue;
        }
        else if (Key==KEY_SHIFTDEL||Key==KEY_SHIFTNUMDEL||Key==KEY_SHIFTDECIMAL) // Shift-Del ������� ������� ����� ������� ������.
        {
          LastSelected=HistoryMenu.GetSelectPos();
          if (!HistoryMenu.GetSelection(LastSelected))
          {
            HistoryMenu.Hide();
            // ������� �� ������� ���.
            for (I=0; I < Opt.DialogsHistoryCount;I++)
            {
              itoa(I,PHisLocked,10);
              DeleteRegValue(RegKey,HisLocked);
              itoa(I,PHisLine,10);
              DeleteRegValue(RegKey,HisLine);
            }
            // ������� �� ������ ������ ��, ��� ���������
            HistoryMenu.DeleteItem(LastSelected);
            // ���������� ������ � �������
            for (Dest=I=0; I < HistoryMenu.GetItemCount(); I++)
            {
               HistoryMenu.GetUserData(Str,sizeof(Str),I);
               itoa(Dest,PHisLine,10);
               SetRegKey(RegKey,HisLine,Str);
               if(HistoryMenu.GetSelection(I))
               {
                 itoa(Dest,PHisLocked,10);
                 SetRegKey(RegKey,HisLocked,TRUE);
               }
               Dest++;
            }
            HistoryMenu.SetUpdateRequired(TRUE);
            IsDeleted=TRUE;
            IsUpdate=TRUE;
            break;
          }
          continue;
        }
        else if (Key==KEY_DEL||Key==KEY_NUMDEL) // Del ������� ������� ������.
        {
          LastSelected=HistoryMenu.GetSelectPos();

          if (!Opt.Confirm.HistoryClear ||
              (Opt.Confirm.HistoryClear &&
               Message(MSG_WARNING,2,MSG(MHistoryTitle),
                       MSG(MHistoryClear),
                       MSG(MClear),MSG(MCancel))==0))
          {
            HistoryMenu.Hide();

            // ������� �� �������
            for (I=0; I < Opt.DialogsHistoryCount;I++)
            {
              itoa(I,PHisLocked,10);
              DeleteRegValue(RegKey,HisLocked);
              itoa(I,PHisLine,10);
              DeleteRegValue(RegKey,HisLine);
            } /* for */

            // ������� � ������
            for (Dest=I=0; I < HistoryMenu.GetItemCount(); I++)
            {
              if (HistoryMenu.GetSelection(I))
              {
                HistoryMenu.GetUserData(Str,sizeof(Str),I);
                itoa(Dest,PHisLine,10);
                SetRegKey(RegKey,HisLine,Str);
                itoa(Dest,PHisLocked,10);
                SetRegKey(RegKey,HisLocked,TRUE);
                Dest++;
              } /* if */
            } /* for */
          } /* if */
          HistoryMenu.SetUpdateRequired(TRUE);
          IsUpdate=TRUE;
          break;
        }

        // ���� ���� �������� DN_LISTCHANGE

        HistoryMenu.ProcessInput();
      }

      if(IsUpdate)
        continue;

      int ExitCode=HistoryMenu.Modal::GetExitCode();
      if (ExitCode<0)
      {
        Ret=FALSE;
        Done=TRUE;
//        break;
      }
      else
      {
        HistoryMenu.GetUserData(Str,Min((int)sizeof(Str),MaxLen),ExitCode);
        Ret=TRUE;
        Done=TRUE;
        IsOk=TRUE;
      }
    }

    // ������� (�� �����)
    CurItem->ListPtr=NULL;

    SetDropDownOpened(FALSE); // ��������� ���� "��������" ����������.
  }

  if(IsOk)
  {
    EditLine->SetString(Str);
    EditLine->SetLeftPos(0);
    EditLine->SetClearFlag(0);
    Dialog::SendDlgMessage((HANDLE)this,DN_EDITCHANGE,CurFocusPos,0);
    Redraw();
  }
  return Ret;
}

//////////////////////////////////////////////////////////////////////////
/* Private:
   ������ � �������� - ���������� � reorder ������
*/
int Dialog::AddToEditHistory(char *AddStr,char *HistoryName)
{
  CriticalSectionLock Lock(CS);

#define MAXSIZESTRING 4096
  int AddLine=-1, I, J, Locked, HistCount, LockedCount=0;
  char Str[MAXSIZESTRING];
  char RegKey[NM];

  sprintf(RegKey,fmtSavedDialogHistory,HistoryName);

  if (*AddStr==0)
  {
    SetRegKey(RegKey,"Flags",(DWORD)0);
    return FALSE;
  }

  struct HistArray{
    char *Str;
    int  Locked;
  } *His,*HisTemp;

  His=(struct HistArray*)alloca(Opt.DialogsHistoryCount*sizeof(struct HistArray));
  HisTemp=(struct HistArray*)alloca((Opt.DialogsHistoryCount+1)*sizeof(struct HistArray));

  if(!His || !HisTemp)
    return FALSE;

  memset(His,0,Opt.DialogsHistoryCount*sizeof(struct HistArray));
  memset(HisTemp,0,(Opt.DialogsHistoryCount+1)*sizeof(struct HistArray));

  // Read content & delete
  for (HistCount=I=0; I < Opt.DialogsHistoryCount; I++)
  {
    itoa(I,PHisLocked,10);
    GetRegKey(RegKey,HisLocked,Locked,0);
    itoa(I,PHisLine,10);
    GetRegKey(RegKey,HisLine,Str,"",sizeof(Str));

    if(*Str)
    {
      if((His[HistCount].Str=xf_strdup(Str)) != NULL)
      {
        His[HistCount].Locked=Locked;
        LockedCount+=Locked;
        DeleteRegValue(RegKey,HisLocked);
        DeleteRegValue(RegKey,HisLine);
        ++HistCount;
      }
    }
  }

  // ���� ������ ����������
  for (I=0; I < HistCount; I++)
    if (!LCStricmp(AddStr,His[I].Str))
    {
      // ����� ������! ���� ������� ���� ���� ����������
      if(AddLine == -1 || AddLine != -1 && His[I].Locked)
        AddLine=I;
    }
  /*
    ����� � ���:
      ���� AddLine == -1, �� ����� ������ ���� � �������
      ���� LockedCount == Opt.DialogsHistoryCount, ��� ��������!
  */

  // � ����� �� ��������� ��?...
  if(LockedCount == Opt.DialogsHistoryCount && AddLine == -1)
    J=0;
  else // ...�� ������ �����, �� � �����!
  {
    // ��������� � ������ � ������ ������������
    HisTemp[0].Str=xf_strdup(AddStr);
    HisTemp[0].Locked=(AddLine == -1)?0:His[AddLine].Locked;
    J=1;
  }

  // Locked ������
  for (I=0; I < HistCount; I++)
  {
    if(His[I].Locked && His[I].Str)
    {
      if(AddLine == I)
        continue;
      HisTemp[J].Str=His[I].Str;
      /* $ 27.11.2001 DJ
         ��� ����� ����������� �� ����
      */
      His[I].Str = NULL;
      /* DJ $ */
      HisTemp[J].Locked=1;
      ++J;
    }
  }

  // UnLocked
  for (I=0; I < HistCount; I++)
  {
    if(!His[I].Locked && His[I].Str)
    {
      if(AddLine == I)
        continue;
      HisTemp[J].Str=His[I].Str;
      /* $ 27.11.2001 DJ
         ��� ����� ����������� �� ����
      */
      His[I].Str=NULL;
      /* DJ $ */
      HisTemp[J].Locked=0;
      ++J;
    }
  }

  // ��������� ���������
  for (I=0; I < Opt.DialogsHistoryCount; I++)
  {
    if(HisTemp[I].Str)
    {
      // ����� ����� ������������
      for(J=I+1; J < Opt.DialogsHistoryCount; ++J)
      {
        if(HisTemp[J].Str)
        {
          if(!LCStricmp(HisTemp[I].Str,HisTemp[J].Str))
          {
            xf_free(HisTemp[J].Str);
            HisTemp[J].Str=NULL;
          }
        }
      }
    }
  }
  // ����� � HisTemp ����� ��������������� ������

  // Save History
  for (J=I=0; I < Opt.DialogsHistoryCount; I++)
  {
    if(HisTemp[I].Str)
    {
      itoa(J,PHisLocked,10);
      itoa(J,PHisLine,10);
      SetRegKey(RegKey,HisLine,HisTemp[I].Str);
      if(HisTemp[I].Locked)
        SetRegKey(RegKey,HisLocked,HisTemp[I].Locked);
      xf_free(HisTemp[I].Str);
      ++J;
    }
  }

  /* $ 27.11.2001 DJ
     �� ������� ���������� ���������� ������
  */
  for (I=0; I<Opt.DialogsHistoryCount; I++)
    if (His[I].Str)
      xf_free(His[I].Str);
  /* DJ $ */

  SetRegKey(RegKey,"Flags",1);
  return TRUE;
}


//////////////////////////////////////////////////////////////////////////
/* Public, Static:
   �������� �� HotKey
*/
/* $ 20.02.2001 SVS
   ��������� ��������� IsKeyHighlighted � ��������� Alt- ��
   ������� ��� ��������*/
int Dialog::IsKeyHighlighted(const char *Str,int Key,int Translate,int AmpPos)
{
  if(AmpPos == -1)
  {
    if ((Str=strchr(Str,'&'))==NULL)
      return(FALSE);
    AmpPos=1;
  }
  else
  {
    if(AmpPos >= (int)strlen(Str))
      return FALSE;
    Str=Str+AmpPos;
    AmpPos=0;
    if(Str[AmpPos] == '&')
      AmpPos++;
  }
//_SVS(SysLog("'%s' (%d)",Str+AmpPos,AmpPos));
  int UpperStrKey=LocalUpper((int)Str[AmpPos]);
  /* $ 08.11.2000 SVS
     ������� �������� ����� ������ ��� hotkey (������������ ��������)
  */
  /* 28.12.2000 SVS
    + ��������� ��������� Opt.HotkeyRules */
  if (Key < 256)
  {
    int KeyToKey=LocalKeyToKey(Key);
    return(UpperStrKey == (int)LocalUpper(Key) ||
      Translate &&
      (!Opt.HotkeyRules && UpperStrKey==(int)LocalUpper(KeyToKey) ||
        Opt.HotkeyRules && LocalKeyToKey(UpperStrKey)==KeyToKey));
  }

  if(Key&KEY_ALT)
  {
    int AltKey=Key&(~KEY_ALT);
    if(AltKey < 256)
    {
      if (AltKey >= '0' && AltKey <= '9')
        return(AltKey==UpperStrKey);

      int AltKeyToKey=LocalKeyToKey(AltKey);
      if (AltKey > ' ' && AltKey <= 255)
  //         (AltKey=='-'  || AltKey=='/' || AltKey==','  || AltKey=='.' ||
  //          AltKey=='\\' || AltKey=='=' || AltKey=='['  || AltKey==']' ||
  //          AltKey==':'  || AltKey=='"' || AltKey=='~'))
      {
        return(UpperStrKey==(int)LocalUpper(AltKey) ||
               Translate &&
               (!Opt.HotkeyRules && UpperStrKey==(int)LocalUpper(AltKeyToKey) ||
                  Opt.HotkeyRules && LocalKeyToKey(UpperStrKey)==AltKeyToKey));
      }
    }
  }
  /* SVS $*/
  /* SVS $*/
  return(FALSE);
}
/* SVS $ */


int Dialog::CheckHighlights(BYTE CheckSymbol,int StartPos)
{
  CriticalSectionLock Lock(CS);

  int I, Type;
  DWORD Flags;
  if(StartPos < 0)
    StartPos=0;

  for (I=StartPos;I<ItemCount;I++)
  {
    Type=Item[I].Type;
    Flags=Item[I].Flags;

    if ((!IsEdit(Type) || (Type == DI_COMBOBOX && (Flags&DIF_DROPDOWNLIST))) &&
        (Flags & (DIF_SHOWAMPERSAND|DIF_DISABLE|DIF_HIDDEN))==0)
    {
      const char *ChPtr=strchr(Item[I].Data,'&');
      if (ChPtr)
      {
        BYTE Ch=ChPtr[1];
        if(Ch && LocalUpper(CheckSymbol) == LocalUpper(Ch))
          return I;
      }
      else if(!CheckSymbol)
        return I;
    }
  }
  return -1;
}

//////////////////////////////////////////////////////////////////////////
/* Private:
   ���� �������� Alt-???
*/
int Dialog::ProcessHighlighting(int Key,int FocusPos,int Translate)
{
  CriticalSectionLock Lock(CS);

  int I, Type;
  DWORD Flags;

  for (I=0;I<ItemCount;I++)
  {
    Type=Item[I].Type;
    Flags=Item[I].Flags;

    if ((!IsEdit(Type) || (Type == DI_COMBOBOX && (Flags&DIF_DROPDOWNLIST))) &&
        (Flags & (DIF_SHOWAMPERSAND|DIF_DISABLE|DIF_HIDDEN))==0)
      if (IsKeyHighlighted(Item[I].Data,Key,Translate))
      {
        int DisableSelect=FALSE;

        // ���� ���: DlgEdit(���� �������) � DI_TEXT � ���� ������, ��...
        if (I>0 &&
            Type==DI_TEXT &&                              // DI_TEXT
            IsEdit(Item[I-1].Type) &&                     // � ��������
            Item[I].Y1==Item[I-1].Y1 &&                   // � ��� � ���� ������
            (I+1 < ItemCount && Item[I].Y1!=Item[I+1].Y1)) // ...� ��������� ������� � ������ ������
        {
          // ������� ������� � ����������� ����� ��������� ��������� �������, � �����...
          if(!DlgProc((HANDLE)this,DN_HOTKEY,I,Key))
            break; // ������� �� ���������� ���������...
          // ... ���� ���������� ������� ���������� ��� �������, ����� �������.
          if ((Item[I-1].Flags&(DIF_DISABLE|DIF_HIDDEN)) != 0) // � �� ����������
             break;
          I=ChangeFocus(I,-1,FALSE);
          DisableSelect=TRUE;
        }
        else if (Item[I].Type==DI_TEXT      || Item[I].Type==DI_VTEXT ||
                 Item[I].Type==DI_SINGLEBOX || Item[I].Type==DI_DOUBLEBOX)
        {
          if(I+1 < ItemCount) // ...� ��������� �������
          {
            // ������� ������� � ����������� ����� ��������� ��������� �������, � �����...
            if(!DlgProc((HANDLE)this,DN_HOTKEY,I,Key))
              break; // ������� �� ���������� ���������...
            // ... ���� ��������� ������� ���������� ��� �������, ����� �������.
            if ((Item[I+1].Flags&(DIF_DISABLE|DIF_HIDDEN)) != 0) // � �� ����������
              break;
            I=ChangeFocus(I,1,FALSE);
            DisableSelect=TRUE;
          }
        }
        /* $ 29.08.2000 SVS
           - ������ ����������� �����-��� - ������� ProcessHighlighting
           MY> ������ � ��������� ����� �������. � ���� � ���� ������� �������.
           MY> ������ � ��������� ���� ������, ���� Alt-������ ����� �
           MY> ��������� ������������� �����.

           � ������ ������ ���������� :-)))
        */
        // ������� � ����������� ����� ��������� ��������� �������
        if(!DlgProc((HANDLE)this,DN_HOTKEY,I,Key))
          break; // ������� �� ���������� ���������...
        ChangeFocus2(FocusPos,I); //??
        if(FocusPos != I)
        {
          ShowDialog(FocusPos);
          ShowDialog(I);
        }
        /* SVS $ */
        if ((Item[I].Type==DI_CHECKBOX || Item[I].Type==DI_RADIOBUTTON) &&
            (!DisableSelect || (Item[I].Flags & DIF_MOVESELECT)))
        {
          Do_ProcessSpace();
          return(TRUE);
        }
        else if (Item[I].Type==DI_BUTTON)
        {
          ProcessKey(KEY_ENTER);
          return(TRUE);
        }
        // ��� ComboBox`� - "����������" ��������� //????
        else if (Item[I].Type==DI_COMBOBOX)
        {
          ProcessOpenComboBox(Item[I].Type,Item+I,I);
          //ProcessKey(KEY_CTRLDOWN);
          return(TRUE);
        }
        return(TRUE);
      }
  }
  return(FALSE);
}


//////////////////////////////////////////////////////////////////////////
/* $ 31.07.2000 tran
   + ������� ������������� ��������� edit ������� */
/* $ 07.08.2000 SVS
   + � ��� ListBox ������?*/
void Dialog::AdjustEditPos(int dx, int dy)
{
  CriticalSectionLock Lock(CS);

  struct DialogItem *CurItem;
  int I;
  int x1,x2,y1,y2;

  if(!DialogMode.Check(DMODE_CREATEOBJECTS))
    return;

  ScreenObject *DialogScrObject;
  for (I=0; I < ItemCount; I++)
  {
    CurItem=&Item[I];
    int Type=CurItem->Type;
    if (CurItem->ObjPtr  && IsEdit(Type) ||
        CurItem->ListPtr && Type == DI_LISTBOX)
    {
       if(Type == DI_LISTBOX)
         DialogScrObject=(ScreenObject *)CurItem->ListPtr;
       else
         DialogScrObject=(ScreenObject *)CurItem->ObjPtr;
       DialogScrObject->GetPosition(x1,y1,x2,y2);
       x1+=dx;
       x2+=dx;
       y1+=dy;
       y2+=dy;
       DialogScrObject->SetPosition(x1,y1,x2,y2);
    }
  }
  ProcessCenterGroup();
}
/* SVS $ */
/* tran 31.07.2000 $ */


//////////////////////////////////////////////////////////////////////////
/* $ 11.08.2000 SVS
   ������ � ���. ������� ���������� �������
   ���� ������� ����������� (����������)
*/
void Dialog::SetDialogData(LONG_PTR NewDataDialog)
{
  DataDialog=NewDataDialog;
}
/* SVS $ */

//////////////////////////////////////////////////////////////////////////
/* $ 29.06.2007 yjh\
   ��� ��������� ����� ����������� �����/������� ��������� ����� ��������
   ���������������� ������� � ����� ����� (�����).
   ����� ���� ���������� ������ �������������� ����� ����� ������� ��������
*/
long WaitUserTime;

/* $ 11.08.2000 SVS
   + ��� ����, ����� ������� DM_CLOSE ����� �������������� Process
*/
void Dialog::Process()
{
  /* $ 17.05.2001 DJ
     NDZ
  */
//  if(DialogMode.Check(DMODE_SMALLDIALOG))
    SetRestoreScreenMode(TRUE);

  InitDialog();

  TaskBarError *TBE=DialogMode.Check(DMODE_WARNINGSTYLE)?new TaskBarError:NULL;

  if(ExitCode == -1)
  {
    static LONG in_dialog = -1;

    clock_t btm=0;
    long    save=0;

    DialogMode.Set(DMODE_BEGINLOOP);
    if (!InterlockedIncrement(&in_dialog))
    {
      btm = clock();
      save = WaitUserTime;
      WaitUserTime = -1;
    }
    FrameManager->ExecuteModal (this);
    save += (clock() - btm);
    if (InterlockedDecrement(&in_dialog) == -1)
      WaitUserTime = save;
  }
  /* DJ $ */
  if(TBE)
  {
    delete TBE;
  }
}
/* SVS $ */

/* $ 18.05.2001 DJ */

void Dialog::CloseDialog()
{
  CriticalSectionLock Lock(CS);

  GetDialogObjectsData();
  if (DlgProc ((HANDLE)this,DN_CLOSE,ExitCode,0))
  {
    DialogMode.Set(DMODE_ENDLOOP);
    Hide();

    if(DialogMode.Check(DMODE_BEGINLOOP) && (DialogMode.Check(DMODE_MSGINTERNAL) || FrameManager->ManagerStarted()))
      FrameManager->DeleteFrame (this);

    _DIALOG(CleverSysLog CL("Close Dialog"));
  }
}

/* DJ $ */

/* $ 17.05.2001 DJ
   ��������� help topic'� � ������ �������, �������� ������������ ����
   �� Modal
*/

void Dialog::SetHelp (const char *Topic)
{
  CriticalSectionLock Lock(CS);

  if (HelpTopic)
    delete[] HelpTopic;
  HelpTopic=NULL;

  if(Topic && *Topic)
  {
    HelpTopic = new char [strlen (Topic)+1];
    if(HelpTopic)
      strcpy (HelpTopic, Topic);
  }
}

void Dialog::ShowHelp()
{
  CriticalSectionLock Lock(CS);

  if (HelpTopic && *HelpTopic)
  {
    Help Hlp (HelpTopic);
  }
}

void Dialog::ClearDone()
{
  CriticalSectionLock Lock(CS);

  ExitCode=-1;
  DialogMode.Clear(DMODE_ENDLOOP);
}

void Dialog::SetExitCode(int Code)
{
  CriticalSectionLock Lock(CS);

  ExitCode=Code;
  DialogMode.Set(DMODE_ENDLOOP);
  //CloseDialog();
}

/* DJ $ */

/* $ 19.05.2001 DJ
   ���������� ���� �������� ��� ���� �� F12
*/

int Dialog::GetTypeAndName (char *Type, char *Name)
{
  CriticalSectionLock Lock(CS);

  if ( Type )
    strcpy (Type, MSG(MDialogType));

  if ( Name )
  {
    *Name = 0;

    const char *lpTitle = GetDialogTitle();

    if ( lpTitle )
      strcpy (Name, lpTitle);
  }

  return MODALTYPE_DIALOG;
}

/* DJ $ */

int Dialog::GetMacroMode()
{
  return MACRO_DIALOG;
}

int Dialog::FastHide()
{
  return Opt.AllCtrlAltShiftRule & CASR_DIALOG;
}

void Dialog::ResizeConsole()
{
  CriticalSectionLock Lock(CS);

  COORD c;
  DialogMode.Set(DMODE_RESIZED);

  if(IsVisible())
    Hide();

  // ��������� �������������� ��������� ������� (����� �� ������������ :-)
  c.X=ScrX+1; c.Y=ScrY+1;
  Dialog::SendDlgMessage((HANDLE)this,DN_RESIZECONSOLE,0,(LONG_PTR)&c);

  // !!!!!!!!!!! ����� ����� ��������� ��������� ��������� !!!!!!!!!!!
  //c.X=((X1*100/PrevScrX)*ScrX)/100;
  //c.Y=((Y1*100/PrevScrY)*ScrY)/100;
  // !!!!!!!!!!! ����� ����� ��������� ��������� ��������� !!!!!!!!!!!

  c.X=c.Y=-1;
  Dialog::SendDlgMessage((HANDLE)this,DM_MOVEDIALOG,TRUE,(LONG_PTR)&c);
  Dialog::SetComboBoxPos();
};

//void Dialog::OnDestroy()
//{
//  /* $ 21.04.2002 KM
//  //  ��� ������� �������� ��� �������� ��� ������� ���������
//  //  � ������� �������� � ����������� �������.
//  if(DialogMode.Check(DMODE_RESIZED))
//  {
//    Frame *BFrame=FrameManager->GetBottomFrame();
//    if(BFrame)
//      BFrame->UnlockRefresh();
//    /* $ 21.04.2002 KM
//        � ��� ���� DM_KILLSAVESCREEN ����� ������ ������. ��������
//        ������� ���������� ��� �������������� ShadowSaveScr � ���
//        ���: "���������" ������������.
//    */
//    Dialog::SendDlgMessage((HANDLE)this,DM_KILLSAVESCREEN,0,0);
//    /* KM $ */
//  }
//  /* KM $ */
//};

LONG_PTR WINAPI Dialog::DlgProc(HANDLE hDlg,int Msg,int Param1,LONG_PTR Param2)
{
  FarDialogEvent de={hDlg,Msg,Param1,Param2,0};
  LONG_PTR ret;
  if(CtrlObject->Plugins.ProcessDialogEvent(DE_DLGPROCINIT,&de))
    return de.Result;
  ret=RealDlgProc(hDlg,Msg,Param1,Param2);
  de.Result=ret;
  if(CtrlObject->Plugins.ProcessDialogEvent(DE_DLGPROCEND,&de))
    return de.Result;
  return ret;
}

//////////////////////////////////////////////////////////////////////////
/* $ 28.07.2000 SVS
   ������� ��������� ������� (�� ���������)
   ��� ������ ��� ������� � �������� ��������� ������� ��������� �������.
   �.�. ����� ������ ���� ��� ��������� ���� ���������!!!
*/
/* $ 02.07.2001 KM
   - ��������� �� �������������� (� ������� �� ������) ����
     ��� Param1==-1.
*/

LONG_PTR WINAPI Dialog::DefDlgProc(HANDLE hDlg,int Msg,int Param1,LONG_PTR Param2)
{
  _DIALOG(CleverSysLog CL("Dialog.DefDlgProc()"));
  _DIALOG(SysLog("hDlg=%p, Msg=%s, Param1=%d (0x%08X), Param2=%d (0x%08X)",hDlg,_DLGMSG_ToName(Msg),Param1,Param1,Param2,Param2));

  if(!hDlg)
    return 0;

  FarDialogEvent de={hDlg,Msg,Param1,Param2,0};
  if(CtrlObject->Plugins.ProcessDialogEvent(DE_DEFDLGPROCINIT,&de))
    return de.Result;

  Dialog* Dlg=(Dialog*)hDlg;

  CriticalSectionLock Lock(Dlg->CS);

  struct DialogItem *CurItem=NULL;
  char *Ptr=NULL;
  int Type=0;

  switch(Msg)
  {
    case DN_INITDIALOG:
      return FALSE; // ��������� �� ����!

    case DM_CLOSE:
      return TRUE;  // �������� � ���������

    case DN_KILLFOCUS:
      return -1;    // "�������� � ������� ������"

    case DN_GOTFOCUS:
      return 0;     // always 0

    case DN_HELP:
      return Param2; // ��� ��������, �� �...

    case DN_DRAGGED:
      return TRUE; // �������� � ������������.

    case DN_DRAWDIALOGDONE:
    {
      if(Param1 == 1)  // ����� ���������� "�������"?
      {
        /* $ 03.08.2000 tran
           ����� ������ � ���� ����� ��������� � ������� �����������
           1) ����� ������ ������������ � ����
           2) ����� ������ ������������ �� ����
           ������ ����� ������� ������� �� ����� */
        Text(Dlg->X1,Dlg->Y1,0xCE,"\\");
        Text(Dlg->X1,Dlg->Y2,0xCE,"/");
        Text(Dlg->X2,Dlg->Y1,0xCE,"/");
        Text(Dlg->X2,Dlg->Y2,0xCE,"\\");
      }
      return TRUE;
    }

    case DN_DRAWDIALOG:
    {
      return TRUE;
    }

    case DN_CTLCOLORDIALOG:
      return Param2;

    case DN_CTLCOLORDLGITEM:
      return Param2;

    case DN_CTLCOLORDLGLIST:
      return FALSE;

    case DN_ENTERIDLE:
      return 0;     // always 0
  }

  // �������������� ��������...
  if(Param1 >= Dlg->ItemCount && Dlg->Item)
    return 0;

  if (Param1>=0)
  {
    CurItem=&Dlg->Item[Param1];
    Type=CurItem->Type;
    Ptr=CurItem->Data;
  }

  switch(Msg)
  {
    case DN_MOUSECLICK:
      return FALSE;

    case DN_DRAWDLGITEM:
      return TRUE;

    case DN_HOTKEY:
      return TRUE;

    case DN_EDITCHANGE:
      return TRUE;

    case DN_BTNCLICK:
      return ((Type==DI_BUTTON && !(CurItem->Flags&DIF_BTNNOCLOSE))?FALSE:TRUE);

    case DN_LISTCHANGE:
      return TRUE;

    /* $ 23.08.2000 SVS
       + �������� �������(�)
    */
    case DN_KEY:
      return FALSE;
    /* SVS $ */

    case DN_MOUSEEVENT:
      return TRUE;

    case DM_GETSELECTION: // Msg=DM_GETSELECTION, Param1=ID, Param2=*EditorSelect
      return FALSE;

    case DM_SETSELECTION:
      return FALSE;
  }

  return 0;
}
/* SVS $ */

LONG_PTR Dialog::CallDlgProc (int nMsg, int nParam1, LONG_PTR nParam2)
{
    CriticalSectionLock Lock (CS);

    return Dialog::DlgProc ((HANDLE)this, nMsg, nParam1, nParam2);
}


//////////////////////////////////////////////////////////////////////////
/* $ 28.07.2000 SVS
   ������� ��������� �������
   ��������� ��������� ��� ������� ������������ ����, �� ��������� ����������
   ����������� �������.
*/
/* $ 02.07.2001 KM
   - ��������� �� �������������� (� ������� �� ������) ����
     ��� Param1==-1.
*/
LONG_PTR WINAPI Dialog::SendDlgMessage(HANDLE hDlg,int Msg,int Param1,LONG_PTR Param2)
{
  if(!hDlg)
    return 0;

  Dialog* Dlg=(Dialog*)hDlg;

  CriticalSectionLock Lock (Dlg->CS);

  int I;

  _DIALOG(CleverSysLog CL("Dialog.SendDlgMessage()"));
  _DIALOG(SysLog("hDlg=%p, Msg=%s, Param1=%d (0x%08X), Param2=%d (0x%08X)",hDlg,_DLGMSG_ToName(Msg),Param1,Param1,Param2,Param2));

  // ���������, �������� ������ ������� � �� ������������� ��������
  switch(Msg)
  {
    /*****************************************************************/
    case DM_RESIZEDIALOG:
      // ������� ����� RESIZE.
      Param1=-1;

    /*****************************************************************/
    /* $ 30.08.2000 SVS
        + ����������� ����������� �������
    */
    case DM_MOVEDIALOG:
    {
      int W1,H1;

      /* $ 10.08.2001 KM
        - ������� ����������� ������ �������.
      */
      W1=Dlg->X2-Dlg->X1+1;
      H1=Dlg->Y2-Dlg->Y1+1;
      /* KM $ */
      // ���������
      Dlg->OldX1=Dlg->X1;
      Dlg->OldY1=Dlg->Y1;
      Dlg->OldX2=Dlg->X2;
      Dlg->OldY2=Dlg->Y2;
      /* $ 30.05.2001 KM
         - �������� ������������� ������� � ��������� �������
      */
      // �����������
      if(Param1>0)   // ���������?
      {
        Dlg->X1=((COORD*)Param2)->X;
        Dlg->Y1=((COORD*)Param2)->Y;
        /* $ 10.08.2001 KM
          - ������� ����������� ���������� X2 � Y2.
        */
        /* $ 06.04.2002 KM
          - �� �����, �� ��� ���� � � ������� ��� ��������
            �� 1 ������ � ������ �������, �� ������� ������
            ��� �������� ������ ������� ������� �� �������
            ���������� ����� � ��������.
        */
        Dlg->X2=W1;
        Dlg->Y2=H1;
        /* KM $ */
        /* KM $ */
        Dlg->CheckDialogCoord();
      }
      else if(Param1 == 0)   // ������ ������������
      {
        Dlg->X1+=((COORD*)Param2)->X;
        Dlg->Y1+=((COORD*)Param2)->Y;
      }
      else // Resize, Param2=width/height
      {
        int OldW1,OldH1;
        OldW1=W1;
        OldH1=H1;
        W1=((COORD*)Param2)->X;
        H1=((COORD*)Param2)->Y;

        Dlg->RealWidth = W1;
        Dlg->RealHeight = H1;
        /* $ 11.10.2001 KM
          - ��� ���� ��������� ��� ����������, � ������ ���������������
            ������ ���� ������� �� ������� ������.
        */
        if(Dlg->X1+W1>ScrX)
          Dlg->X1=ScrX-W1+1;
        if(Dlg->Y1+H1>ScrY+1)
          Dlg->Y1=ScrY-H1+2;
        /* KM $ */

        if (W1<OldW1 || H1<OldH1)
        {
          Dlg->DialogMode.Set(DMODE_DRAWING);
          DialogItem *Item;
          SMALL_RECT Rect;
          for (I=0;I<Dlg->ItemCount;I++)
          {
            Item=Dlg->Item+I;
            if(Item->Flags&DIF_HIDDEN)
              continue;
            Rect.Left=Item->X1;
            Rect.Top=Item->Y1;
            if (Item->X2>=W1)
            {
              Rect.Right=Item->X2-(OldW1-W1);
              Rect.Bottom=Item->Y2;
              Dlg->SetItemRect(I,&Rect);
            }
            if (Item->Y2>=H1)
            {
              Rect.Right=Item->X2;
              Rect.Bottom=Item->Y2-(OldH1-H1);
              Dlg->SetItemRect(I,&Rect);
            }
          }
          Dlg->DialogMode.Clear(DMODE_DRAWING);
        }
      }
      /* KM $ */
      // ��������� � ���������������
      if(Dlg->X1<0)
        Dlg->X1=0;
      if(Dlg->Y1<0)
        Dlg->Y1=0;
      /* $ 11.10.2001 KM
        - ��� ���� ��������� ��� ����������, � ������ ���������������
          ������ ���� ������� �� ������� ������.
      */
      if(Dlg->X1+W1>ScrX)
        Dlg->X1=ScrX-W1+1;
      if(Dlg->Y1+H1>ScrY+1)
        Dlg->Y1=ScrY-H1+2;
      /* KM $ */
      /* $ 10.08.2001 KM
        - ������� ����������� ���������� X2 � Y2.
      */
      Dlg->X2=Dlg->X1+W1-1;
      Dlg->Y2=Dlg->Y1+H1-1;
      /* KM $ */

      Dlg->CheckDialogCoord();

      if(Param1 < 0)   // ������?
      {
        ((COORD*)Param2)->X=Dlg->X2-Dlg->X1+1;
        ((COORD*)Param2)->Y=Dlg->Y2-Dlg->Y1+1;
      }
      else
      {
        ((COORD*)Param2)->X=Dlg->X1;
        ((COORD*)Param2)->Y=Dlg->Y1;
      }

      I=Dlg->IsVisible();// && Dlg->DialogMode.Check(DMODE_INITOBJECTS);
      if(I) Dlg->Hide();
      // �������.
      Dlg->AdjustEditPos(Dlg->X1-Dlg->OldX1,Dlg->Y1-Dlg->OldY1);
      if(I) Dlg->Show(); // ������ ���� ������ ��� �����

      return Param2;
    }
    /* SVS $ */

    /*****************************************************************/
    case DM_REDRAW:
    {
      if(Dlg->DialogMode.Check(DMODE_INITOBJECTS))
        Dlg->Show();
      return 0;
    }

    /*****************************************************************/
    /* $ 18.08.2000 SVS
       + ����������/���������� ��������� �������
    */

    case DM_ENABLEREDRAW:
    {
      int Prev=Dlg->IsEnableRedraw;

      if(Param1 == TRUE)
        Dlg->IsEnableRedraw++;
      else if(Param1 == FALSE)
        Dlg->IsEnableRedraw--;

      //Edit::DisableEditOut(!Dlg->IsEnableRedraw?FALSE:TRUE);

      if(!Dlg->IsEnableRedraw && Prev != Dlg->IsEnableRedraw)
        if(Dlg->DialogMode.Check(DMODE_INITOBJECTS))
        {
          Dlg->ShowDialog();
//          Dlg->Show();
          ScrBuf.Flush();
        }
      return Prev;
    }

/*
    case DM_ENABLEREDRAW:
    {
      if(Param1)
        Dlg->IsEnableRedraw++;
      else
        Dlg->IsEnableRedraw--;

      if(!Dlg->IsEnableRedraw)
        if(Dlg->DialogMode.Check(DMODE_INITOBJECTS))
        {
          Dlg->ShowDialog();
          ScrBuf.Flush();
//          Dlg->Show();
        }
      return 0;
    }
*/
    /* SVS $ */

    /*****************************************************************/
    /* $ 23.08.2000 SVS
       + ��������/�������� ������.
    */
    case DM_SHOWDIALOG:
    {
//      if(!Dlg->IsEnableRedraw)
      {
        if(Param1)
        {
          /* $ 20.04.2002 KM
            ������� ���������� ��� �������� �������, � ���������
            ������ ������ �������� ������, ��� ������������
            ������ ������!
          */
          if(!Dlg->IsVisible())
          {
            Dlg->Unlock();
            Dlg->Show();
          }
        }
        else
        {
          if(Dlg->IsVisible())
          {
            Dlg->Hide();
            Dlg->Lock();
          }
          /* KM $ */
        }
      }
      return 0;
    }
    /* SVS $ */

    /*****************************************************************/
    /* $ 23.08.2000 SVS
       + ����������/����� ������ �������.
    */
    case DM_SETDLGDATA:
    {
      LONG_PTR PrewDataDialog=Dlg->DataDialog;
      Dlg->DataDialog=Param2;
      return PrewDataDialog;
    }

    /*****************************************************************/
    case DM_GETDLGDATA:
    {
      return Dlg->DataDialog;
    }
    /* SVS $ */

    /*****************************************************************/
    /* $ 23.08.2000 SVS
       + ������� �������(�)
    */
    case DM_KEY:
    {
      int *KeyArray=(int*)Param2;
      Dlg->DialogMode.Set(DMODE_KEY);
      for(I=0; I < Param1; ++I)
        Dlg->ProcessKey(KeyArray[I]);
      Dlg->DialogMode.Clear(DMODE_KEY);
      return 0;
    }
    /* SVS $ */

    /*****************************************************************/
    /* $ 23.08.2000 SVS
       + ������������� ������� ������
    */
    case DM_CLOSE:
    {
      if(Param1 == -1)
        Dlg->ExitCode=Dlg->FocusPos;
      else
        Dlg->ExitCode=Param1;
      /* $ 17.05.2001 DJ */
      Dlg->CloseDialog();
      /* DJ $ */
      return TRUE;  // �������� � ���������
    }
    /* SVS $ */

    /*****************************************************************/
    /* $ 25.08.2000 SVS
        + �������� ���������� ����������� ����
    */
    case DM_GETDLGRECT:
    {
      if(Param2 && !IsBadWritePtr((void*)Param2,sizeof(SMALL_RECT)))
      {
        int x1,y1,x2,y2;
        Dlg->GetPosition(x1,y1,x2,y2);
        ((SMALL_RECT*)Param2)->Left=x1;
        ((SMALL_RECT*)Param2)->Top=y1;
        ((SMALL_RECT*)Param2)->Right=x2;
        ((SMALL_RECT*)Param2)->Bottom=y2;
        return TRUE;
      }
      return FALSE;
    }
    /* SVS $ */

    /*****************************************************************/
    /* $ 23.06.2001 KM */
    case DM_GETDROPDOWNOPENED: // Param1=0; Param2=0
    {
      return Dlg->GetDropDownOpened();
    }

    /*****************************************************************/
    case DM_KILLSAVESCREEN:
    {
      if (Dlg->SaveScr) Dlg->SaveScr->Discard();
      if (Dlg->ShadowSaveScr) Dlg->ShadowSaveScr->Discard();
      return TRUE;
    }

    /*****************************************************************/
    /*
      Msg=DM_ALLKEYMODE
      Param1 = -1 - �������� ���������
             =  0 - ���������
             =  1 - ��������
      Ret = ���������
    */
    case DM_ALLKEYMODE:
    {
      if(Param1 == -1)
        return IsProcessAssignMacroKey;
      BOOL OldIsProcessAssignMacroKey=IsProcessAssignMacroKey;
      IsProcessAssignMacroKey=Param1;
      return OldIsProcessAssignMacroKey;
    }

    /*****************************************************************/
    case DM_SETMOUSEEVENTNOTIFY: // Param1 = 1 on, 0 off, -1 - get
    {
      int State=Dlg->DialogMode.Check(DMODE_MOUSEEVENT)?TRUE:FALSE;
      if(Param1 != -1)
      {
        if(!Param1)
          Dlg->DialogMode.Clear(DMODE_MOUSEEVENT);
        else
          Dlg->DialogMode.Set(DMODE_MOUSEEVENT);
      }
      return State;
    }

    /*****************************************************************/
    case DN_RESIZECONSOLE:
    {
      return Dlg->CallDlgProc(Msg,Param1,Param2);
    }
  }

  /*****************************************************************/
  if(Msg >= DM_USER)
  {
    return Dlg->CallDlgProc(Msg,Param1,Param2);
  }

  /*****************************************************************/
  struct DialogItem *CurItem=NULL;
  int Type=0;
  char *Ptr=NULL;
  int Len;
  struct FarDialogItem PluginDialogItem;
  // �������������� ��������...
  /* $ 09.12.2001 DJ
     ��� DM_USER ��������� _��_����_!
  */
  if((Param1 < 0 || Param1 >= Dlg->ItemCount) || !Dlg->Item)
    return 0;
  /* DJ $ */

//  CurItem=&Dlg->Item[Param1];
  CurItem=Dlg->Item+Param1;
  Type=CurItem->Type;
  Ptr=CurItem->Data;

  switch(Msg)
  {
    /*****************************************************************/
    case DM_LISTSORT: // Param1=ID Param=Direct {0|1}
    case DM_LISTADD: // Param1=ID Param2=FarList: ItemsNumber=Count, Items=Src
    case DM_LISTADDSTR: // Param1=ID Param2=String
    case DM_LISTDELETE: // Param1=ID Param2=FarListDelete: StartIndex=BeginIndex, Count=���������� (<=0 - ���!)
    case DM_LISTGETITEM: // Param1=ID Param2=FarListGetItem: ItemsNumber=Index, Items=Dest
    case DM_LISTSET: // Param1=ID Param2=FarList: ItemsNumber=Count, Items=Src
    case DM_LISTGETCURPOS: // Param1=ID Param2=FarListPos
    case DM_LISTSETCURPOS: // Param1=ID Param2=FarListPos Ret: RealPos
    case DM_LISTUPDATE: // Param1=ID Param2=FarList: ItemsNumber=Index, Items=Src
    case DM_LISTINFO:// Param1=ID Param2=FarListInfo
    case DM_LISTFINDSTRING: // Param1=ID Param2=FarListFind
    case DM_LISTINSERT: // Param1=ID Param2=FarListInsert
    case DM_LISTGETDATA: // Param1=ID Param2=Index
    case DM_LISTSETDATA: // Param1=ID Param2=struct FarListItemData
    case DM_LISTSETTITLES: // Param1=ID Param2=struct FarListTitles: TitleLen=strlen(Title), BottomLen=strlen(Bottom)
    case DM_LISTGETTITLES: // Param1=ID Param2=struct FarListTitles: TitleLen=strlen(Title), BottomLen=strlen(Bottom)
    case DM_LISTGETDATASIZE: // Param1=ID Param2=Index
    case DM_LISTSETMOUSEREACTION: // Param1=ID Param2=FARLISTMOUSEREACTIONTYPE Ret=OldSets
    case DM_SETCOMBOBOXEVENT: // Param1=ID Param2=FARCOMBOBOXEVENTTYPE Ret=OldSets
    case DM_GETCOMBOBOXEVENT: // Param1=ID Param2=0 Ret=Sets
    {
      if(Type==DI_LISTBOX || Type==DI_COMBOBOX)
      {
        VMenu *ListBox=CurItem->ListPtr;
        if(ListBox)
        {
          int Ret=TRUE;
          switch(Msg)
          {
            case DM_LISTINFO:// Param1=ID Param2=FarListInfo
            {
              return ListBox->GetVMenuInfo((struct FarListInfo*)Param2);
            }

            case DM_LISTSORT: // Param1=ID Param=Direct {0|1}
            {
              ListBox->SortItems((int)Param2);
              /* $ 23.02.2002 DJ
                 ������������� ������� �����, ����� �� ���� ���� ���������� ���������
              */
              ListBox->AdjustSelectPos();
              /* DJ $ */
              break;
            }

            case DM_LISTFINDSTRING: // Param1=ID Param2=FarListFind
            {
              return ListBox->FindItem(((struct FarListFind *)Param2)->StartIndex,
                                       ((struct FarListFind *)Param2)->Pattern,
                                       ((struct FarListFind *)Param2)->Flags);
            }

            case DM_LISTADDSTR: // Param1=ID Param2=String
            {
              Ret=ListBox->AddItem((char*)Param2);
              /* $ 23.02.2002 DJ
                 � ����� ��� ������ ������ �������, �� ������� ����� ��������� ������?
              */
              ListBox->AdjustSelectPos();
              /* DJ $ */
              break;
            }

            case DM_LISTADD: // Param1=ID Param2=FarList: ItemsNumber=Count, Items=Src
            {
              struct FarList *ListItems=(struct FarList *)Param2;
              if(!ListItems)
                return FALSE;
              Ret=ListBox->AddItem(ListItems);
              /* $ 21.02.2002 DJ
                 � ����� ����� ����� ��������?
              */
              ListBox->AdjustSelectPos();
              /* DJ $ */
              break;
            }

            case DM_LISTDELETE: // Param1=ID Param2=FarListDelete: StartIndex=BeginIndex, Count=���������� (<=0 - ���!)
            {
              int Count;
              struct FarListDelete *ListItems=(struct FarListDelete *)Param2;
              if(!ListItems || (Count=ListItems->Count) <= 0)
                ListBox->DeleteItems();
              else
                ListBox->DeleteItem(ListItems->StartIndex,Count);
              break;
            }

            case DM_LISTINSERT: // Param1=ID Param2=FarListInsert
            {
              if((Ret=ListBox->InsertItem((struct FarListInsert *)Param2)) == -1)
                return -1;
              /* $ 23.02.2002 DJ
                 � ����� �������� ����� � LIF_SELECTED?
              */
              ListBox->AdjustSelectPos();
              /* DJ $ */
              break;
            }

            case DM_LISTUPDATE: // Param1=ID Param2=FarListUpdate: Index=Index, Items=Src
            {
              if(Param2 && ListBox->UpdateItem((struct FarListUpdate *)Param2))
                break;
              return FALSE;
            }

            case DM_LISTGETITEM: // Param1=ID Param2=FarListGetItem: ItemsNumber=Index, Items=Dest
            {
              struct FarListGetItem *ListItems=(struct FarListGetItem *)Param2;
              if(!ListItems)
                return FALSE;
              struct MenuItem *ListMenuItem;
              if((ListMenuItem=ListBox->GetItemPtr(ListItems->ItemIndex)) != NULL)
              {
                //ListItems->ItemIndex=1;
                struct FarListItem *Item=&ListItems->Item;
                memset(Item,0,sizeof(struct FarListItem));
                Item->Flags=ListMenuItem->Flags;
                xstrncpy(Item->Text,ListMenuItem->Name,sizeof(Item->Text)-1);
                /*
                if(ListMenuItem->UserDataSize <= sizeof(DWORD)) //???
                   Item->UserData=ListMenuItem->UserData;
                */
                return TRUE;
              }
              return FALSE;
            }

            case DM_LISTGETDATA: // Param1=ID Param2=Index
            {
              if(Param2 < ListBox->GetItemCount())
                return (LONG_PTR)ListBox->GetUserData(NULL,0,(int)Param2);
              return 0;
            }

            case DM_LISTGETDATASIZE: // Param1=ID Param2=Index
            {
              if(Param2 < ListBox->GetItemCount())
                return ListBox->GetUserDataSize((int)Param2);
              return 0;
            }

            case DM_LISTSETDATA: // Param1=ID Param2=struct FarListItemData
            {
              struct FarListItemData *ListItems=(struct FarListItemData *)Param2;
              if(ListItems &&
                 ListItems->Index < ListBox->GetItemCount())
              {
                Ret=ListBox->SetUserData(ListItems->Data,
                                            ListItems->DataSize,
                                            ListItems->Index);
                if(!Ret && ListBox->GetUserData(NULL,0,ListItems->Index))
                  Ret=sizeof(DWORD);
                return Ret;
              }
              return 0;
            }

            /* $ 02.12.2001 KM
               + ��������� ��� ���������� � ������ �����, � ���������
                 ��� ������������, �.�. "������" ���������
            */
            case DM_LISTSET: // Param1=ID Param2=FarList: ItemsNumber=Count, Items=Src
            {
              struct FarList *ListItems=(struct FarList *)Param2;
              if(!ListItems)
                return FALSE;
              ListBox->DeleteItems();
              Ret=ListBox->AddItem(ListItems);
              /* $ 21.02.2002 DJ
                 � ����� ����� ����� ��������?
              */
              ListBox->AdjustSelectPos();
              /* DJ $ */
              break;
            }
            /* KM $ */
            //case DM_LISTINS: // Param1=ID Param2=FarList: ItemsNumber=Index, Items=Dest

            case DM_LISTSETTITLES: // Param1=ID Param2=struct FarListTitles
            {
              struct FarListTitles *ListTitle=(struct FarListTitles *)Param2;
              ListBox->SetTitle((!ListTitle)?NULL:ListTitle->Title);
              ListBox->SetBottomTitle((!ListTitle)?NULL:ListTitle->Bottom);
              /* $ 23.02.2002 DJ
                 � ������������?
              */
              break;   //return TRUE;
              /* DJ $ */
            }

            case DM_LISTGETTITLES: // Param1=ID Param2=struct FarListTitles
            {
              struct FarListTitles *ListTitle=(struct FarListTitles *)Param2;
              if(ListTitle)
              {
                /* $ 23.02.2002 DJ
                   _������_ �� || ���������� ��� ���������, ��� �� ������� ������
                   ������ �����������, ���������� �� ���������� �������!
                */
                BOOL haveTitle = (ListBox->GetTitle(ListTitle->Title,ListTitle->TitleLen) != NULL);
                BOOL haveBottom = (ListBox->GetBottomTitle(ListTitle->Bottom,ListTitle->BottomLen) != NULL);
                if (haveTitle || haveBottom)
                  return TRUE;
                /* DJ $ */
              }
              return FALSE;
            }

            case DM_LISTGETCURPOS: // Param1=ID Param2=FarListPos
            {
              if (Param2)
                return ListBox->GetSelectPos((struct FarListPos *)Param2);
              else
                return ListBox->GetSelectPos();
            }

            case DM_LISTSETCURPOS: // Param1=ID Param2=FarListPos Ret: RealPos
            {
              /* 26.06.2001 KM ������� ����� ���������� ������� �� ���� ��������� */
              int CurListPos=ListBox->GetSelectPos();
              Ret=ListBox->SetSelectPos((struct FarListPos *)Param2);
              if(Ret!=CurListPos)
                if(!Dlg->CallDlgProc(DN_LISTCHANGE,Param1,Ret))
                  Ret=ListBox->SetSelectPos(CurListPos,1);
              /* KM $ */
              break; // �.�. ����� ������������!
            }

            case DM_LISTSETMOUSEREACTION: // Param1=ID Param2=FARLISTMOUSEREACTIONTYPE Ret=OldSets
            {
              int OldSets=CurItem->IFlags.Flags;
              if(Param2 == LMRT_ONLYFOCUS)
              {
                CurItem->IFlags.Clear(DLGIIF_LISTREACTIONNOFOCUS);
                CurItem->IFlags.Set(DLGIIF_LISTREACTIONFOCUS);
              }
              else if(Param2 == LMRT_NEVER)
              {
                CurItem->IFlags.Clear(DLGIIF_LISTREACTIONNOFOCUS|DLGIIF_LISTREACTIONFOCUS);
                //ListBox->ClearFlags(VMENU_MOUSEREACTION);
              }
              else
              {
                CurItem->IFlags.Set(DLGIIF_LISTREACTIONNOFOCUS|DLGIIF_LISTREACTIONFOCUS);
                //ListBox->SetFlags(VMENU_MOUSEREACTION);
              }

              if((OldSets&(DLGIIF_LISTREACTIONNOFOCUS|DLGIIF_LISTREACTIONFOCUS)) == (DLGIIF_LISTREACTIONNOFOCUS|DLGIIF_LISTREACTIONFOCUS))
                OldSets=LMRT_ALWAYS;
              else if((OldSets&(DLGIIF_LISTREACTIONNOFOCUS|DLGIIF_LISTREACTIONFOCUS)) == 0)
                OldSets=LMRT_NEVER;
              else
                OldSets=LMRT_ONLYFOCUS;
              return OldSets;
            }

            case DM_GETCOMBOBOXEVENT: // Param1=ID Param2=0 Ret=Sets
            {
              return (CurItem->IFlags.Check(DLGIIF_COMBOBOXEVENTKEY)?CBET_KEY:0)|(CurItem->IFlags.Check(DLGIIF_COMBOBOXEVENTMOUSE)?CBET_MOUSE:0);
            }

            case DM_SETCOMBOBOXEVENT: // Param1=ID Param2=FARCOMBOBOXEVENTTYPE Ret=OldSets
            {
              int OldSets=CurItem->IFlags.Flags;
              CurItem->IFlags.Clear(DLGIIF_COMBOBOXEVENTKEY|DLGIIF_COMBOBOXEVENTMOUSE);
              if(Param2&CBET_KEY)
                CurItem->IFlags.Set(DLGIIF_COMBOBOXEVENTKEY);
              if(Param2&CBET_MOUSE)
                CurItem->IFlags.Set(DLGIIF_COMBOBOXEVENTMOUSE);
              return OldSets;
            }

          }
          // ��������� ��� DI_COMBOBOX - ����� ��� � DlgEdit ����� ��������� ���������
          if(!CurItem->IFlags.Check(DLGIIF_COMBOBOXNOREDRAWEDIT) && Type==DI_COMBOBOX && CurItem->ObjPtr)
          {
            struct MenuItem *ListMenuItem;
            if((ListMenuItem=ListBox->GetItemPtr(ListBox->GetSelectPos())) != NULL)
            {
              if(CurItem->Flags & (DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND))
                ((DlgEdit *)(CurItem->ObjPtr))->SetHiString(ListMenuItem->Name);
              else
                ((DlgEdit *)(CurItem->ObjPtr))->SetString(ListMenuItem->Name);

              ((DlgEdit *)(CurItem->ObjPtr))->Select(-1,-1); // ������� ���������
            }
          }

          if(Dlg->DialogMode.Check(DMODE_SHOW) && ListBox->UpdateRequired())
          {
            Dlg->ShowDialog(Param1);
            ScrBuf.Flush();
          }
          return Ret;
        }
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_SETHISTORY: // Param1 = ID, Param2 = LPSTR HistoryName
    {
      if(Type==DI_EDIT || Type==DI_FIXEDIT)
      {
        if(Param2 && *(char *)Param2)
        {
          CurItem->Flags|=DIF_HISTORY;
          CurItem->History=(char *)Param2;
          if(Type==DI_EDIT && (CurItem->Flags&DIF_USELASTHISTORY))
          {
            /* $ 09.12.2001 DJ
               ������� � ��������� �������
            */
            Dlg->ProcessLastHistory (CurItem, Param1);
      /* DJ $ */
          }
        }
        else
        {
          CurItem->Flags&=~DIF_HISTORY;
          CurItem->History=NULL;
        }
        if(Dlg->DialogMode.Check(DMODE_SHOW))
        {
          Dlg->ShowDialog(Param1);
          ScrBuf.Flush();
        }
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_ADDHISTORY:
    {
      if(Param2 &&
         (Type==DI_EDIT || Type==DI_FIXEDIT) &&
         (CurItem->Flags & DIF_HISTORY))
      {
        return Dlg->AddToEditHistory((char*)Param2,CurItem->History);
      }
      return FALSE;
    }

    /*****************************************************************/
    // $ 23.10.2000 SVS - ��������/���������� ������� � ������� ��������������
    case DM_GETCURSORPOS:
    {
      if(!Param2)
        return FALSE;
      if (IsEdit(Type) && CurItem->ObjPtr)
      {
        ((COORD*)Param2)->X=((DlgEdit *)(CurItem->ObjPtr))->GetCurPos();
        ((COORD*)Param2)->Y=0;
        return TRUE;
      }
      else if(Type == DI_USERCONTROL && CurItem->UCData)
      {
        ((COORD*)Param2)->X=CurItem->UCData->CursorPos.X;
        ((COORD*)Param2)->Y=CurItem->UCData->CursorPos.Y;
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_SETCURSORPOS:
    {
      if (IsEdit(Type) && CurItem->ObjPtr && ((COORD*)Param2)->X >= 0)
      {
        DlgEdit *EditPtr=(DlgEdit *)(CurItem->ObjPtr);
        EditPtr->SetCurPos(((COORD*)Param2)->X);
        //EditPtr->Show();
        Dlg->ShowDialog (Param1);
        return TRUE;
      }
      else if(Type == DI_USERCONTROL && CurItem->UCData)
      {
        // �����, ��� ���������� ��� ����� �������� ������ �������������!
        //  � ���������� � 0,0
        COORD Coord=*(COORD*)Param2;
        Coord.X+=CurItem->X1;
        if(Coord.X > CurItem->X2)
          Coord.X=CurItem->X2;

        Coord.Y+=CurItem->Y1;
        if(Coord.Y > CurItem->Y2)
          Coord.Y=CurItem->Y2;

        // ��������
        CurItem->UCData->CursorPos.X=Coord.X-CurItem->X1;
        CurItem->UCData->CursorPos.Y=Coord.Y-CurItem->Y1;
        // ���������� ���� ����
        if(Dlg->DialogMode.Check(DMODE_SHOW) && Dlg->FocusPos == Param1)
        {
           // ���-�� ���� ���� ������ :-)
           MoveCursor(Coord.X+Dlg->X1,Coord.Y+Dlg->Y1); // ???
           Dlg->ShowDialog(Param1); //???
        }
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_GETEDITPOSITION:
    {
      if(Param2 && !IsBadWritePtr((void*)Param2,sizeof(struct EditorSetPosition)) && IsEdit(Type))
      {
        if(Type == DI_MEMOEDIT)
        {
          //EditorControl(ECTL_GETINFO,(struct EditorSetPosition *)Param2);
          return TRUE;
        }
        else
        {
          struct EditorSetPosition *esp=(struct EditorSetPosition *)Param2;
          DlgEdit *EditPtr=(DlgEdit *)(CurItem->ObjPtr);
          esp->CurLine=0;
          esp->CurPos=EditPtr->GetCurPos();
          esp->CurTabPos=EditPtr->GetTabCurPos();
          esp->TopScreenLine=0;
          esp->LeftPos=EditPtr->GetLeftPos();
          esp->Overtype=EditPtr->GetOvertypeMode();
          return TRUE;
        }
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_SETEDITPOSITION:
    {
      if(Param2 && !IsBadReadPtr((void*)Param2,sizeof(struct EditorSetPosition)) && IsEdit(Type))
      {
        if(Type == DI_MEMOEDIT)
        {
          //EditorControl(ECTL_SETPOSITION,(struct EditorSetPosition *)Param2);
          return TRUE;
        }
        else
        {
          struct EditorSetPosition *esp=(struct EditorSetPosition *)Param2;
          DlgEdit *EditPtr=(DlgEdit *)(CurItem->ObjPtr);
          EditPtr->SetCurPos(esp->CurPos);
          EditPtr->SetTabCurPos(esp->CurTabPos);
          EditPtr->SetLeftPos(esp->LeftPos);
          EditPtr->SetOvertypeMode(esp->Overtype);

          Dlg->ShowDialog(Param1);
          ScrBuf.Flush();
          return TRUE;
        }
      }
      return FALSE;
    }



    /*****************************************************************/
    /* $ 23.10.2000 SVS
       ��������/���������� ������ �������
       Param2=0
       Return MAKELONG(Visible,Size)
    */
    case DM_GETCURSORSIZE:
    {
      if (IsEdit(Type) && CurItem->ObjPtr)
      {
        int Visible,Size;
        ((DlgEdit *)(CurItem->ObjPtr))->GetCursorType(Visible,Size);
        return MAKELONG(Visible,Size);
      }
      else if(Type == DI_USERCONTROL && CurItem->UCData)
      {
        return MAKELONG(CurItem->UCData->CursorVisible,CurItem->UCData->CursorSize);
      }
      return FALSE;
    }

    /*****************************************************************/
    // Param2=MAKELONG(Visible,Size)
    //   Return MAKELONG(OldVisible,OldSize)
    case DM_SETCURSORSIZE:
    {
      int Visible=0,Size=0;
      if (IsEdit(Type) && CurItem->ObjPtr)
      {
        ((DlgEdit *)(CurItem->ObjPtr))->GetCursorType(Visible,Size);
        ((DlgEdit *)(CurItem->ObjPtr))->SetCursorType(LOWORD(Param2),HIWORD(Param2));
      }
      else if(Type == DI_USERCONTROL && CurItem->UCData)
      {
        Visible=CurItem->UCData->CursorVisible;
        Size=CurItem->UCData->CursorSize;

        CurItem->UCData->CursorVisible=LOWORD(Param2);
        CurItem->UCData->CursorSize=HIWORD(Param2);

        int CCX=CurItem->UCData->CursorPos.X;
        int CCY=CurItem->UCData->CursorPos.Y;
        if(Dlg->DialogMode.Check(DMODE_SHOW) &&
           Dlg->FocusPos == Param1 &&
           CCX != -1 && CCY != -1)
          SetCursorType(CurItem->UCData->CursorVisible,CurItem->UCData->CursorSize);
      }
      return MAKELONG(Visible,Size);
    }

    /*****************************************************************/
    case DN_LISTCHANGE:
    {
      return Dlg->CallDlgProc(Msg,Param1,Param2);
    }

    /*****************************************************************/
    case DN_EDITCHANGE:
    {
      Dialog::ConvertItem(CVTITEM_TOPLUGIN,&PluginDialogItem,CurItem,1,TRUE);
      if((I=(int)Dlg->CallDlgProc(DN_EDITCHANGE,Param1,(LONG_PTR)&PluginDialogItem)) == TRUE)
      {
        Dialog::ConvertItem(CVTITEM_FROMPLUGIN,&PluginDialogItem,CurItem,1,TRUE);
        if((Type == DI_LISTBOX || Type == DI_COMBOBOX) && CurItem->ListPtr)
          CurItem->ListPtr->ChangeFlags(VMENU_DISABLED,CurItem->Flags&DIF_DISABLE);
      }
      return I;
    }

    /*****************************************************************/
    case DN_BTNCLICK:
    {
      int Ret=(int)Dlg->CallDlgProc(Msg,Param1,Param2);
      if(Ret && (CurItem->Flags&DIF_AUTOMATION) && CurItem->AutoCount && CurItem->AutoPtr)
      {
        DialogItemAutomation* Auto=CurItem->AutoPtr;
        Param2%=3;
        for(I=0; I < CurItem->AutoCount; ++I, ++Auto)
        {
          DWORD NewFlags=Dlg->Item[Auto->ID].Flags;
          Dlg->Item[Auto->ID].Flags=(NewFlags&(~Auto->Flags[Param2][1]))|Auto->Flags[Param2][0];
          // ����� ��������� � ���������� �� ���������� ������ �� ���������
          // ���������...
        }
      }
      return Ret;
    }

    /*****************************************************************/
    case DM_GETCHECK:
    {
      if(Type==DI_CHECKBOX || Type==DI_RADIOBUTTON)
        return CurItem->Selected;
      return 0;
    }

    /*****************************************************************/
    case DM_SET3STATE:
    {
      if(Type == DI_CHECKBOX)
      {
        int OldState=CurItem->Flags&DIF_3STATE?TRUE:FALSE;
        if(Param2)
          CurItem->Flags|=DIF_3STATE;
        else
          CurItem->Flags&=~DIF_3STATE;
        return OldState;
      }
      return 0;
    }

    /*****************************************************************/
    case DM_SETCHECK:
    {
      if(Type == DI_CHECKBOX)
      {
        int Selected=CurItem->Selected;

        if(Param2 == BSTATE_TOGGLE)
          Param2=++Selected;

        if(CurItem->Flags&DIF_3STATE)
          Param2%=3;
        else
          Param2&=1;
        CurItem->Selected=(int)Param2;

        if(Selected != (int)Param2 && Dlg->DialogMode.Check(DMODE_SHOW))
        {
          // �������������
          if((CurItem->Flags&DIF_AUTOMATION) && CurItem->AutoCount && CurItem->AutoPtr)
          {
            DialogItemAutomation* Auto=CurItem->AutoPtr;
            Param2%=3;
            for(I=0; I < CurItem->AutoCount; ++I, ++Auto)
            {
              DWORD NewFlags=Dlg->Item[Auto->ID].Flags;
              Dlg->Item[Auto->ID].Flags=(NewFlags&(~Auto->Flags[Param2][1]))|Auto->Flags[Param2][0];
              // ����� ��������� � ���������� �� ���������� ������ �� ���������
              // ���������...
            }
            Param1=-1;
          }

          Dlg->ShowDialog(Param1);
          ScrBuf.Flush();
        }
        return Selected;
      }
      else if(Type == DI_RADIOBUTTON)
      {
        Param1=Dlg->ProcessRadioButton(Param1);
        if(Dlg->DialogMode.Check(DMODE_SHOW))
        {
          Dlg->ShowDialog();
          ScrBuf.Flush();
        }
        return Param1;
      }
      return 0;
    }

    /*****************************************************************/
    case DN_DRAWDLGITEM:
    {
      // ����������� ������ ���!
      Dialog::ConvertItem(CVTITEM_TOPLUGIN,&PluginDialogItem,CurItem,1);
      I=(int)Dlg->CallDlgProc(Msg,Param1,(LONG_PTR)&PluginDialogItem);
      Dialog::ConvertItem(CVTITEM_FROMPLUGIN,&PluginDialogItem,CurItem,1);
      if((Type == DI_LISTBOX || Type == DI_COMBOBOX) && CurItem->ListPtr)
        CurItem->ListPtr->ChangeFlags(VMENU_DISABLED,CurItem->Flags&DIF_DISABLE);
      return I;
    }

    /*****************************************************************/
    /* $ 08.09.2000 SVS
      - ���� �������, �� DM_SETFOCUS ����� ��� � ������� :-)
    */
    case DM_SETFOCUS:
    {
      if(!Dialog::IsFocused(Type))
        return FALSE;
      if(Dlg->FocusPos == Param1) // ��� � ��� ����������� ���!
        return TRUE;
      if(Dlg->ChangeFocus2(Dlg->FocusPos,Param1) == Param1)
      {
        Dlg->ShowDialog();
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_GETFOCUS: // �������� ID ������
    {
      return Dlg->FocusPos;
    }

    /*****************************************************************/
    case DM_GETTEXTPTR:
      if(Param2)
      {
        struct FarDialogItemData IData;
        IData.PtrData=(char *)Param2;
        IData.PtrLength=0;
        return Dialog::SendDlgMessage(hDlg,DM_GETTEXT,Param1,(LONG_PTR)&IData);
      }

    /*****************************************************************/
    case DM_GETTEXT:
      if(Param2) // ���� ����� NULL, �� ��� ��� ���� ������ �������� ������
      {
        struct FarDialogItemData *did=(struct FarDialogItemData*)Param2;
        Len=0;
        switch(Type)
        {
          case DI_MEMOEDIT:
            break;
          case DI_COMBOBOX:
          case DI_EDIT:
          case DI_PSWEDIT:
          case DI_FIXEDIT:
            if(!CurItem->ObjPtr)
              break;
            /* $ 04.06.2002 KM
                ����� ����������� � 1024 �����, ��� ���� ������
                ��������� �� ������������� ������.
            */
            Ptr=const_cast <char *>(((DlgEdit *)(CurItem->ObjPtr))->GetStringAddr());
            /* KM $ */

          case DI_TEXT:
          case DI_VTEXT:
          case DI_SINGLEBOX:
          case DI_DOUBLEBOX:
          case DI_CHECKBOX:
          case DI_RADIOBUTTON:
          case DI_BUTTON:

            Len=(int)strlen(Ptr)+1;
            if (!(CurItem->Flags & DIF_NOBRACKETS) && Type == DI_BUTTON)
            {
              Ptr+=2;
              Len-=4;
            }

            if(!did->PtrLength)
              did->PtrLength=Len;
            else if(Len > did->PtrLength)
              Len=did->PtrLength+1; // �������� 1, ����� ������ ������� ����.

            if(Len > 0 && did->PtrData)
            {
              memmove(did->PtrData,Ptr,Len);
              did->PtrData[Len-1]=0;
            }
            break;

          case DI_USERCONTROL:
            did->PtrLength=CurItem->Ptr.PtrLength;
            did->PtrData=(char*)CurItem->Ptr.PtrData;
            break;

          case DI_LISTBOX:
          {
//            if(!CurItem->ListPtr)
//              break;
//            did->PtrLength=CurItem->ListPtr->GetUserData(did->PtrData,did->PtrLength,-1);
            break;
          }

          default:  // �������������, ��� ��������
            did->PtrLength=0;
            break;
        }
        return Len-(!Len?0:1);
      }
      // ����� ��������� �� ������ return, �.�. ����� �������� ������
      // ������������� ����� ������ ���� "case DM_GETTEXTLENGTH"!!!

    /*****************************************************************/
    case DM_GETTEXTLENGTH:
    {
      switch(Type)
      {
        case DI_BUTTON:
          Len=(int)strlen(Ptr)+1;
          if (!(CurItem->Flags & DIF_NOBRACKETS))
            Len-=4;
          break;

        case DI_USERCONTROL:
          Len=CurItem->Ptr.PtrLength;
          break;

        case DI_TEXT:
        case DI_VTEXT:
        case DI_SINGLEBOX:
        case DI_DOUBLEBOX:
        case DI_CHECKBOX:
        case DI_RADIOBUTTON:
          Len=(int)strlen(Ptr)+1;
          break;

        case DI_COMBOBOX:
        case DI_EDIT:
        case DI_PSWEDIT:
        case DI_FIXEDIT:
        case DI_MEMOEDIT:
          if(CurItem->ObjPtr)
          {
            Len=((DlgEdit *)(CurItem->ObjPtr))->GetLength()+1;
            break;
          }

        case DI_LISTBOX:
        {
          Len=0;
          struct MenuItem *ListMenuItem;
          if((ListMenuItem=CurItem->ListPtr->GetItemPtr(-1)) != NULL)
          {
            Len=(int)strlen(ListMenuItem->Name)+1;
          }
          break;
        }

        default:
          Len=0;
          break;
      }
      return Len-(!Len?0:1);
    }

    /*****************************************************************/
    case DM_SETTEXTPTR:
    {
      if(!Param2)
        return 0;

      struct FarDialogItemData IData;
      IData.PtrData=(char *)Param2;
      IData.PtrLength=(int)strlen(IData.PtrData);
      return Dialog::SendDlgMessage(hDlg,DM_SETTEXT,Param1,(LONG_PTR)&IData);
    }

    /*****************************************************************/
    case DM_SETTEXT:
    {
      if(Param2)
      {
        int NeedInit=TRUE;
        struct FarDialogItemData *did=(struct FarDialogItemData*)Param2;
        switch(Type)
        {
          case DI_MEMOEDIT:
            break;

          case DI_COMBOBOX:
          case DI_EDIT:
            if(CurItem->Flags&DIF_VAREDIT)
            {
              Ptr=(char *)did->PtrData;
              Len=did->PtrLength+1; // �������� 1, ����� ������ ������� ����.
              if(Len > CurItem->Ptr.PtrLength)
                Len=CurItem->Ptr.PtrLength;
              break;
            }

          case DI_TEXT:
          case DI_VTEXT:
          case DI_SINGLEBOX:
          case DI_DOUBLEBOX:
          case DI_BUTTON:
          case DI_CHECKBOX:
          case DI_RADIOBUTTON:
          case DI_PSWEDIT:
          case DI_FIXEDIT:
          case DI_LISTBOX: // ������ ������ ������� ����
            if((Len=did->PtrLength) == 0)
            {
              xstrncpy(Ptr,(char *)did->PtrData,511);
              Len=(int)strlen(Ptr)+1;
            }
            else
            {
              if((unsigned)did->PtrLength > 511)
                Len=511;
              if(Len > 0)
                memmove(Ptr,(char *)did->PtrData,Len);
              Ptr[Len]=0;
            }
            break;
          default:
            Len=0;
            break;
        }

        switch(Type)
        {
          case DI_USERCONTROL:
            CurItem->Ptr.PtrLength=did->PtrLength;
            CurItem->Ptr.PtrData=did->PtrData;
            return CurItem->Ptr.PtrLength;

          case DI_TEXT:
          case DI_VTEXT:
          case DI_SINGLEBOX:
          case DI_DOUBLEBOX:
            if(Dlg->DialogMode.Check(DMODE_SHOW))
            {
              SetFarTitle(Dlg->GetDialogTitle());
              Dlg->ShowDialog(Param1);
              ScrBuf.Flush();
            }
            return Len-(!Len?0:1);

          case DI_BUTTON:
          case DI_CHECKBOX:
          case DI_RADIOBUTTON:
            break;

          case DI_MEMOEDIT:
            break;

          case DI_COMBOBOX:
          case DI_EDIT:
          case DI_PSWEDIT:
          case DI_FIXEDIT:
            NeedInit=FALSE;
            if(CurItem->ObjPtr)
            {
              DlgEdit *EditLine=(DlgEdit *)(CurItem->ObjPtr);
              int ReadOnly=EditLine->GetReadOnly();
              EditLine->SetReadOnly(0);
              EditLine->SetString((char *)Ptr);
              EditLine->SetReadOnly(ReadOnly);
              if(Dlg->DialogMode.Check(DMODE_INITOBJECTS)) // �� ������ �����-����, ���� �� ��������������������
                EditLine->SetClearFlag(0);
              EditLine->Select(-1,0); // ������� ���������
              // ...��� ��� ��������� � DlgEdit::SetString()
            }
            break;

          case DI_LISTBOX: // ������ ������ ������� ����
          {
            VMenu *ListBox=CurItem->ListPtr;
            if(ListBox)
            {
              struct FarListUpdate LUpdate;
              LUpdate.Index=ListBox->GetSelectPos();
              struct MenuItem *ListMenuItem=ListBox->GetItemPtr(LUpdate.Index);
              if(ListMenuItem)
              {
                LUpdate.Item.Flags=ListMenuItem->Flags;
                xstrncpy(LUpdate.Item.Text,Ptr,sizeof(LUpdate.Item.Text));
                Dialog::SendDlgMessage(hDlg,DM_LISTUPDATE,Param1,(LONG_PTR)&LUpdate);
              }
              break;
            }
            else
              return 0;
          }

          default:  // �������������, ��� ��������
            return 0;
        }
        if(NeedInit)
          Dlg->InitDialogObjects(Param1); // ������������������ �������� �������
        if(Dlg->DialogMode.Check(DMODE_SHOW)) // ���������� �� �����????!!!!
        {
          Dlg->ShowDialog(Param1);
          ScrBuf.Flush();
        }
        return strlen((char *)Ptr); //???
      }
      return 0;
    }

    /*****************************************************************/
    case DM_SETMAXTEXTLENGTH:
    {
      if((Type==DI_EDIT || Type==DI_PSWEDIT ||
          (Type==DI_COMBOBOX && !(CurItem->Flags & DIF_DROPDOWNLIST))) &&
         CurItem->ObjPtr)
      {
        int MaxLen=((DlgEdit *)(CurItem->ObjPtr))->GetMaxLength();

        if((CurItem->Type==DI_EDIT || CurItem->Type==DI_COMBOBOX) &&
           (CurItem->Flags&DIF_VAREDIT))
          CurItem->Ptr.PtrLength=(int)Param2; //???
        else if(Param2 > 511)
          Param2=511;

        // BugZ#628 - ������������ ����� �������������� ������.
        ((DlgEdit *)(CurItem->ObjPtr))->SetMaxLength((int)Param2);

        //if (DialogMode.Check(DMODE_INITOBJECTS)) //???
        Dlg->InitDialogObjects(Param1); // ������������������ �������� �������
        SetFarTitle(Dlg->GetDialogTitle());
        return MaxLen;
      }
      return 0;
    }

    /*****************************************************************/
    case DM_GETDLGITEM:
    {
      if(Param2 && !IsBadWritePtr((void*)Param2,sizeof(struct FarDialogItem)))
      {
        Dialog::ConvertItem(CVTITEM_TOPLUGIN,(struct FarDialogItem *)Param2,CurItem,1);
        if(Type==DI_LISTBOX || Type==DI_COMBOBOX)
          ((struct FarDialogItem *)Param2)->Param.ListPos=CurItem->ListPtr?CurItem->ListPtr->GetSelectPos():0;
/*
        if(IsEdit(Type))
        {
          ((DlgEdit *)(CurItem->ObjPtr))->GetString(Str,sizeof(Str));
          strcpy((char *)Param2,Str);
        }
        else
          strcpy(((struct FarDialogItem *)Param2)->Data,CurItem->Data);
*/
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    case DM_SETDLGITEM:
    {
      if(Param2 && !IsBadReadPtr((void*)Param2,sizeof(struct FarDialogItem)) &&
         Type == ((struct FarDialogItem *)Param2)->Type) // ���� ������ ������ ���
      {
        Dialog::ConvertItem(CVTITEM_FROMPLUGIN,(struct FarDialogItem *)Param2,CurItem,1);
        CurItem->Type=Type;
        if((Type == DI_LISTBOX || Type == DI_COMBOBOX) && CurItem->ListPtr)
          CurItem->ListPtr->ChangeFlags(VMENU_DISABLED,CurItem->Flags&DIF_DISABLE);
        // ��� �����, �.�. ������ ����� ���� ��������
        Dlg->InitDialogObjects(Param1);
        SetFarTitle(Dlg->GetDialogTitle());
        if(Dlg->DialogMode.Check(DMODE_SHOW))
        {
          Dlg->ShowDialog(Param1);
          ScrBuf.Flush();
        }
        return TRUE;
      }
      return FALSE;
    }

    /*****************************************************************/
    /* $ 03.01.2001 SVS
        + ��������/������ �������
        Param2: -1 - �������� ���������
                 0 - ��������
                 1 - ��������
        Return:  ���������� ���������
    */
    case DM_SHOWITEM:
    {
      DWORD PrevFlags=CurItem->Flags;
      if(Param2 != -1)
      {
         if(Param2)
           CurItem->Flags&=~DIF_HIDDEN;
         else
           CurItem->Flags|=DIF_HIDDEN;
        if(Dlg->DialogMode.Check(DMODE_SHOW))// && (PrevFlags&DIF_HIDDEN) != (CurItem->Flags&DIF_HIDDEN))//!(CurItem->Flags&DIF_HIDDEN))
        {
          if((CurItem->Flags&DIF_HIDDEN) && Dlg->FocusPos == Param1)
          {
            Param2=Dlg->ChangeFocus(Param1,1,TRUE);
            Dlg->ChangeFocus2(Param1,(int)Param2);
          }
          // ���� ���,  ����... ������ 1
          Dlg->ShowDialog(Dlg->GetDropDownOpened()||(CurItem->Flags&DIF_HIDDEN)?-1:Param1);
          ScrBuf.Flush();
        }
      }
      return (PrevFlags&DIF_HIDDEN)?FALSE:TRUE;
    }

    /*****************************************************************/
    case DM_SETDROPDOWNOPENED: // Param1=ID; Param2={TRUE|FALSE}
    {
      if (!Param2) // ��������� ����� �������� ��������� ��� �������
      {
        if (Dlg->GetDropDownOpened())
        {
          Dlg->SetDropDownOpened(FALSE);
          Sleep(10);
        }
        return TRUE;
      }
      /* $ 09.12.2001 DJ
         � DI_PSWEDIT �� ������ �������!
      */
      else if (Param2 && (Type==DI_COMBOBOX || ((Type==DI_EDIT || Type==DI_FIXEDIT)
               && (CurItem->Flags&DIF_HISTORY)))) /* DJ $ */
      {
        // ��������� �������� � Param1 ��������� ��� �������
        if (Dlg->GetDropDownOpened())
        {
          Dlg->SetDropDownOpened(FALSE);
          Sleep(10);
        }

        if (Dialog::SendDlgMessage(hDlg,DM_SETFOCUS,Param1,0))
        {
          Dlg->ProcessOpenComboBox(Type,CurItem,Param1); //?? Param1 ??
          //Dlg->ProcessKey(KEY_CTRLDOWN);
          return TRUE;
        }
        else
          return FALSE;
      }
      return FALSE;
    }
    /* KM $ */

    /*****************************************************************/
    case DM_SETITEMPOSITION: // Param1 = ID; Param2 = SMALL_RECT
    {
      return Dlg->SetItemRect((int)Param1,(SMALL_RECT*)Param2);
    }

    /*****************************************************************/
    /* $ 31.08.2000 SVS
        + ������������/��������� ��������� Enable/Disable ��������
    */
    case DM_ENABLE:
    {
      DWORD PrevFlags=CurItem->Flags;
      if(Param2 != -1)
      {
         if(Param2)
           CurItem->Flags&=~DIF_DISABLE;
         else
           CurItem->Flags|=DIF_DISABLE;
         if((Type == DI_LISTBOX || Type == DI_COMBOBOX) && CurItem->ListPtr)
           CurItem->ListPtr->ChangeFlags(VMENU_DISABLED,CurItem->Flags&DIF_DISABLE);
      }
      if(Dlg->DialogMode.Check(DMODE_SHOW)) //???
      {
        Dlg->ShowDialog(Param1);
        ScrBuf.Flush();
      }
      return (PrevFlags&DIF_DISABLE)?FALSE:TRUE;
    }
    /* SVS $ */

    /*****************************************************************/
    // �������� ������� � ������� ��������
    case DM_GETITEMPOSITION: // Param1=ID, Param2=*SMALL_RECT
      if(Param2 && !IsBadWritePtr((void*)Param2,sizeof(SMALL_RECT)))
      {
        RECT Rect;
        if(Dlg->GetItemRect(Param1,Rect))
        {
          ((SMALL_RECT *)Param2)->Left=(short)Rect.left;
          ((SMALL_RECT *)Param2)->Top=(short)Rect.top;
          ((SMALL_RECT *)Param2)->Right=(short)Rect.right;
          ((SMALL_RECT *)Param2)->Bottom=(short)Rect.bottom;
          return TRUE;
        }
      }
      return FALSE;

    /*****************************************************************/
    case DM_SETITEMDATA:
    {
      LONG_PTR PrewDataDialog=CurItem->UserData;
      CurItem->UserData=Param2;
      return PrewDataDialog;
    }

    /*****************************************************************/
    case DM_GETITEMDATA:
    {
      return CurItem->UserData;
    }

    /*****************************************************************/
    case DM_EDITUNCHANGEDFLAG: // -1 Get, 0 - Skip, 1 - Set; ��������� ����� ���������.
    {
      if(IsEdit(Type))
      {
        DlgEdit *EditLine=(DlgEdit *)(CurItem->ObjPtr);
        int ClearFlag=EditLine->GetClearFlag();
        if(Param2 >= 0)
        {
          EditLine->SetClearFlag((int)Param2);
          EditLine->Select(-1,0); // ������� ���������
          if(Dlg->DialogMode.Check(DMODE_SHOW)) //???
          {
            Dlg->ShowDialog(Param1);
            ScrBuf.Flush();
          }
        }
        return ClearFlag;
      }
      break;
    }

    /*****************************************************************/
    case DM_GETSELECTION: // Msg=DM_GETSELECTION, Param1=ID, Param2=*EditorSelect
    case DM_SETSELECTION: // Msg=DM_SETSELECTION, Param1=ID, Param2=*EditorSelect
    {
      if(IsEdit(Type) && Param2)
      {
        if(Msg == DM_GETSELECTION)
        {
          if(!IsBadWritePtr((void*)Param2,sizeof(struct EditorSelect)))
          {
            struct EditorSelect *EdSel=(struct EditorSelect *)Param2;
            DlgEdit *EditLine=(DlgEdit *)(CurItem->ObjPtr);
            EdSel->BlockStartLine=0;
            EdSel->BlockHeight=1;
            EditLine->GetSelection(EdSel->BlockStartPos,EdSel->BlockWidth);
            if(EdSel->BlockStartPos == -1 && EdSel->BlockWidth==0)
              EdSel->BlockType=BTYPE_NONE;
            else
            {
              EdSel->BlockType=BTYPE_STREAM;
              EdSel->BlockWidth-=EdSel->BlockStartPos;
            }
            return TRUE;
          }
        }
        else
        {
          if(!IsBadReadPtr((void*)Param2,sizeof(struct EditorSelect)))
          {
            struct EditorSelect *EdSel=(struct EditorSelect *)Param2;
            DlgEdit *EditLine=(DlgEdit *)(CurItem->ObjPtr);
            //EdSel->BlockType=BTYPE_STREAM;
            //EdSel->BlockStartLine=0;
            //EdSel->BlockHeight=1;
            if(EdSel->BlockType==BTYPE_NONE)
              EditLine->Select(-1,0);
            else
              EditLine->Select(EdSel->BlockStartPos,EdSel->BlockStartPos+EdSel->BlockWidth);
            if(Dlg->DialogMode.Check(DMODE_SHOW)) //???
            {
              Dlg->ShowDialog(Param1);
              ScrBuf.Flush();
            }
            return TRUE;
          }
        }
      }
      break;
    }
  }

  // ���, ��� ���� �� ������������ - �������� �� ��������� �����������.
  return Dlg->CallDlgProc(Msg,Param1,Param2);
}
/* SVS $ */

void Dialog::SetPosition(int X1,int Y1,int X2,int Y2)
{
  CriticalSectionLock Lock(CS);

  if ( X1 >= 0 )
    RealWidth = X2-X1+1;
  else
    RealWidth = X2;

  if ( Y1 >= 0 )
    RealHeight = Y2-Y1+1;
  else
    RealHeight = Y2;

  ScreenObject::SetPosition (X1, Y1, X2, Y2);
}

void Dialog::SetComboBoxPos()
{
  if(GetDropDownOpened())
  {
    int EditX1,EditY1,EditX2,EditY2;
    ((DlgEdit*)(Item[FocusPos].ObjPtr))->GetPosition(EditX1,EditY1,EditX2,EditY2);
    if(EditX2-EditX1<20)
      EditX2=EditX1+20;
    if(ScrY-EditY1<Min(Opt.Dialogs.CBoxMaxHeight,Item[FocusPos].ListPtr->GetItemCount())+2 && EditY1>ScrY/2)
      Item[FocusPos].ListPtr->SetPosition(EditX1,Max(0,EditY1-1-Min(Opt.Dialogs.CBoxMaxHeight,Item[FocusPos].ListPtr->GetItemCount())-1),EditX2,EditY1-1);
    else
      Item[FocusPos].ListPtr->SetPosition(EditX1,EditY1+1,EditX2,0);
	}
}
