let socket;

let currentFilename = '';
let fileData = [];
let isDownloading = false;

let currentRunningSequence = null;

const statusElement = document.getElementById("connection-status");

function updateStatus(connected) {
    if (connected) {
        statusElement.textContent = "Connected";
        statusElement.classList.remove("disconnected");
        statusElement.classList.add("connected");
    } else {
        statusElement.textContent = "Not Connected";
        statusElement.classList.remove("connected");
        statusElement.classList.add("disconnected");
    }
}

function initWebSocket() {
    socket = new WebSocket("ws://" + location.host + ":81/");

    socket.onopen = () => {
        console.log("WebSocket connected");
        requestFileList();
        socket.send(`GET_FS_INFO`);
        setPWM(0);
        setFanPWM(0,0);
        setFanPWM(1,0);
        updateStatus(true);
    }
    socket.onmessage = (event) => handleMessage(event.data);
    socket.onclose = () => {
        setTimeout(initWebSocket, 2000);
        updateStatus(false);
    }
}

let pendingDurations = {};

function handleMessage(message) {
    // console.log("Received:", message);
    if (typeof message === "string") {
        if (message.startsWith("LOG:")) {
            addLog(message.substring(4));
        } else if (message.startsWith("FILES:")) {
            updateFileList(message.substring(6));
        } else if (message.startsWith("START_DOWNLOAD:")) {
            currentFilename = message.split(':')[1];
            fileData = [];
            isDownloading = true;
            console.log("Starting download of:", currentFilename);
        } else if (message === `END_DOWNLOAD:${currentFilename}`) {
            isDownloading = false;

            const blob = new Blob(fileData, { type: 'application/octet-stream' });

            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = currentFilename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            console.log("Download complete for:", currentFilename);

            currentFilename = '';
            fileData = [];
        } else if (message.startsWith("DELETED:")) {
            let fileName = message.split(':')[1];
            requestFileList();
            socket.send(`GET_FS_INFO`);
        } else if (message.startsWith("FS_INFO:")) {
            let data = JSON.parse(message.substring(8));
            document.getElementById("fsInfo").innerHTML =
                `Total: ${data.total} bytes, ` +
                `Used: ${data.used} bytes, ` +
                `Free: ${data.free} bytes`;
        } else if (message.startsWith("UPLOADED_SIZE:")) {
            let size = parseInt(message.substring(14));
            document.getElementById("uploadProgress").value = size;
        } else if (message.startsWith("DURATION:")) {
            let msg = message.split(':');
            let fileName = msg[1];
            let duration = parseInt(msg[2]);
            let durationId = pendingDurations[fileName];
            if (durationId) {
                let durationElement = document.getElementById(durationId);
                if (durationElement) {
                    if (duration < 10000) {
                        durationElement.textContent = `${duration} ms`;
                    } else if (duration < 60000) {
                        durationElement.textContent = `${(duration / 1000).toFixed(1)} s`;
                    } else if (duration < 60 * 60000) {
                        durationElement.textContent = `${(duration / 60000).toFixed(1)} min`;
                    } else {
                        durationElement.textContent = `${(duration / 3600000).toFixed(1)} h`;
                    }
                }
                delete pendingDurations[fileName];
            }
        } else if (message.startsWith("SEQ_STARTED:")) {
            let filename = message.split(':')[1];
            updateRunButton(filename);
            showCurrentSequence(filename);
        } else if (message.startsWith("SEQ_STOPPED")) {
            updateStopButton();
            currentRunningSequence = null;
            hideCurrentSequence();
        } else if (message.startsWith("SEQ_PROGRESS:")) {
            let parts = message.split(':');
            let progress = parseInt(parts[1]);
            updateSequenceProgress(progress);
        } else if (message.startsWith("CAL_STARTED")) {
            showCalibration();
            document.getElementById("calibration").classList.add("active");
        } else if (message.startsWith("CAL_STOPPED")) {
            hideCalibration();
            document.getElementById("calibration").classList.remove("active");
        } else if (message.startsWith("CAL_PROGRESS:")) {
            let parts = message.split(':');
            let progress = parseInt(parts[1]);
            let lux = parseFloat(parts[2]);
            updateCalibrationProgress(progress,lux);
        } else if (message.startsWith("LUX:")) {
            let lux = parseFloat(message.substring(4));
            updateLuxDisplay(lux);
        }
    } else if (isDownloading) {
        fileData.push(message);
    }
}

function deleteFile(fileName) {
    if (confirm(`delete file "${fileName}" ?`)) {
        socket.send(`DELETE:${fileName}`);
    }
}

function startSequence(fileName) {
    if (currentRunningSequence) { //one is running?
        updateStopButton();
    }
    currentRunningSequence = fileName;
    socket.send(`START_SEQ:${fileName}`);
}

function stopSequence(fileName) {
    socket.send(`STOP_SEQ`);
}

function updateRunButton(fileName) {
    let button = document.querySelector(`.run-button[data-file='${fileName}']`);
    if (button) {
        button.innerHTML = "⏹️"; // stop
        button.onclick = function () { stopSequence(fileName); };
    }
}

function updateStopButton() {
    let fileName = currentRunningSequence;
    let button = document.querySelector(`.run-button[data-file='${fileName}']`);
    if (button) {
        button.innerHTML = `<svg width="20" height="20" viewBox="0 0 24 24" fill="green">
                                <polygon points="5,3 19,12 5,21"></polygon>
                            </svg>`; // play
        button.onclick = function () { startSequence(fileName); };
    }
}

// show calibration progression
function showCalibration() {
    document.getElementById('currentCalibration').style.display = 'block';
    updateCalibrationProgress(0,0.0);
}

function hideCalibration() {
    document.getElementById('currentCalibration').style.display = 'none';
    updateCalibrationProgress(0,0.0);
}

function updateCalibrationProgress(progress,lux) {
    let progressBar = document.getElementById('progressBarCal');
    let progressText = document.getElementById('progressCalText');

    document.getElementById('currentcalValue').textContent = lux + ' lux';
    
    let clampedProgress = Math.max(0, Math.min(100, progress/10.0));
    progressBar.style.width = clampedProgress + '%';
    progressText.textContent = clampedProgress + '%';
}

// show sequence progression
function showCurrentSequence(filename) {
    document.getElementById('currentSequence').style.display = 'block';
    document.getElementById('currentSeqName').textContent = filename;
    updateSequenceProgress(0);
}

function hideCurrentSequence() {
    document.getElementById('currentSequence').style.display = 'none';
    document.getElementById('currentSeqName').textContent = '--';
    updateSequenceProgress(0);
}

function updateSequenceProgress(progress) {
    let progressBar = document.getElementById('progressBar');
    let progressText = document.getElementById('progressText');
    
    let clampedProgress = Math.max(0, Math.min(100, progress));
    progressBar.style.width = clampedProgress + '%';
    progressText.textContent = clampedProgress + '%';
}


function requestDownloadFile(fileName) {
    socket.send(`GET:${fileName}`);
}

function updateFileList(jsonString) {
    try {
        let files = JSON.parse(jsonString);
        let fileListContainer = document.getElementById("fileList");
        let seqListContainer = document.getElementById("seqList");
        let seqCount = 0;

        // Main table (all files)
        let tableHTML = `
            <table class="file-table">
                <thead>
                    <tr>
                        <th>Filename</th>
                        <th>Size (bytes)</th>
                        <th>Action</th>
                    </tr>
                </thead>
                <tbody>
        `;

        // Table for .seq files
        let seqTableHTML = `
            <table class="file-table">
                <thead>
                    <tr>
                        <th>Filename</th>
                        <th>Size (bytes)</th>
                        <th>Duration</th>
                        <th>Run</th>
                    </tr>
                </thead>
                <tbody>
        `;

        if (!Array.isArray(files) || files.length === 0) {
            tableHTML += `<tr><td colspan="3">No file found.</td></tr>`;
            seqTableHTML += `<tr><td colspan="5">No sequence file found.</td></tr>`;
        } else {
            files.forEach(file => {
                if (file.name && file.size !== undefined) {
                    // Add to the main table
                    tableHTML += `
                        <tr>
                            <td class="filename" onclick="requestDownloadFile('${file.name}')">${file.name}</td>
                            <td class="size">${file.size}</td>
                            <td class="action">
                                <button class="trash-button" onclick="deleteFile('${file.name}')">🗑️</button>
                            </td>
                        </tr>
                    `;

                    // Add to the .seq file table
                    if (file.name.endsWith(".seq")) {
                        seqCount++;
                        let durationId = `duration-${file.name.replace(/\W/g, "_")}`; // Unique ID for the duration

                        seqTableHTML += `
                            <tr>
                                <td class="filename">${file.name}</td>
                                <td class="size">${file.size}</td>
                                <td class="duration" id="${durationId}">--</td>
                                <td class="run">
                                    <button class="run-button" data-file="${file.name}" onclick="startSequence('${file.name}')">
                                        <svg width="20" height="20" viewBox="0 0 24 24" fill="green">
                                            <polygon points="5,3 19,12 5,21"></polygon>
                                        </svg>
                                    </button>
                                </td>
                            </tr>
                        `;
                        requestDuration(file.name, durationId);
                    }
                }
            });
        }

        tableHTML += `</tbody></table>`;
        seqTableHTML += `</tbody></table>`;

        // Update the interface
        fileListContainer.innerHTML = tableHTML;
        if (seqCount === 0) { seqTableHTML = ""; }
        seqListContainer.innerHTML = seqTableHTML;
    } catch (e) {
        console.error("Erreur de parsing JSON :", e);
    }
}

function requestDuration(fileName, durationId) {
    socket.send("GET_DURATION:" + fileName);
    pendingDurations[fileName] = durationId;
}

function addLog(log) {
    let logElement = document.getElementById("logBox");
    let entry = document.createElement("div");
    entry.textContent = log;
    logElement.appendChild(entry);
    logElement.scrollTop = logElement.scrollHeight;
}

function toggleLED() {
    socket.send("LED_TOGGLE");
}

function calibration() {
    socket.send("TOGGLE_CALIBRATION");
}

function luxRead() {
    let button = document.getElementById("luxRead");
    if (button.classList.contains("active")) {
        socket.send("LUX_READ_STOP");
        button.classList.remove("active");
    } else {
        socket.send("LUX_READ_START");
        button.classList.add("active");
    }
}

function updateLuxDisplay(lux) {
    let luxDisplay = document.getElementById("luxDisplay");
    if (luxDisplay) {
        luxDisplay.textContent = `${lux.toFixed(2)} lux`;
    }
}

function setPWM(value) {
    document.getElementById("pwmValue").value = value;
    document.getElementById("pwmSlider").value = value;
    socket.send("PWM:" + value);
}

function setFanPWM(id, value) {
    if(id==0) {
        document.getElementById("fan0Value").value = value;
        document.getElementById("fan0Slider").value = value;
        socket.send("FAN0:" + value);
    } else if(id == 1) {
        document.getElementById("fan1Value").value = value;
        document.getElementById("fan1Slider").value = value;
        socket.send("FAN1:" + value);
    }
}


function requestFileList() {
    socket.send("LIST");
}

document.addEventListener("DOMContentLoaded", function () {
    initWebSocket();
    document.getElementById("toggleLED").addEventListener("click", toggleLED);
    document.getElementById("calibration").addEventListener("click", calibration);
    document.getElementById("luxRead").addEventListener("click", luxRead);
    document.getElementById("pwmSlider").addEventListener("input", (e) => setPWM(e.target.value));
    document.getElementById("pwmValue").addEventListener("input", (e) => setPWM(e.target.value));
    document.getElementById("fan0Slider").addEventListener("input", (e) => setFanPWM(0,e.target.value));
    document.getElementById("fan0Value").addEventListener("input" , (e) => setFanPWM(0,e.target.value));
    document.getElementById("fan1Slider").addEventListener("input", (e) => setFanPWM(1,e.target.value));
    document.getElementById("fan1Value").addEventListener("input" , (e) => setFanPWM(1,e.target.value));

    dropZone.addEventListener("dragover", function (event) {
        event.preventDefault();
        dropZone.style.backgroundColor = "#ddd";
    });

    dropZone.addEventListener("dragleave", function () {
        dropZone.style.backgroundColor = "";
    });

    dropZone.addEventListener("drop", function (event) {
        event.preventDefault();
        dropZone.style.backgroundColor = "";

        let files = event.dataTransfer.files;
        if (files.length > 0) {
            uploadFile(files[0]);
        }
    });
});

function uploadFile(file) {
    let reader = new FileReader();
    reader.readAsArrayBuffer(file);

    reader.onload = function (event) {
        let arrayBuffer = event.target.result;
        let fileName = file.name;

        if (socket && socket.readyState === WebSocket.OPEN) {
            // Send the file name
            socket.send("UPLOAD:" + fileName);

            // Send the data in chunks
            let chunkSize = 512;
            let bytes = new Uint8Array(arrayBuffer);
            document.getElementById("uploadProgress").max = bytes.length;
            for (let i = 0; i < bytes.length; i += chunkSize) {
                let chunk = bytes.slice(i, i + chunkSize);
                socket.send(chunk);
            }

            // End-of-file signal
            socket.send("UPLOAD_DONE");
            requestFileList();
            socket.send(`GET_FS_INFO`);
        }
    };
}
