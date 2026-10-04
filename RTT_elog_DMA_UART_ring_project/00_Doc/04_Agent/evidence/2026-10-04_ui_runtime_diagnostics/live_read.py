import ctypes, json, time, argparse
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--address',type=lambda x:int(x,0),required=True)
parser.add_argument('--duration',type=int,default=600)
args=parser.parse_args()
dll=ctypes.WinDLL('C:/Program Files/SEGGER/JLink/JLink_x64.dll')
dll.JLINKARM_Open.restype=ctypes.c_char_p
dll.JLINKARM_EMU_SelectByUSBSN.argtypes=[ctypes.c_uint]
dll.JLINKARM_ExecCommand.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_int]
dll.JLINKARM_ReadMem.argtypes=[ctypes.c_uint,ctypes.c_uint,ctypes.c_void_p]
logdir=Path('06_Output/Logs/ui_runtime_diagnostics'); logdir.mkdir(parents=True,exist_ok=True)
fields=['sequence','uptime_ms','clock_hz','lvgl_total','lvgl_free','lvgl_largest','lvgl_peak','display_stack_free','rtos_heap_free','rtos_heap_min','flush_count','flush_max_cycles','flush_total_cycles','pixels','process_count','process_max_cycles','process_total_cycles','flush_errors','reserved18','reserved19']
dll.JLINKARM_EMU_SelectByUSBSN(602713300)
error=dll.JLINKARM_Open()
if error: raise RuntimeError(error)
try:
    response=ctypes.create_string_buffer(1024)
    result=dll.JLINKARM_ExecCommand(b'device = STM32F411CE',response,1024)
    if result < 0: raise RuntimeError(response.value)
    dll.JLINKARM_TIF_Select(1)
    dll.JLINKARM_SetSpeed(4000)
    if dll.JLINKARM_Connect() < 0: raise RuntimeError('SWD connection failed')
    if dll.JLINKARM_IsHalted(): raise RuntimeError('Target unexpectedly halted; no measurement taken')
    print('LIVE_READER_READY (CPU running)',flush=True)
    start=time.monotonic()
    with (logdir/'snapshots.jsonl').open('a',encoding='utf-8',buffering=1) as file:
        while time.monotonic()-start<args.duration and not (logdir/'stop_reader').exists():
            if dll.JLINKARM_IsHalted(): raise RuntimeError('Target halted during measurement')
            data=(ctypes.c_uint32*20)()
            got=dll.JLINKARM_ReadMem(args.address,80,data)
            seq=ctypes.c_uint32()
            dll.JLINKARM_ReadMem(args.address,4,ctypes.byref(seq))
            if time.monotonic()-start < 3: print('READ',got,list(data)[:8],seq.value,flush=True)
            if got>=0 and data[0]==seq.value and not data[0]%2 and data[0]:
                row=dict(zip(fields,data)); row['host_elapsed_s']=round(time.monotonic()-start,3)
                file.write(json.dumps(row)+'\n')
                if int(time.monotonic()-start)%10==0: print(json.dumps(row),flush=True)
            time.sleep(1)
finally:
    dll.JLINKARM_Close()
