"""Run firmware Host tests from the repository root; requires Python 3.9+ and GCC."""
from pathlib import Path
import subprocess,json,re,sys
p=Path('RTT_elog_DMA_UART_ring_project');out=Path('06_Output/Logs/host_tests');out.mkdir(parents=True,exist_ok=True)
incs=[p/'00_Config',p/'01_APP',p/'01_APP/ui']+[d for b in ['02_Service','03_Platform','04_Impl','05_Vendors'] for d in (p/b).rglob('*') if d.is_dir() and 'lvgl' not in d.parts]
mapSources={
 'app_communication_outbound':['01_APP/app_communication.c'],
 'platform_bsp_gpio':['04_Impl/impl_bsp/impl_platform_bsp_gpio.c'],
 'platform_bsp_spi':['04_Impl/impl_bsp/impl_platform_bsp_spi.c'],
 'platform_bsp_uart':['04_Impl/impl_bsp/impl_platform_bsp_uart.c'],
 'platform_log':['04_Impl/impl_middleware/impl_log/easylogger_port.c'],
 'platform_os':[f.as_posix().removeprefix(p.as_posix()+'/') for f in (p/'04_Impl/impl_os/freertos').glob('*.c')],
 'platform_st7789':['03_Platform/platform_bsp/st7789/platform_st7789.c','03_Platform/platform_bsp/st7789/platform_bsp_st7789.c'],
 'platform_graphics':['03_Platform/platform_graphics/platform_graphics.c','03_Platform/platform_graphics/font/platform_font_ascii_8x16.c'],
 'app_display':['01_APP/app_display.c'],
 'platform_gui':['03_Platform/platform_gui/platform_gui.c'],
}
gpio='03_Platform/platform_mcu/gpio/platform_gpio.c'
spi='03_Platform/platform_mcu/spi/platform_spi.c'
uart='03_Platform/platform_mcu/uart/platform_uart.c'
common=['03_Platform/platform_common/platform_device.c','03_Platform/platform_common/platform_object.c']
mapSources.update({
 'impl_platform_gpio':[gpio],
 'impl_platform_spi':[spi,gpio]+common,
 'impl_platform_uart':[uart]+common,
 'platform_button':['03_Platform/platform_bsp/button/platform_button.c',gpio],
 'platform_led':['03_Platform/platform_bsp/led/platform_led.c',gpio],
 'platform_i2c':['03_Platform/platform_mcu/i2c/platform_i2c.c',gpio],
 'platform_spi':[spi,gpio]+common,
 'platform_uart':[uart]+common,
 'service_uart':['02_Service/service_uart/service_uart.c','02_Service/service_common/ring_buffer.c'],
})
mapSources['platform_st7789'] += [spi,gpio]+common
results=[]
for f in sorted((p/'Tests').glob('*/test_*.c')):
 name=f.stem;suite=f.parent.name;src=[];content=f.read_text(encoding='utf8')
 if name.endswith(('_types','_headers')) or suite in ['project_config','app_ipc_types']:pass
 elif suite in mapSources:src=[p/s for s in mapSources[suite]]
 elif re.search(r'#include\s+"[^"\n]+\.c"',content):pass
 else:
  candidates=[s for b in ['01_APP','02_Service','03_Platform','04_Impl'] for s in (p/b).rglob(suite+'.c')]
  assert len(candidates)==1,(suite,candidates)
  src=candidates
 cmd=['gcc','-std=c99','-Wall','-Wextra','-I',str(f.parent),'-I',str(f.parent/'fakes')]+[x for d in incs for x in ['-I',str(d)]]+[str(f)]+[str(s) for s in src]+['-o',str(out/(name+'.exe'))]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'.compile.log')).write_text(r.stdout+r.stderr,encoding='utf8')
 item={'test':name,'compile_exit':r.returncode,'command':cmd}
 if not r.returncode:
  r=subprocess.run([str((out/(name+'.exe')).resolve())],capture_output=True,text=True,timeout=10);item['run_exit']=r.returncode;(out/(name+'.run.log')).write_text(r.stdout+r.stderr,encoding='utf8')
 else:print(name,r.stderr[-1800:])
 results.append(item)
 print(name,'PASS' if item.get('run_exit')==0 else 'FAIL '+str(item.get('run_exit','compile')))
(out/'summary.json').write_text(json.dumps(results,indent=2,ensure_ascii=False),encoding='utf8')
passed=sum(x.get('run_exit')==0 for x in results);print('Host:',passed,'/',len(results));sys.exit(0 if passed==len(results) else 1)
