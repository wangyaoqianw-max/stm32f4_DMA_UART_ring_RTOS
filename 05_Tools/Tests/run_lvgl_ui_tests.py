"""Build and run page checks against the firmware LVGL sources and memory configuration."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
project = root / 'RTT_elog_DMA_UART_ring_project'
build = root / '06_Output/Build/host_lvgl_ui'
logs = root / '06_Output/Logs/host_tests'
parser = argparse.ArgumentParser()
parser.add_argument('--native', action='store_true')
args = parser.parse_args()
build.mkdir(parents=True, exist_ok=True)
logs.mkdir(parents=True, exist_ok=True)
generated = root / 'ui_project/generated' if args.native else project / '01_APP/ui/generated'
config = (project / '00_Config/lv_conf.h').read_text(encoding='utf8')
config = config.replace('#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>', '#define LV_ASSERT_HANDLER_INCLUDE "host_lvgl_assert.h"')
config = config.replace('#define LV_ASSERT_HANDLER while(1);', '#define LV_ASSERT_HANDLER host_lvgl_assert(__FILE__, __LINE__);')
assert_header = 'void host_lvgl_assert(const char *file, int line);\n'
if not (build / 'host_lvgl_assert.h').exists() or (build / 'host_lvgl_assert.h').read_text(encoding='utf8') != assert_header:
    (build / 'host_lvgl_assert.h').write_text(assert_header, encoding='utf8')
config = config.replace('#define LV_FONT_MONTSERRAT_14 1', '#define LV_FONT_MONTSERRAT_14 0')
config = config.replace('#define LV_FONT_CUSTOM_DECLARE\n', '#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_montserratMedium_14)\n')
config = config.replace('#define LV_FONT_DEFAULT &lv_font_montserrat_14', '#define LV_FONT_DEFAULT &lv_font_montserratMedium_14')
if not (build / 'lv_conf.h').exists() or (build / 'lv_conf.h').read_text(encoding='utf8') != config:
    (build / 'lv_conf.h').write_text(config, encoding='utf8')
xml = ET.parse(project / 'MDK-ARM/RTT_elog_DMA_UART_ring_project.uvprojx')
sources = [(project / 'MDK-ARM' / e.text.replace('\\', '/')).resolve()
           for e in xml.iter('FilePath') if 'lvgl' in e.text.lower() and e.text.endswith('.c')]
def quoted(path):
    return '"' + Path(path).as_posix() + '"'
includes = [build, project / '05_Vendors/lvgl', project / '01_APP', project / '01_APP/ui', generated,
            project / '01_APP/ui/custom']
includes += [d for base in ['02_Service', '03_Platform', '04_Impl'] for d in (project / base).rglob('*') if d.is_dir()]
if args.native:
    includes += [root / 'ui_project/custom']
page_sources = list(generated.rglob('*.c'))
test_source = project / 'Tests/ui_sensor_monitor' / ('native_layout_probe.c' if args.native else 'test_ui_sensor_monitor.c')
if not args.native:
    page_sources += [project / '01_APP/ui/ui_sensor_monitor.c']
cmake_text = '\n'.join([
    'cmake_minimum_required(VERSION 3.16)',
    'project(host_lvgl_ui C)', 'set(CMAKE_C_STANDARD 99)',
    'add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)',
    'include_directories(' + ' '.join(map(quoted, includes)) + ')',
    'add_library(firmware_lvgl STATIC ' + ' '.join(map(quoted, sources)) + ')',
    'add_executable(ui_checks ' + ' '.join(map(quoted, [test_source] + page_sources)) + ')',
    'target_link_libraries(ui_checks PRIVATE firmware_lvgl)',
    'target_compile_options(ui_checks PRIVATE -Wall -Wextra)',
])
if not (build / 'CMakeLists.txt').exists() or (build / 'CMakeLists.txt').read_text(encoding='utf8') != cmake_text:
    (build / 'CMakeLists.txt').write_text(cmake_text, encoding='utf8')
kit = Path('E:/APP/ProgramFile/Guider/GUIGuider/resources/assets')
environment = os.environ.copy()
environment['PATH'] = str(kit / 'mingw64/bin') + os.pathsep + environment['PATH']
cmake = shutil.which('cmake') or str(kit / 'cmake/bin/cmake.exe')
commands = [[cmake, '-S', str(build), '-B', str(build / 'cmake'), '-G', 'MinGW Makefiles'],
            [cmake, '--build', str(build / 'cmake'), '-j', '8'],
            [str(build / 'cmake/ui_checks.exe')]]
result = {'native': args.native, 'lv_mem_size': 24576}
for stage, command in zip(['configure', 'compile', 'run'], commands):
    run = subprocess.run(command, cwd=logs, env=environment, capture_output=True, text=True, timeout=600 if stage == 'compile' else 30)
    (logs / ('lvgl_ui_' + stage + '.log')).write_text(run.stdout + run.stderr, encoding='utf8')
    result[stage + '_exit'] = run.returncode
    if stage == 'run' or run.returncode:
        print((run.stdout + run.stderr)[-3000:])
    if run.returncode:
        break
(logs / 'lvgl_ui_summary.json').write_text(json.dumps(result, indent=2), encoding='utf8')
raise SystemExit(run.returncode)
