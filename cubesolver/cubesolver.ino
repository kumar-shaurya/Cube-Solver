#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h> 

const char* ap_ssid = "Cube_Solver"; 
const char* ap_password = "password123";  

const byte DNS_PORT = 53; 
DNSServer dnsServer;
WebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>AI 3D Cube Solver</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, sans-serif; text-align: center; background-color: #1e1e1e; color: #fff; margin: 0; padding: 10px; overflow-x: hidden; }
    .container { max-width: 600px; margin: auto; }
    .box { background: #2d2d2d; padding: 15px; border-radius: 8px; margin-bottom: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    button { padding: 10px 14px; margin: 4px; font-size: 15px; font-weight: bold; cursor: pointer; border: none; border-radius: 6px; transition: 0.2s; }
    button:active { transform: scale(0.95); }
    button:disabled { opacity: 0.5; cursor: not-allowed; }
    .btn-move { background: #3498db; color: white; width: 50px; }
    .btn-action { background: #2ecc71; color: white; width: 45%; }
    .btn-solve { background: #e67e22; color: white; width: 100%; font-size: 18px; padding: 12px; margin-top:10px; }
    .btn-toggle { background: #f39c12; color: white; width: 100%; padding: 12px; font-size: 16px; margin-bottom: 10px; }
    .player-controls { display: flex; justify-content: space-between; margin-top: 15px; }
    .btn-step { background: #9b59b6; color: white; flex: 1; margin: 0 5px; font-size: 16px; padding: 12px; }
    .output-text { color: #f1c40f; font-family: monospace; font-size: 18px; word-wrap: break-word; min-height: 22px; margin: 10px 0;}
    #currentMoveDisplay { font-size: 24px; color: #e74c3c; font-weight: bold; height: 30px; margin-bottom: 10px; }
    .hidden { display: none !important; }
    #view3D { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 350px; }
    .scene { width: 150px; height: 150px; perspective: 800px; cursor: grab; margin-top: 30px; }
    .scene:active { cursor: grabbing; }
    .cube-wrapper { width: 100%; height: 100%; position: relative; transform-style: preserve-3d; transform: rotateX(-20deg) rotateY(-45deg); }
    #cube { width: 100%; height: 100%; position: absolute; transform-style: preserve-3d; }
    #pivot { width: 100%; height: 100%; position: absolute; transform-style: preserve-3d; }
    .cubie { position: absolute; width: 50px; height: 50px; left: 50px; top: 50px; transform-style: preserve-3d; }
    .sticker { position: absolute; width: 46px; height: 46px; margin: 2px; border-radius: 4px; box-sizing: border-box; border: 2px solid #000; background: #000; }
    .s-U { transform: rotateX(90deg) translateZ(25px); }
    .s-D { transform: rotateX(-90deg) translateZ(25px); }
    .s-R { transform: rotateY(90deg) translateZ(25px); }
    .s-L { transform: rotateY(-90deg) translateZ(25px); }
    .s-F { transform: translateZ(25px); }
    .s-B { transform: rotateY(180deg) translateZ(25px); }
    #cubeCanvas { background: #222; border-radius: 8px; border: 1px solid #444; cursor: pointer; }
  </style>
</head>
<body>
  <div class="container">
    <h2>AI 3D Cube Solver</h2>
    <div class="box">
      <button class="btn-toggle" id="modeToggle" onclick="toggleMode()">Switch to Paint Mode</button>
      <div id="currentMoveDisplay"></div>
      <div id="view3D">
        <div class="scene" id="scene">
          <div class="cube-wrapper" id="cubeWrapper">
            <div id="cube"></div>
            <div id="pivot"></div>
          </div>
        </div>
        <p style="color:#aaa; font-size: 13px; margin-top: 40px;">Drag the cube to rotate 3D view.</p>
      </div>
      <div id="view2D" class="hidden">
        <canvas id="cubeCanvas" width="240" height="180"></canvas>
        <p style="color:#aaa; font-size: 14px;">Click the stickers to map your physical cube.</p>
      </div>
    </div>
    <div class="box" id="controlsBox">
      <p id="scrambleStr" class="output-text">Ready...</p>
      <div id="moveButtons">
        <button class="btn-move" onclick="userMove('U')">U</button><button class="btn-move" onclick="userMove('D')">D</button><button class="btn-move" onclick="userMove('R')">R</button><button class="btn-move" onclick="userMove('L')">L</button><button class="btn-move" onclick="userMove('F')">F</button><button class="btn-move" onclick="userMove('B')">B</button><br>
        <button class="btn-move" onclick="userMove('U\'')">U'</button><button class="btn-move" onclick="userMove('D\'')">D'</button><button class="btn-move" onclick="userMove('R\'')">R'</button><button class="btn-move" onclick="userMove('L\'')">L'</button><button class="btn-move" onclick="userMove('F\'')">F'</button><button class="btn-move" onclick="userMove('B\'')">B'</button>
      </div>
      <div style="margin-top:10px;">
        <button class="btn-action" onclick="generateScramble()">Random Scramble</button>
        <button class="btn-action" onclick="clearAll()" style="background:#7f8c8d;">Clear All</button>
      </div>
    </div>
    <div class="box">
      <button class="btn-solve" id="solveBtn" onclick="solveCube()">Ask AI to Solve</button>
      <p id="solutionStr" class="output-text">Waiting...</p>
      <div class="player-controls hidden" id="playerBox">
        <button class="btn-step" id="btnPrev" onclick="stepBackward()">&#171; Prev</button>
        <button class="btn-step" id="btnPlay" onclick="togglePlay()">Play Auto</button>
        <button class="btn-step" id="btnNext" onclick="stepForward()">Next &#187;</button>
      </div>
    </div>
  </div>

  <script>
    let currentScramble = []; let solutionMoves = []; let currentStep = 0; let isAnimating = false; let autoPlayInterval = null; let isPaintMode = false; let cubies = [];
    const colors = ['#ffffff', '#e74c3c', '#2ecc71', '#f1c40f', '#e67e22', '#3498db']; const faceLetters = ['U', 'R', 'F', 'D', 'L', 'B']; let paintState = new Array(54).fill(0);

    function init3DCube() {
        const cube = document.getElementById('cube'); cube.innerHTML = ''; cubies = [];
        for(let x=-1; x<=1; x++) {
            for(let y=-1; y<=1; y++) {
                for(let z=-1; z<=1; z++) {
                    let c = document.createElement('div'); c.className = 'cubie'; c.logical = {x, y, z};
                    c.style.transform = `translate3d(${x*50}px, ${y*50}px, ${z*50}px)`;
                    let faces = [ { axis: 'y', val: -1, color: colors[0], class: 's-U' }, { axis: 'x', val: 1,  color: colors[1], class: 's-R' }, { axis: 'z', val: 1,  color: colors[2], class: 's-F' }, { axis: 'y', val: 1,  color: colors[3], class: 's-D' }, { axis: 'x', val: -1, color: colors[4], class: 's-L' }, { axis: 'z', val: -1, color: colors[5], class: 's-B' } ];
                    faces.forEach(f => {
                        let sticker = document.createElement('div'); sticker.className = 'sticker ' + f.class;
                        if(x === f.val && f.axis === 'x') sticker.style.background = f.color;
                        else if(y === f.val && f.axis === 'y') sticker.style.background = f.color;
                        else if(z === f.val && f.axis === 'z') sticker.style.background = f.color;
                        c.appendChild(sticker);
                    });
                    cube.appendChild(c); cubies.push(c);
                }
            }
        }
    }

    function animateMove(move, duration) {
        return new Promise(resolve => {
            isAnimating = true;
            let base = move[0]; let modifier = move.length > 1 ? move[1] : ''; let dir = modifier === "'" ? -1 : (modifier === "2" ? 2 : 1);
            let axis, value, angle;
            if(base==='U') { axis='y'; value=-1; angle= -90 * dir; } if(base==='D') { axis='y'; value= 1; angle=  90 * dir; }
            if(base==='R') { axis='x'; value= 1; angle=  90 * dir; } if(base==='L') { axis='x'; value=-1; angle= -90 * dir; }
            if(base==='F') { axis='z'; value= 1; angle=  90 * dir; } if(base==='B') { axis='z'; value=-1; angle= -90 * dir; }

            let pivot = document.getElementById('pivot'); let moving = cubies.filter(c => c.logical[axis] === value);
            moving.forEach(c => pivot.appendChild(c));

            if(duration > 0) pivot.style.transition = `transform ${duration}ms ease-in-out`;
            else pivot.style.transition = 'none';
            pivot.style.transform = `rotate${axis.toUpperCase()}(${angle}deg)`;

            setTimeout(() => {
                pivot.style.transition = 'none'; pivot.style.transform = 'none';
                moving.forEach(c => {
                    let rotStr = `rotate${axis.toUpperCase()}(${angle}deg) `;
                    c.style.transform = rotStr + c.style.transform.replace('none', '');
                    document.getElementById('cube').appendChild(c);
                    let x = c.logical.x, y = c.logical.y, z = c.logical.z;
                    let rad = angle * Math.PI / 180; let cos = Math.round(Math.cos(rad)), sin = Math.round(Math.sin(rad));
                    if(axis === 'x') { c.logical.y = y*cos - z*sin; c.logical.z = y*sin + z*cos; } 
                    else if(axis === 'y') { c.logical.x = x*cos + z*sin; c.logical.z = -x*sin + z*cos; } 
                    else if(axis === 'z') { c.logical.x = x*cos - y*sin; c.logical.y = x*sin + y*cos; }
                });
                isAnimating = false; resolve();
            }, duration + 20); 
        });
    }

    let isDragging = false, prevX = 0, prevY = 0, rotX = -20, rotY = -45; const scene = document.getElementById('scene'); const cubeWrapper = document.getElementById('cubeWrapper');
    function startDrag(e) { isDragging = true; prevX = e.touches ? e.touches[0].clientX : e.clientX; prevY = e.touches ? e.touches[0].clientY : e.clientY; }
    function moveDrag(e) { if(!isDragging) return; let x = e.touches ? e.touches[0].clientX : e.clientX; let y = e.touches ? e.touches[0].clientY : e.clientY; rotY += (x - prevX) * 0.5; rotX -= (y - prevY) * 0.5; cubeWrapper.style.transform = `rotateX(${rotX}deg) rotateY(${rotY}deg)`; prevX = x; prevY = y; }
    function endDrag() { isDragging = false; }
    scene.addEventListener('mousedown', startDrag); window.addEventListener('mousemove', moveDrag); window.addEventListener('mouseup', endDrag);
    scene.addEventListener('touchstart', startDrag, {passive: true}); window.addEventListener('touchmove', moveDrag, {passive: true}); window.addEventListener('touchend', endDrag);

    async function stepForward() { if(isAnimating || currentStep >= solutionMoves.length) return; document.getElementById('currentMoveDisplay').innerText = "Executing: " + solutionMoves[currentStep]; await animateMove(solutionMoves[currentStep], 300); currentStep++; updatePlayerUI(); }
    async function stepBackward() { if(isAnimating || currentStep <= 0) return; currentStep--; let move = solutionMoves[currentStep]; document.getElementById('currentMoveDisplay').innerText = "Reversing: " + move; let inv = move.includes("'") ? move[0] : (move.includes("2") ? move : move + "'"); await animateMove(inv, 300); updatePlayerUI(); }
    async function togglePlay() { let btn = document.getElementById('btnPlay'); if(autoPlayInterval) { clearInterval(autoPlayInterval); autoPlayInterval = null; btn.innerText = "Play Auto"; } else { btn.innerText = "Pause"; autoPlayInterval = setInterval(async () => { if(currentStep < solutionMoves.length) { await stepForward(); } else { clearInterval(autoPlayInterval); autoPlayInterval = null; btn.innerText = "Play Auto"; } }, 600); } }
    function updatePlayerUI() { document.getElementById('btnPrev').disabled = (currentStep === 0); document.getElementById('btnNext').disabled = (currentStep === solutionMoves.length); if(currentStep === solutionMoves.length && solutionMoves.length > 0) document.getElementById('currentMoveDisplay').innerText = "Cube Solved! 🎉"; }

    function toggleMode() { if(isAnimating) return; isPaintMode = !isPaintMode; let toggleBtn = document.getElementById("modeToggle"); if(isPaintMode) { toggleBtn.innerText = "Switch to 3D Move Mode"; toggleBtn.style.background = "#e74c3c"; document.getElementById("view3D").classList.add("hidden"); document.getElementById("controlsBox").classList.add("hidden"); document.getElementById("view2D").classList.remove("hidden"); } else { toggleBtn.innerText = "Switch to Paint Mode"; toggleBtn.style.background = "#f39c12"; document.getElementById("view2D").classList.add("hidden"); document.getElementById("view3D").classList.remove("hidden"); document.getElementById("controlsBox").classList.remove("hidden"); } clearAll(); }
    async function userMove(m) { if(isAnimating) return; currentScramble.push(m); document.getElementById("scrambleStr").innerText = currentScramble.join(" "); await animateMove(m, 300); }
    async function generateScramble() { if(isAnimating) return; clearAll(); const basicMoves = ['U', 'D', 'R', 'L', 'F', 'B']; const modifiers = ['', "'", '2']; for(let i=0; i<20; i++) { let m = basicMoves[Math.floor(Math.random() * basicMoves.length)] + modifiers[Math.floor(Math.random() * modifiers.length)]; currentScramble.push(m); await animateMove(m, 0); } document.getElementById("scrambleStr").innerText = currentScramble.join(" "); }
    function clearAll() { if(isAnimating) return; if(autoPlayInterval) { clearInterval(autoPlayInterval); autoPlayInterval = null; } currentScramble = []; solutionMoves = []; currentStep = 0; document.getElementById("solutionStr").innerText = "Waiting..."; document.getElementById("scrambleStr").innerText = "Ready..."; document.getElementById("currentMoveDisplay").innerText = ""; document.getElementById("playerBox").classList.add("hidden"); for(let i=0; i<54; i++) paintState[i] = Math.floor(i/9); init3DCube(); drawPaintCanvas(); }

    function solveCube() {
      if(isAnimating) return; document.getElementById("solutionStr").innerText = "AI is thinking..."; document.getElementById("solveBtn").disabled = true; let payload = {};
      if(isPaintMode) { let cubeString = paintState.map(val => faceLetters[val]).join(''); payload = { cube_string: cubeString }; } else { if(currentScramble.length === 0) { document.getElementById("solveBtn").disabled = false; return alert("Scramble first!"); } payload = { scramble: currentScramble.join(" ") }; }
      
      fetch('/api/solve', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) })
      .then(res => res.json())
      .then(async data => {
        document.getElementById("solveBtn").disabled = false; if(data.error) throw new Error(data.error);
        solutionMoves = data.solution.split(" "); currentStep = 0; document.getElementById("solutionStr").innerText = data.solution;
        if(isPaintMode) { document.getElementById("view2D").classList.add("hidden"); document.getElementById("view3D").classList.remove("hidden"); init3DCube(); if(data.scramble) { for(let m of data.scramble.split(" ")) await animateMove(m, 0); } }
        document.getElementById("playerBox").classList.remove("hidden"); updatePlayerUI();
      })
      .catch(err => { document.getElementById("solveBtn").disabled = false; document.getElementById("solutionStr").innerText = err.message; });
    }

    document.getElementById('cubeCanvas').addEventListener('mousedown', function(e) { if (!isPaintMode || isAnimating) return; let rect = this.getBoundingClientRect(); let clickX = e.clientX - rect.left; let clickY = e.clientY - rect.top; const sq = 18; const gap = 2; const faceSize = (sq * 3) + (gap * 2); const offsets = [ {x: faceSize, y: 0}, {x: faceSize*2, y: faceSize}, {x: faceSize, y: faceSize}, {x: faceSize, y: faceSize*2}, {x: 0, y: faceSize}, {x: faceSize*3, y: faceSize} ]; for (let f = 0; f < 6; f++) { let ox = offsets[f].x + 10; let oy = offsets[f].y + 10; for (let i = 0; i < 9; i++) { let x = ox + ((i % 3) * (sq + gap)); let y = oy + (Math.floor(i / 3) * (sq + gap)); if (clickX >= x && clickX <= x + sq && clickY >= y && clickY <= y + sq) { paintState[f * 9 + i] = (paintState[f * 9 + i] + 1) % 6; drawPaintCanvas(); return; } } } });
    function drawPaintCanvas() { const canvas = document.getElementById('cubeCanvas'); if(!canvas) return; const ctx = canvas.getContext('2d'); ctx.clearRect(0, 0, canvas.width, canvas.height); const sq = 18; const gap = 2; const faceSize = (sq * 3) + (gap * 2); const offsets = [ {x: faceSize, y: 0}, {x: faceSize*2, y: faceSize}, {x: faceSize, y: faceSize}, {x: faceSize, y: faceSize*2}, {x: 0, y: faceSize}, {x: faceSize*3, y: faceSize} ]; for (let f = 0; f < 6; f++) { let ox = offsets[f].x + 10; let oy = offsets[f].y + 10; for (let i = 0; i < 9; i++) { let x = ox + ((i % 3) * (sq + gap)); let y = oy + (Math.floor(i / 3) * (sq + gap)); ctx.fillStyle = colors[paintState[f * 9 + i]]; ctx.fillRect(x, y, sq, sq); ctx.strokeStyle = "#000"; ctx.strokeRect(x, y, sq, sq); } } }
    window.onload = function() { clearAll(); };
  </script>
</body>
</html>
)rawliteral";

void forwardToPython() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"error\":\"No body received\"}");
    return;
  }
  
  String requestBody = server.arg("plain");
  while(Serial.available()) { Serial.read(); }
  
  Serial.println(requestBody); 

  unsigned long startTime = millis();
  String pythonResponse = "";
  bool gotResponse = false;

  while (millis() - startTime < 60000) { 
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("{") && line.endsWith("}")) {
        pythonResponse = line;
        gotResponse = true;
        break;
      }
    }
    delay(10);
  }

  if (gotResponse) {
    server.send(200, "application/json", pythonResponse);
  } else {
    server.send(500, "application/json", "{\"error\":\"USB Timeout. Is the Python server running?\"}");
  }
}

void setup() {
  Serial.begin(115200);
  
  Serial.println("\nStarting Wi-Fi Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  server.on("/api/solve", HTTP_POST, forwardToPython);
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  
  Serial.println("\n--- ESP32 CAPTIVE PORTAL ONLINE ---");
  Serial.println("Ready to connect!");
}

void loop() { 
  dnsServer.processNextRequest();
  server.handleClient(); 
}