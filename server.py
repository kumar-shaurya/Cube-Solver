import sys
import os
import json
import torch
import kociemba
from flask import Flask, request, jsonify

sys.path.append(os.path.join(os.path.dirname(__file__), 'CayleyPy_Files'))
from pilgrim import Pilgrim, Searcher

app = Flask(__name__)

# ==========================================
# 1. INITIALIZE THE CAYLEYPY MODEL
# ==========================================
print("Loading CayleyPy AI Model...")

WEIGHTS_PATH = 'CayleyPy_Files/p054-t000_333_e08192.pth'
GENERATOR_PATH = 'CayleyPy_Files/p054.json'
TARGET_PATH = 'CayleyPy_Files/p054-t000.pt'
INFO_PATH = 'CayleyPy_Files/logs/model_p054-t000_333.json'

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

with open(GENERATOR_PATH, "r") as f:
    data = json.load(f)
    if isinstance(data, dict) and "moves" in data and "move_names" in data:
        all_moves = data["moves"]
        move_names = data["move_names"]
    else:
        all_moves, move_names = data.values()
all_moves_tensor = torch.tensor(all_moves, dtype=torch.int64, device=device)

V0 = torch.load(TARGET_PATH, weights_only=True, map_location=device)

with open(INFO_PATH, "r") as f:
    info = json.load(f)

num_classes = torch.unique(V0).numel()
state_size = all_moves_tensor.size(1)

model = Pilgrim(
    num_classes=num_classes, state_size=state_size,
    hd1=info["hd1"], hd2=info["hd2"], nrd=info["nrd"],
    dropout_rate=info.get("dropout", 0.0),
)

state_dict = torch.load(WEIGHTS_PATH, weights_only=False, map_location=device)
model.load_state_dict(state_dict, strict=True)
model.eval()

if device.type == "cuda":
    model.half()
    model.dtype = torch.float16
else:
    model.dtype = torch.float32

model.to(device)
if V0.min() < 0:
    model.z_add = -V0.min().item()

searcher = Searcher(model=model, all_moves=all_moves_tensor, V0=V0, device=device, verbose=0)
print("✅ Model loaded!")

# ==========================================
# 2. HELPER FUNCTIONS
# ==========================================
def apply_moves_to_state(start_state, sequence_str):
    current_state = start_state.clone()
    if not sequence_str.strip(): return current_state
        
    for move_str in sequence_str.strip().split():
        if move_str in move_names:
            move_idx = move_names.index(move_str)
            current_state = current_state[all_moves_tensor[move_idx]]
        elif '2' in move_str:
            base_move = move_str[0]
            if base_move in move_names:
                move_idx = move_names.index(base_move)
                current_state = current_state[all_moves_tensor[move_idx]]
                current_state = current_state[all_moves_tensor[move_idx]]
            else:
                raise ValueError(f"Unknown base move: {move_str}")
        else:
            raise ValueError(f"Unknown move: {move_str}")
    return current_state

def invert_sequence(seq):
    """ Reverses a move sequence (e.g., to find the scramble that created a state) """
    moves = seq.split()
    inv = []
    for m in reversed(moves):
        if "'" in m: inv.append(m[0])
        elif "2" in m: inv.append(m)
        else: inv.append(m + "'")
    return " ".join(inv)

# ==========================================
# 3. WEB SERVER ENDPOINTS
# ==========================================
@app.route('/solve', methods=['POST'])
def solve():
    data = request.json
    scramble_sequence = data.get('scramble', '')
    cube_string = data.get('cube_string', '') 
    
    try:
        if cube_string:
            print(f"\n[ESP32] Physical Cube String received.")
            try:
                kociemba_solution = kociemba.solve(cube_string)
                scramble_sequence = invert_sequence(kociemba_solution)
            except Exception:
                return jsonify({'error': 'Invalid colors. The cube is mathematically impossible to solve.'}), 400
                
        print(f"[ESP32] Target Scramble: {scramble_sequence}")
        scrambled_state = apply_moves_to_state(V0, scramble_sequence)
        
        result = searcher.get_solution(scrambled_state, B=4096, num_steps=200, num_attempts=1)
        moves_indices = result[0]
        
        if moves_indices is not None:
            solution_str = " ".join([move_names[i] for i in moves_indices.tolist()])
            print(f"[Model] Solution: {solution_str}")
            # FIX: We now return the scramble sequence back to the ESP32!
            return jsonify({'solution': solution_str, 'scramble': scramble_sequence}), 200
        else:
            return jsonify({'error': 'Model failed to find a solution.'}), 400
            
    except Exception as e:
        print(f"Error: {e}")
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
