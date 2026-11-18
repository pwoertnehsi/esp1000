from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeDialogTest(unittest.TestCase):
    def test_actual_dialog_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            (tmp/'shim.h').write_text('''#ifndef NATIVE_DIALOG_SHIM
#define NATIVE_DIALOG_SHIM
#define PSP_UTILITY_DIALOG_NONE 0
#define PSP_UTILITY_DIALOG_VISIBLE 2
#define PSP_UTILITY_DIALOG_QUIT 3
#define PSP_UTILITY_DIALOG_FINISHED 4
#define PSP_SYSTEMPARAM_LANGUAGE_ENGLISH 1
#define PSP_SYSTEMPARAM_ID_INT_LANGUAGE 1
#define PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP 2
#define PSP_UTILITY_MSGDIALOG_MODE_TEXT 1
#define PSP_UTILITY_MSGDIALOG_OPTION_TEXT 1
typedef struct {unsigned size;int language,buttonSwap,graphicsThread,accessThread,fontThread,soundThread;} common;
typedef struct {common base;int mode,options;char message[512];} pspUtilityMsgDialogParams;
int sceUtilityGetSystemParamInt(int,int *);
int sceUtilityMsgDialogInitStart(pspUtilityMsgDialogParams *);
int sceUtilityMsgDialogGetStatus(void);
void sceUtilityMsgDialogUpdate(int);
int sceUtilityMsgDialogAbort(void);
void sceUtilityMsgDialogShutdownStart(void);
#endif
''')
            for name in ['pspkernel.h','psputility.h','psputility_msgdialog.h','psputility_sysparam.h']:
                (tmp/name).write_text('#include "shim.h"\n')
            exe=tmp/'dialog'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(tmp),str(ROOT/'tests/native_dialog_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=10,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
