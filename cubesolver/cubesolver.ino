#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ==========================================
// UPDATE THESE WITH YOUR DETAILS
// ==========================================
const char* ssid = "vivo T2x 5G";
const char* password = "244466666";
String pythonServerIP = "10.183.217.15";
// ==========================================

WebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>AI Cube Simulator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, sans-serif; text-align: center; background-color: #1e1e1e; color: #fff; margin: 0; padding: 10px; }
    .container { max-width: 600px; margin: auto; }
    .box { background: #2d2d2d; padding: 15px; border-radius: 8px; margin-bottom: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    button { padding: 10px 14px; margin: 4px; font-size: 15px; font-weight: bold; cursor: pointer; border: none; border-radius: 6px; transition: 0.2s; }
    button:active { transform: scale(0.95); }
    .btn-move { background: #3498db; color: white; width: 50px; }
    .btn-action { background: #2ecc71; color: white; width: 45%; }
    .btn-solve { background: #e67e22; color: white; width: 100%; font-size: 18px; padding: 12px; margin-top:10px; }
    .btn-play { background: #9b59b6; color: white; width: 100%; margin-top: 10px; font-size: 16px; padding: 12px; }
    .btn-toggle { background: #f39c12; color: white; width: 100%; padding: 12px; font-size: 16px; }
    .output-text { color: #f1c40f; font-family: monospace; font-size: 18px; word-wrap: break-word; min-height: 22px; margin: 10px 0;}
    #cubeCanvas { background: #222; border-radius: 8px; margin-top: 10px; border: 1px solid #444; cursor: pointer; }
    #currentMoveDisplay { font-size: 24px; color: #e74c3c; font-weight: bold; height: 30px; }
    .hidden { display: none; }
  </style>
</head>
<body>
  <div class="container">
    <h2>AI Cube Simulator</h2>

    <div class="box">
      <button class="btn-toggle" id="modeToggle" onclick="toggleMode()">Switch to Paint Mode</button>
      <div id="currentMoveDisplay"></div>
      <canvas id="cubeCanvas" width="240" height="180"></canvas>
      <p id="paintHelp" class="hidden" style="color:#aaa; font-size: 14px;">Click the stickers to change their colors.</p>
    </div>

    <div class="box" id="controlsBox">
      <p id="scrambleStr" class="output-text">Ready...</p>
      <div id="moveButtons">
        <button class="btn-move" onclick="userMove('U')">U</button>
        <button class="btn-move" onclick="userMove('D')">D</button>
        <button class="btn-move" onclick="userMove('R')">R</button>
        <button class="btn-move" onclick="userMove('L')">L</button>
        <button class="btn-move" onclick="userMove('F')">F</button>
        <button class="btn-move" onclick="userMove('B')">B</button><br>
        <button class="btn-move" onclick="userMove('U\'')">U'</button>
        <button class="btn-move" onclick="userMove('D\'')">D'</button>
        <button class="btn-move" onclick="userMove('R\'')">R'</button>
        <button class="btn-move" onclick="userMove('L\'')">L'</button>
        <button class="btn-move" onclick="userMove('F\'')">F'</button>
        <button class="btn-move" onclick="userMove('B\'')">B'</button>
      </div>
      <div style="margin-top:10px;">
        <button class="btn-action" onclick="generateScramble()">Random</button>
        <button class="btn-action" onclick="clearAll()" style="background:#7f8c8d;">Clear</button>
      </div>
    </div>

    <div class="box">
      <button class="btn-solve" onclick="solveCube()">Ask AI to Solve</button>
      <p id="solutionStr" class="output-text">Waiting...</p>
      <button class="btn-play" id="playBtn" onclick="playSolution()">&#9654; Simulate Solution</button>
    </div>
  </div>

  <script>
    let currentScramble = [];
    let currentSolution = [];
    let savedPaintedState = []; // ADDED: Snapshot to remember painted colors
    let isPlaying = false;
    let isPaintMode = false;
    
    // Colors: 0=White(U), 1=Red(R), 2=Green(F), 3=Yellow(D), 4=Orange(L), 5=Blue(B)
    const colors = ['#ffffff', '#e74c3c', '#2ecc71', '#f1c40f', '#e67e22', '#3498db'];
    const faceLetters = ['U', 'R', 'F', 'D', 'L', 'B'];
    let state = new Array(54).fill(0);
    
    function resetCube() {
        for (let i = 0; i < 6; i++) {
            for (let j = 0; j < 9; j++) { state[i * 9 + j] = i; }
        }
    }

    function toggleMode() {
        isPaintMode = !isPaintMode;
        let toggleBtn = document.getElementById("modeToggle");
        let controls = document.getElementById("controlsBox");
        let helpText = document.getElementById("paintHelp");
        
        if(isPaintMode) {
            toggleBtn.innerText = "Switch to Move Mode";
            toggleBtn.style.background = "#e74c3c";
            controls.classList.add("hidden");
            helpText.classList.remove("hidden");
            clearAll(); // Start with clean cube for painting
        } else {
            toggleBtn.innerText = "Switch to Paint Mode";
            toggleBtn.style.background = "#f39c12";
            controls.classList.remove("hidden");
            helpText.classList.add("hidden");
            clearAll();
        }
    }

    // --- CANVAS CLICK (PAINTING) LOGIC ---
    document.getElementById('cubeCanvas').addEventListener('mousedown', function(e) {
        if (!isPaintMode || isPlaying) return;
        const canvas = document.getElementById('cubeCanvas');
        let rect = canvas.getBoundingClientRect();
        let clickX = e.clientX - rect.left;
        let clickY = e.clientY - rect.top;

        const sq = 18; const gap = 2; const faceSize = (sq * 3) + (gap * 2);
        const offsets = [ {x: faceSize, y: 0}, {x: faceSize*2, y: faceSize}, {x: faceSize, y: faceSize}, {x: faceSize, y: faceSize*2}, {x: 0, y: faceSize}, {x: faceSize*3, y: faceSize} ];

        for (let f = 0; f < 6; f++) {
            let ox = offsets[f].x + 10;
            let oy = offsets[f].y + 10;
            for (let i = 0; i < 9; i++) {
                let x = ox + ((i % 3) * (sq + gap));
                let y = oy + (Math.floor(i / 3) * (sq + gap));
                
                if (clickX >= x && clickX <= x + sq && clickY >= y && clickY <= y + sq) {
                    // Clicked this sticker! Change color.
                    state[f * 9 + i] = (state[f * 9 + i] + 1) % 6;
                    drawCube();
                    return;
                }
            }
        }
    });

    // --- CUBE ROTATION MATH ---
    function rotateFaceCW(f) {
        let s = f * 9;
        let temp = [state[s], state[s+1], state[s+2], state[s+3], state[s+4], state[s+5], state[s+6], state[s+7], state[s+8]];
        state[s] = temp[6]; state[s+1] = temp[3]; state[s+2] = temp[0];
        state[s+3] = temp[7]; state[s+4] = temp[4]; state[s+5] = temp[1];
        state[s+6] = temp[8]; state[s+7] = temp[5]; state[s+8] = temp[2];
    }
    function cycle4(a, b, c, d) {
        for(let i=0; i<3; i++) {
            let temp = state[d[i]];
            state[d[i]] = state[c[i]];
            state[c[i]] = state[b[i]];
            state[b[i]] = state[a[i]];
            state[a[i]] = temp;
        }
    }
    function moveU() { rotateFaceCW(0); cycle4([45,46,47], [18,19,20], [9,10,11], [36,37,38]); }
    function moveR() { rotateFaceCW(1); cycle4([8,5,2], [51,48,45], [35,32,29], [26,23,20]); }
    function moveF() { rotateFaceCW(2); cycle4([6,7,8], [9,12,15], [29,28,27], [44,41,38]); }
    function moveD() { rotateFaceCW(3); cycle4([24,25,26], [42,43,44], [15,16,17], [51,52,53]); }
    function moveL() { rotateFaceCW(4); cycle4([0,3,6], [18,21,24], [27,30,33], [53,50,47]); }
    function moveB() { rotateFaceCW(5); cycle4([2,1,0], [36,39,42], [33,34,35], [17,14,11]); }

    function applyMove(m) {
        let base = m[0];
        let times = m.includes("'") ? 3 : m.includes("2") ? 2 : 1;
        for(let i=0; i<times; i++) {
            if(base === 'U') moveU(); if(base === 'R') moveR(); if(base === 'F') moveF();
            if(base === 'D') moveD(); if(base === 'L') moveL(); if(base === 'B') moveB();
        }
        drawCube();
    }

    function drawCube() {
        const canvas = document.getElementById('cubeCanvas');
        if(!canvas) return;
        const ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        const sq = 18; const gap = 2; const faceSize = (sq * 3) + (gap * 2);
        
        const offsets = [ {x: faceSize, y: 0}, {x: faceSize*2, y: faceSize}, {x: faceSize, y: faceSize}, {x: faceSize, y: faceSize*2}, {x: 0, y: faceSize}, {x: faceSize*3, y: faceSize} ];

        for (let f = 0; f < 6; f++) {
            let ox = offsets[f].x + 10; let oy = offsets[f].y + 10;
            for (let i = 0; i < 9; i++) {
                let x = ox + ((i % 3) * (sq + gap));
                let y = oy + (Math.floor(i / 3) * (sq + gap));
                
                ctx.fillStyle = colors[state[f * 9 + i]];
                ctx.fillRect(x, y, sq, sq);
                ctx.strokeStyle = "#000";
                ctx.strokeRect(x, y, sq, sq);
            }
        }
    }

    function userMove(m) {
        if(isPlaying) return;
        currentScramble.push(m);
        applyMove(m);
        updateUI();
    }

    function updateUI() {
      document.getElementById("scrambleStr").innerText = currentScramble.length > 0 ? currentScramble.join(" ") : "Ready...";
    }

    function clearAll() {
      if(isPlaying) return;
      currentScramble = [];
      currentSolution = [];
      savedPaintedState = []; // Reset snapshot
      document.getElementById("solutionStr").innerText = "Waiting...";
      document.getElementById("currentMoveDisplay").innerText = "";
      resetCube();
      drawCube();
      updateUI();
    }

    function generateScramble() {
      if(isPlaying) return;
      clearAll();
      const basicMoves = ['U', 'D', 'R', 'L', 'F', 'B'];
      const modifiers = ['', "'", '2'];
      for(let i=0; i<20; i++) {
        let m = basicMoves[Math.floor(Math.random() * basicMoves.length)] + modifiers[Math.floor(Math.random() * modifiers.length)];
        currentScramble.push(m);
      }
      resetCube();
      currentScramble.forEach(applyMove);
      updateUI();
    }

    function solveCube() {
      document.getElementById("solutionStr").innerText = "AI is thinking...";
      
      let payload = {};
      
      if(isPaintMode) {
          // Take a snapshot of the painted cube before we do anything!
          savedPaintedState = [...state]; 
          let cubeString = state.map(val => faceLetters[val]).join('');
          payload = { cube_string: cubeString };
      } else {
          if(currentScramble.length === 0) return alert("Scramble first!");
          payload = { scramble: currentScramble.join(" ") };
      }
      
      fetch('/api/solve', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      .then(res => res.json())
      .then(data => {
        if(data.error) throw new Error(data.error);
        currentSolution = data.solution.split(" ");
        document.getElementById("solutionStr").innerText = data.solution;
      })
      .catch(err => {
        document.getElementById("solutionStr").innerText = err.message;
      });
    }

    async function playSolution() {
        if(currentSolution.length === 0) return alert("Get a solution first!");
        if(isPlaying) return;
        
        isPlaying = true;
        let display = document.getElementById("currentMoveDisplay");
        
        // Restore to the correct starting state
        if(isPaintMode) {
            if(savedPaintedState.length === 54) {
                state = [...savedPaintedState]; // Load painted snapshot
                drawCube();
            }
        } else {
            resetCube();
            currentScramble.forEach(applyMove); // Load scrambled sequence
        }

        // Playback moves
        for(let i=0; i<currentSolution.length; i++) {
            let m = currentSolution[i];
            display.innerText = "Executing: " + m;
            applyMove(m);
            await new Promise(r => setTimeout(r, 600)); 
        }
        
        display.innerText = "Solved!";
        isPlaying = false;
    }

    window.onload = function() {
        resetCube();
        drawCube();
    };
  </script>
</body>
</html>
)rawliteral";

void forwardToPython(String endpoint) {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"error\":\"No body received\"}");
    return;
  }
  String requestBody = server.arg("plain");
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + pythonServerIP + ":5000" + endpoint;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(60000); 
    
    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode > 0) {
      String response = http.getString();
      server.send(httpResponseCode, "application/json", response);
    } else {
      server.send(500, "application/json", "{\"error\":\"Python server timeout or unreachable.\"}");
    }
    http.end();
  } else {
    server.send(503, "application/json", "{\"error\":\"ESP32 WiFi disconnected\"}");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected! Go to: http://" + WiFi.localIP().toString());
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  server.on("/api/solve", HTTP_POST, []() { forwardToPython("/solve"); });
  server.begin();
}

void loop() { server.handleClient(); }