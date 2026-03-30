import sys
import os
import json
import torch
import kociemba
import serial
import time

sys.path.append(os.path.join(os.path.dirname(__file__), 'CayleyPy_Files'))
from pilgrim import Pilgrim, Searcher

COM_PORT = 'COM5'
BAUD_RATE = 115200

print("\n[*] Waking up PyTorch AI...")
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

WEIGHTS_PATH = 'CayleyPy_Files/p054-t000_333_e08192.pth'
GENERATOR_PATH = 'CayleyPy_Files/p054.json'
TARGET_PATH = 'CayleyPy_Files/p054-t000.pt'
INFO_PATH = 'CayleyPy_Files/logs/model_p054-t000_333.json'

with open(GENERATOR_PATH, "r") as f:
    data = json.load(f)
    if isinstance(data, dict) and "moves" in data and "move_names" in data:
        all_moves, move_names = data["moves"], data["move_names"]
    else:
        all_moves, move_names = list(data.values())[0], list(data.values())[1]
all_moves_tensor = torch.tensor(all_moves, dtype=torch.int64, device=device)

V0 = torch.load(TARGET_PATH, weights_only=True, map_location=device)
with open(INFO_PATH, "r") as f: info = json.load(f)

model = Pilgrim(num_classes=torch.unique(V0).numel(), state_size=all_moves_tensor.size(1), hd1=info["hd1"], hd2=info["hd2"], nrd=info["nrd"], dropout_rate=info.get("dropout", 0.0))
model.load_state_dict(torch.load(WEIGHTS_PATH, weights_only=False, map_location=device), strict=True)
model.eval()

if device.type == "cuda":
    model.half()
    model.dtype = torch.float16
else:
    model.dtype = torch.float32

model.to(device)
if V0.min() < 0: model.z_add = -V0.min().item()

try:
    model = torch.compile(model)
except Exception: pass

searcher = Searcher(model=model, all_moves=all_moves_tensor, V0=V0, device=device, verbose=0)
print(f"[*] AI Ready on {device} (0.5s solves enabled).")

def apply_moves(state, seq_str):
    curr = state.clone()
    if not seq_str.strip(): return curr
    for m in seq_str.strip().split():
        if m in move_names: curr = curr[all_moves_tensor[move_names.index(m)]]
        elif '2' in m:
            base = m[0]
            curr = curr[all_moves_tensor[move_names.index(base)]]
            curr = curr[all_moves_tensor[move_names.index(base)]]
    return curr

def invert_sequence(seq):
    inv = []
    for m in reversed(seq.split()):
        if "'" in m: inv.append(m[0])
        elif "2" in m: inv.append(m)
        else: inv.append(m + "'")
    return " ".join(inv)

def solve_progressive(state):
    beam_sizes = [256, 1024, 4096, 16384] 
    for b in beam_sizes:
        res = searcher.get_solution(state, B=b, num_steps=200, num_attempts=1)
        if res[0] is not None:
            return " ".join([move_names[i] for i in res[0].tolist()])
    return None

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    print(f"\n[+] Connected to ESP32 on {COM_PORT}.")
    print("[+] Waiting for commands from USB...")
except serial.SerialException:
    print(f"\n[-] ERROR: Could not open {COM_PORT}. Close Arduino Serial Monitor!")
    sys.exit()

while True:
    try:
        if ser.in_waiting > 0:
            raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if raw_line.startswith('{') and raw_line.endswith('}'):
                print(f"\n[ESP32 -> PC] {raw_line}")
                data = json.loads(raw_line)
                
                scramble_seq = data.get('scramble', '')
                cube_str = data.get('cube_string', '')
                
                response_payload = {}
                
                try:
                    if cube_str:
                        kociemba_sol = kociemba.solve(cube_str)
                        scramble_seq = invert_sequence(kociemba_sol)
                        
                    state = apply_moves(V0, scramble_seq)
                    t0 = time.time()
                    solution = solve_progressive(state)
                    t1 = time.time()
                    
                    if solution:
                        print(f"[PC -> ESP32] Found in {t1-t0:.2f}s: {solution}")
                        response_payload = {'solution': solution, 'scramble': scramble_seq}
                    else:
                        response_payload = {'error': 'AI failed to solve.'}
                        
                except Exception as e:
                    response_payload = {'error': f'Math error: {str(e)}'}
                
                out_str = json.dumps(response_payload) + '\n'
                ser.write(out_str.encode('utf-8'))
                
    except KeyboardInterrupt:
        print("\n[*] Shutting down.")
        ser.close()
        break
    except Exception as e:
        print(f"Loop error: {e}")
        time.sleep(1)
