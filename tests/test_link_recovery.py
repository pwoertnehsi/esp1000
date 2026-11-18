from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class LinkRecoveryTest(unittest.TestCase):
    def test_link_grace(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'grace'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/link_recovery_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    def test_actual_phy_rebuild(self):
        with tempfile.TemporaryDirectory() as d:
            tmp=Path(d);(tmp/'esp_private').mkdir()
            (tmp/'esp_private/usb_phy.h').write_text('''#include <stddef.h>
typedef int esp_err_t;typedef void *usb_phy_handle_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define USB_PHY_CTRL_OTG 1
#define USB_PHY_TARGET_INT 2
#define USB_OTG_MODE_HOST 3
#define USB_PHY_SPEED_FULL 4
typedef struct {int controller,target,otg_mode,otg_speed;} usb_phy_config_t;
esp_err_t usb_del_phy(usb_phy_handle_t);
esp_err_t usb_new_phy(const usb_phy_config_t *,usb_phy_handle_t *);
''')
            (tmp/'tusb.h').write_text('int tuh_inited(void);\n')
            exe=tmp/'phy'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),str(ROOT/'tests/phy_recovery_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
