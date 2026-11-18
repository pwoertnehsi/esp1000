"""Compile the actual ESP service wake adapter against a notification model."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HostWakeupTest(unittest.TestCase):
    def test_wakeup(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp = Path(directory)
            (tmp / "freertos").mkdir()
            (tmp / "freertos/FreeRTOS.h").write_text(
                "typedef void *TaskHandle_t;\ntypedef int BaseType_t;\n"
                "#define pdTRUE 1\n#define portYIELD_FROM_ISR() test_yield()\n"
                "void test_yield(void);\n")
            (tmp / "freertos/task.h").write_text(
                "TaskHandle_t xTaskGetCurrentTaskHandle(void);\n"
                "void xTaskNotifyGive(TaskHandle_t);\n"
                "void vTaskNotifyGiveFromISR(TaskHandle_t,BaseType_t *);\n"
                "unsigned ulTaskNotifyTake(BaseType_t,unsigned);\n")
            exe = tmp / "host-wakeup-test"
            subprocess.run([
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-I", str(tmp),
                str(ROOT / "tests/host_wakeup_test.c"), "-o", str(exe)
            ], check=True)
            subprocess.run([str(exe)], check=True, timeout=5,
                           env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
