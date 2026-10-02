#ifndef WEB_ASSETS_HPP
#define WEB_ASSETS_HPP

#include <string>

const std::string INDEX_HTML = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AirBridge - File Bridge</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;800;900&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0b0f19;
            --card-bg: rgba(255, 255, 255, 0.03);
            --card-border: rgba(255, 255, 255, 0.08);
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --accent-red: #ef4444;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
        }

        body {
            font-family: 'Inter', sans-serif;
            background-color: var(--bg-color);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            overflow-x: hidden;
            background-image: 
                radial-gradient(circle at 10% 20%, rgba(59, 130, 246, 0.08) 0%, transparent 40%),
                radial-gradient(circle at 90% 80%, rgba(16, 185, 129, 0.06) 0%, transparent 45%);
        }

        header {
            padding: 24px 40px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid var(--card-border);
            backdrop-filter: blur(10px);
            position: sticky;
            top: 0;
            z-index: 100;
        }

        .logo-container {
            display: flex;
            flex-direction: column;
        }

        .logo {
            font-size: 24px;
            font-weight: 900;
            letter-spacing: 2px;
            background: linear-gradient(135deg, #3b82f6, #10b981);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 13px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--accent-green);
            box-shadow: 0 0 8px var(--accent-green);
        }

        .container {
            max-width: 1200px;
            width: 100%;
            margin: 0 auto;
            padding: 40px;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 32px;
            flex-grow: 1;
        }

        @media (max-width: 900px) {
            .container {
                grid-template-columns: 1fr;
            }
        }

        .panel {
            background: var(--card-bg);
            border: 1px solid var(--card-border);
            border-radius: 24px;
            padding: 28px;
            backdrop-filter: blur(20px);
            display: flex;
            flex-direction: column;
            gap: 24px;
            transition: border-color 0.3s;
        }

        .panel:hover {
            border-color: rgba(255, 255, 255, 0.12);
        }

        .panel-title {
            font-size: 18px;
            font-weight: 800;
            letter-spacing: 0.5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        /* Peer List */
        .peer-list {
            display: flex;
            flex-direction: column;
            gap: 12px;
            max-height: 250px;
            overflow-y: auto;
            padding-right: 4px;
        }

        .peer-item {
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--card-border);
            border-radius: 16px;
            padding: 16px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            cursor: pointer;
            transition: all 0.2s;
        }

        .peer-item:hover {
            background: rgba(255, 255, 255, 0.05);
            transform: translateY(-2px);
        }

        .peer-item.selected {
            border-color: var(--accent-blue);
            background: rgba(59, 130, 246, 0.06);
            box-shadow: 0 0 12px rgba(59, 130, 246, 0.15);
        }

        .peer-info {
            display: flex;
            align-items: center;
            gap: 14px;
        }

        .peer-icon {
            font-size: 24px;
        }

        .peer-details {
            display: flex;
            flex-direction: column;
            gap: 2px;
        }

        .peer-name {
            font-weight: 600;
            font-size: 15px;
        }

        .peer-ip {
            font-size: 12px;
            color: var(--text-secondary);
        }

        .peer-os-badge {
            font-size: 10px;
            padding: 2px 8px;
            border-radius: 8px;
            background: rgba(255, 255, 255, 0.08);
            color: var(--text-secondary);
            font-weight: 600;
            align-self: center;
        }

        /* Drag & Drop Zone */
        .drop-zone {
            border: 2px dashed rgba(255, 255, 255, 0.15);
            border-radius: 20px;
            padding: 40px 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 12px;
            cursor: pointer;
            transition: all 0.3s;
            background: rgba(255, 255, 255, 0.01);
            text-align: center;
        }

        .drop-zone:hover, .drop-zone.dragover {
            border-color: var(--accent-blue);
            background: rgba(59, 130, 246, 0.02);
        }

        .drop-zone-icon {
            font-size: 40px;
            color: var(--text-secondary);
            transition: color 0.3s;
        }

        .drop-zone:hover .drop-zone-icon {
            color: var(--accent-blue);
        }

        .drop-zone-text {
            font-size: 14px;
            color: var(--text-secondary);
        }

        .drop-zone-text strong {
            color: var(--text-primary);
        }

        #file-input {
            display: none;
        }

        /* Clipboard */
        .clipboard-box {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        textarea {
            width: 100%;
            height: 120px;
            background: rgba(0, 0, 0, 0.2);
            border: 1px solid var(--card-border);
            border-radius: 16px;
            padding: 16px;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 14px;
            resize: none;
            outline: none;
            transition: border-color 0.3s;
        }

        textarea:focus {
            border-color: var(--accent-green);
        }

        .btn {
            padding: 12px 24px;
            border-radius: 12px;
            font-weight: 600;
            font-size: 14px;
            cursor: pointer;
            border: none;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.2s;
            color: white;
        }

        .btn-blue {
            background-color: var(--accent-blue);
        }

        .btn-blue:hover {
            background-color: #2563eb;
            transform: translateY(-1px);
        }

        .btn-green {
            background-color: var(--accent-green);
        }

        .btn-green:hover {
            background-color: #0d9488;
            transform: translateY(-1px);
        }

        .btn:disabled {
            background-color: rgba(255, 255, 255, 0.05);
            color: var(--text-secondary);
            cursor: not-allowed;
            transform: none;
        }

        /* Transfers & Progress */
        .transfers-container {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }

        .transfer-item {
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--card-border);
            border-radius: 16px;
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .transfer-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 14px;
            font-weight: 600;
        }

        .transfer-meta {
            display: flex;
            justify-content: space-between;
            font-size: 12px;
            color: var(--text-secondary);
        }

        .progress-bar-container {
            width: 100%;
            height: 6px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 3px;
            overflow: hidden;
        }

        .progress-bar {
            height: 100%;
            width: 0%;
            background: var(--accent-blue);
            transition: width 0.1s;
        }

        .progress-bar.completed {
            background: var(--accent-green);
        }

        .progress-bar.failed {
            background: var(--accent-red);
        }

        /* Scrollbars */
        ::-webkit-scrollbar {
            width: 6px;
            height: 6px;
        }

        ::-webkit-scrollbar-track {
            background: transparent;
        }

        ::-webkit-scrollbar-thumb {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 10px;
        }

        ::-webkit-scrollbar-thumb:hover {
            background: rgba(255, 255, 255, 0.2);
        }

        /* Shared Files History */
        .files-list {
            display: flex;
            flex-direction: column;
            gap: 10px;
            max-height: 250px;
            overflow-y: auto;
        }

        .file-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 16px;
            background: rgba(255, 255, 255, 0.01);
            border: 1px solid var(--card-border);
            border-radius: 12px;
            font-size: 13px;
        }

        .file-details {
            display: flex;
            flex-direction: column;
            gap: 2px;
        }

        .file-name {
            font-weight: 600;
        }

        .file-size {
            font-size: 11px;
            color: var(--text-secondary);
        }

        .file-actions {
            display: flex;
            gap: 8px;
        }

        .action-link {
            color: var(--accent-blue);
            text-decoration: none;
            font-weight: 600;
            cursor: pointer;
        }

        .action-link:hover {
            text-decoration: underline;
        }

        /* Clipboard History */
        .clip-history-item {
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--card-border);
            border-radius: 12px;
            padding: 12px;
            font-size: 13px;
            font-family: monospace;
            cursor: pointer;
            transition: background 0.2s;
            word-break: break-all;
        }

        .clip-history-item:hover {
            background: rgba(255, 255, 255, 0.05);
        }
    </style>
</head>
<body>
    <header>
        <div class="logo-container">
            <span class="logo">AIRBRIDGE</span>
            <div class="status-badge">
                <span class="status-dot"></span>
                <span id="host-ip">Server Binding...</span>
            </div>
        </div>
    </header>

    <div class="container">
        <!-- Left Column: Devices and File Sender -->
        <div class="panel">
            <div class="panel-title">Nearby Devices <span style="font-size: 12px; font-weight: normal; color: var(--text-secondary);">(Select to connect)</span></div>
            <div class="peer-list" id="peer-list">
                <div style="text-align: center; color: var(--text-secondary); padding: 20px 0; font-size: 14px;">
                    Scanning local network for devices...
                </div>
            </div>

            <div class="panel-title">File Transfer</div>
            <div class="drop-zone" id="drop-zone">
                <span class="drop-zone-icon">📥</span>
                <span class="drop-zone-text">Drag & drop files here or <strong>browse files</strong> to upload</span>
                <input type="file" id="file-input">
            </div>

            <div class="transfers-container" id="transfers-container">
                <!-- Active progress bars will render here -->
            </div>
        </div>

        <!-- Right Column: Clipboard and Files History -->
        <div class="panel">
            <div class="panel-title">Clipboard Transfer</div>
            <div class="clipboard-box">
                <textarea id="clip-textarea" placeholder="Type or paste text here to broadcast to selected device..."></textarea>
                <button class="btn btn-green" id="btn-send-clip" disabled>Sync Clipboard</button>
            </div>

            <div class="panel-title">Last Received Text</div>
            <div class="clip-history-item" id="latest-clip" style="font-style: italic; color: var(--text-secondary);">
                (No clipboard items received yet)
            </div>

            <div class="panel-title">Received Files</div>
            <div class="files-list" id="files-list">
                <div style="text-align: center; color: var(--text-secondary); padding: 20px 0; font-size: 13px;">
                    No received files yet.
                </div>
            </div>
        </div>
    </div>

    <script>
        let selectedPeerIp = null;
        let selectedPeerPort = null;

        // Dom Elements
        const peerListContainer = document.getElementById('peer-list');
        const filesListContainer = document.getElementById('files-list');
        const transfersContainer = document.getElementById('transfers-container');
        const hostIpText = document.getElementById('host-ip');
        const clipTextarea = document.getElementById('clip-textarea');
        const btnSendClip = document.getElementById('btn-send-clip');
        const latestClip = document.getElementById('latest-clip');
        const dropZone = document.getElementById('drop-zone');
        const fileInput = document.getElementById('file-input');

        // Setup dropzone events
        dropZone.addEventListener('click', () => {
            if (!selectedPeerIp) {
                alert("Please select a device from 'Nearby Devices' first!");
                return;
            }
            fileInput.click();
        });

        dropZone.addEventListener('dragover', (e) => {
            e.preventDefault();
            dropZone.classList.add('dragover');
        });

        dropZone.addEventListener('dragleave', () => {
            dropZone.classList.remove('dragover');
        });

        dropZone.addEventListener('drop', (e) => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            if (!selectedPeerIp) {
                alert("Please select a device from 'Nearby Devices' first!");
                return;
            }
            if (e.dataTransfer.files.length > 0) {
                uploadFile(e.dataTransfer.files[0]);
            }
        });

        fileInput.addEventListener('change', () => {
            if (fileInput.files.length > 0) {
                uploadFile(fileInput.files[0]);
                fileInput.value = ''; // Reset
            }
        });

        // Send clipboard button
        btnSendClip.addEventListener('click', async () => {
            const text = clipTextarea.value.trim();
            if (!text || !selectedPeerIp) return;

            btnSendClip.disabled = true;
            try {
                const response = await fetch('/api/send_clip', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ip: selectedPeerIp, port: selectedPeerPort, text: text })
                });
                if (response.ok) {
                    clipTextarea.value = '';
                } else {
                    alert("Failed to send clipboard text.");
                }
            } catch(e) {
                alert("Error connecting to server.");
            } finally {
                btnSendClip.disabled = false;
            }
        });

        // Trigger action when textbox is typed
        clipTextarea.addEventListener('input', () => {
            btnSendClip.disabled = !selectedPeerIp || clipTextarea.value.trim().length === 0;
        });

        // Upload file via AJAX
        function uploadFile(file) {
            const transferId = 'tx-' + Date.now();
            const txEl = document.createElement('div');
            txEl.className = 'transfer-item';
            txEl.id = transferId;
            txEl.innerHTML = `
                <div class="transfer-header">
                    <span>Sending: ${file.name}</span>
                    <span class="pct">0%</span>
                </div>
                <div class="progress-bar-container">
                    <div class="progress-bar" style="width: 0%;"></div>
                </div>
                <div class="transfer-meta">
                    <span class="speed">0.0 MB/s</span>
                    <span class="size">0 / ${(file.size / (1024*1024)).toFixed(1)} MB</span>
                </div>
            `;
            transfersContainer.prepend(txEl);

            const xhr = new XMLHttpRequest();
            xhr.open('POST', `/api/send_file?ip=${encodeURIComponent(selectedPeerIp)}&port=${selectedPeerPort}&filename=${encodeURIComponent(file.name)}&size=${file.size}`);
            
            let startTime = Date.now();

            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const percent = (e.loaded / e.total) * 100;
                    const elapsed = (Date.now() - startTime) / 1000;
                    const speed = elapsed > 0 ? (e.loaded / (1024*1024)) / elapsed : 0;
                    
                    txEl.querySelector('.pct').innerText = `${percent.toFixed(1)}%`;
                    txEl.querySelector('.progress-bar').style.width = `${percent}%`;
                    txEl.querySelector('.speed').innerText = `${speed.toFixed(1)} MB/s`;
                    txEl.querySelector('.size').innerText = `${(e.loaded / (1024*1024)).toFixed(1)} / ${(e.total / (1024*1024)).toFixed(1)} MB`;
                }
            };

            xhr.onload = function() {
                if (xhr.status === 200) {
                    txEl.querySelector('.pct').innerText = 'Completed';
                    txEl.querySelector('.progress-bar').classList.add('completed');
                    txEl.querySelector('.speed').innerText = '0.0 MB/s';
                } else {
                    txEl.querySelector('.pct').innerText = 'Failed';
                    txEl.querySelector('.progress-bar').classList.add('failed');
                    txEl.querySelector('.speed').innerText = 'Error';
                }
                setTimeout(() => txEl.remove(), 5000);
            };

            xhr.onerror = function() {
                txEl.querySelector('.pct').innerText = 'Failed';
                txEl.querySelector('.progress-bar').classList.add('failed');
                txEl.querySelector('.speed').innerText = 'Network Error';
                setTimeout(() => txEl.remove(), 5000);
            };

            xhr.send(file);
        }

        // Helpers to get platform icons
        function getOSIcon(os) {
            os = os.toLowerCase();
            if (os === 'windows') return '💻';
            if (os === 'linux') return '🐧';
            if (os === 'android') return '📱';
            return '🖥️';
        }

        // Copy history to click
        latestClip.addEventListener('click', () => {
            const text = latestClip.innerText;
            if (text !== "(No clipboard items received yet)" && text.trim().length > 0) {
                navigator.clipboard.writeText(text);
                alert("Copied to clipboard!");
            }
        });

        // Main polling loop
        async function pollStatus() {
            try {
                const response = await fetch('/api/status');
                if (!response.ok) return;

                const data = await response.json();
                
                // Update Local IP info
                hostIpText.innerText = `Active on ${data.my_ip}:${data.my_port}`;

                // Update Peer list
                if (data.peers.length === 0) {
                    peerListContainer.innerHTML = `
                        <div style="text-align: center; color: var(--text-secondary); padding: 20px 0; font-size: 14px;">
                            Scanning local network for devices...
                        </div>
                    `;
                    selectedPeerIp = null;
                    selectedPeerPort = null;
                    btnSendClip.disabled = true;
                } else {
                    let html = '';
                    data.peers.forEach(peer => {
                        const isSel = (selectedPeerIp === peer.ip && selectedPeerPort === peer.port);
                        html += `
                            <div class="peer-item ${isSel ? 'selected' : ''}" onclick="selectPeer('${peer.ip}', ${peer.port})">
                                <div class="peer-info">
                                    <span class="peer-icon">${getOSIcon(peer.os)}</span>
                                    <div class="peer-details">
                                        <span class="peer-name">${peer.name}</span>
                                        <span class="peer-ip">${peer.ip}</span>
                                    </div>
                                </div>
                                <span class="peer-os-badge">${peer.os}</span>
                            </div>
                        `;
                    });
                    peerListContainer.innerHTML = html;
                }

                // Update latest clipboard
                if (data.last_clip) {
                    latestClip.innerText = data.last_clip;
                    latestClip.style.fontStyle = 'normal';
                    latestClip.style.color = 'var(--text-primary)';
                }

                // Update Received Files
                if (data.files.length === 0) {
                    filesListContainer.innerHTML = `
                        <div style="text-align: center; color: var(--text-secondary); padding: 20px 0; font-size: 13px;">
                            No received files yet.
                        </div>
                    `;
                } else {
                    let html = '';
                    data.files.forEach(f => {
                        html += `
                            <div class="file-item">
                                <div class="file-details">
                                    <span class="file-name">${f.name}</span>
                                    <span class="file-size">${(f.size / (1024*1024)).toFixed(2)} MB</span>
                                </div>
                                <div class="file-actions">
                                    <a class="action-link" href="/api/download?file=${encodeURIComponent(f.name)}" download>Download</a>
                                </div>
                            </div>
                        `;
                    });
                    filesListContainer.innerHTML = html;
                }
            } catch(e) {
                console.error("Polling error: ", e);
            }
        }

        // Selection peer callback
        window.selectPeer = function(ip, port) {
            selectedPeerIp = ip;
            selectedPeerPort = port;
            pollStatus(); // Re-render selected state immediately
            clipTextarea.dispatchEvent(new Event('input')); // Update clipboard button disabled state
        }

        // Start Polling every 1 second
        setInterval(pollStatus, 1000);
        pollStatus(); // First invoke
    </script>
</body>
</html>
)rawhtml";

#endif // WEB_ASSETS_HPP
