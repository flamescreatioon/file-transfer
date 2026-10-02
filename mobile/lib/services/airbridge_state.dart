import 'dart:io';
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';
import 'package:path/path.dart' as p;
import 'web_assets.dart'; // import embedded HTML dashboard

enum TransferDirection { send, receive }
enum TransferState { idle, running, completed, failed }

class TransferItem {
  final String filename;
  final String filepath;
  final int totalBytes;
  int bytesTransferred;
  double speedMBps;
  final TransferDirection direction;
  TransferState state;
  String? error;

  TransferItem({
    required this.filename,
    required this.filepath,
    required this.totalBytes,
    this.bytesTransferred = 0,
    this.speedMBps = 0.0,
    required this.direction,
    this.state = TransferState.idle,
    this.error,
  });
}

class PeerDevice {
  String name;
  final String ip;
  final int port;
  String os;
  DateTime lastSeen;

  PeerDevice({
    required this.name,
    required this.ip,
    required this.port,
    required this.os,
    required this.lastSeen,
  });
}

class AirBridgeState extends ChangeNotifier {
  static const int udpPort = 53535;
  static const int tcpPort = 53536;

  String deviceName = "Android Device";
  String localIp = "127.0.0.1";
  bool isRunning = false;

  final List<PeerDevice> peers = [];
  final List<TransferItem> transfers = [];
  final List<String> clipboardHistory = [];
  String lastReceivedClip = "";
  
  Future<bool> Function(String senderName, String filename, int size)? onConfirmTransfer;
  final DiskWriteLock _writeLock = DiskWriteLock();
  final Set<String> _activeTransferIps = {};

  RawDatagramSocket? _udpSocket;
  HttpServer? _httpServer;
  Timer? _broadcastTimer;
  Timer? _pruneTimer;

  AirBridgeState() {
    _initDeviceDetails();
  }

  Future<void> _initDeviceDetails() async {
    localIp = await getLocalIP();
    deviceName = "Android-Phone";
    notifyListeners();
  }

  Future<void> startServices() async {
    if (isRunning) return;
    isRunning = true;

    await _startUdpDiscovery();
    await _startHttpServer();

    // Broadcast presence every 2 seconds
    _broadcastTimer = Timer.periodic(const Duration(seconds: 2), (timer) {
      _broadcastPresence();
    });

    // Prune stale peers every 2 seconds
    _pruneTimer = Timer.periodic(const Duration(seconds: 2), (timer) {
      _pruneStalePeers();
    });

    notifyListeners();
  }

  void stopServices() {
    if (!isRunning) return;
    isRunning = false;

    _broadcastTimer?.cancel();
    _pruneTimer?.cancel();
    _udpSocket?.close();
    _httpServer?.close(force: true);

    peers.clear();
    notifyListeners();
  }

  Future<String> getLocalIP() async {
    try {
      for (var interface in await NetworkInterface.list()) {
        for (var addr in interface.addresses) {
          if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
            if (addr.address.startsWith("192.") || 
                addr.address.startsWith("10.") || 
                addr.address.startsWith("172.")) {
              return addr.address;
            }
          }
        }
      }
    } catch (e) {
      debugPrint("Error getting IP: $e");
    }
    return "127.0.0.1";
  }

  Future<void> _startUdpDiscovery() async {
    try {
      _udpSocket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, udpPort, reuseAddress: true, reusePort: true);
      _udpSocket?.broadcastEnabled = true;
      _udpSocket?.listen((event) {
        if (event == RawSocketEvent.read) {
          Datagram? dg = _udpSocket?.receive();
          if (dg != null) {
            String msg = utf8.decode(dg.data, allowMalformed: true);
            if (msg.startsWith("AIRB_DISCOVER:")) {
              _parseDiscoveryMessage(msg, dg.address.address);
            }
          }
        }
      });
    } catch (e) {
      debugPrint("UDP Bind error: $e");
    }
  }

  void _parseDiscoveryMessage(String msg, String senderIp) {
    if (senderIp == localIp) return;
    
    final parts = msg.substring(14).split(':');
    if (parts.length >= 3) {
      final name = parts[0];
      final os = parts[1];
      final port = int.tryParse(parts[2]) ?? tcpPort;

      final existingIndex = peers.indexWhere((p) => p.ip == senderIp && p.port == port);
      if (existingIndex != -1) {
        peers[existingIndex].name = name;
        peers[existingIndex].os = os;
        peers[existingIndex].lastSeen = DateTime.now();
      } else {
        peers.add(PeerDevice(
          name: name,
          ip: senderIp,
          port: port,
          os: os,
          lastSeen: DateTime.now(),
        ));
        notifyListeners();
      }
    }
  }

  void _broadcastPresence() {
    if (_udpSocket == null) return;
    try {
      final payload = "AIRB_DISCOVER:$deviceName:Android:$tcpPort";
      final data = utf8.encode(payload);
      _udpSocket?.send(data, InternetAddress("255.255.255.255"), udpPort);
      _udpSocket?.send(data, InternetAddress("224.0.0.1"), udpPort);
    } catch (e) {
      debugPrint("UDP Broadcast error: $e");
    }
  }

  void _pruneStalePeers() {
    final now = DateTime.now();
    final beforeSize = peers.length;
    peers.removeWhere((p) {
      if (_activeTransferIps.contains(p.ip)) {
        return false;
      }
      return now.difference(p.lastSeen).inSeconds > 20;
    });
    if (peers.length != beforeSize) {
      notifyListeners();
    }
  }

  void touchPeer(String ip) {
    final existingIndex = peers.indexWhere((p) => p.ip == ip && p.port == tcpPort);
    if (existingIndex != -1) {
      peers[existingIndex].lastSeen = DateTime.now();
    }
  }

  // HTTP Server implementation on Port 53536
  Future<void> _startHttpServer() async {
    try {
      _httpServer = await HttpServer.bind(InternetAddress.anyIPv4, tcpPort, shared: true);
      _httpServer?.listen((HttpRequest request) {
        _handleHttpRequest(request);
      });
    } catch (e) {
      debugPrint("HTTP Server start error: $e");
    }
  }

  Future<void> _handleHttpRequest(HttpRequest request) async {
    final path = request.uri.path;
    final method = request.method;

    // Support CORS for Web Interface requests
    request.response.headers.add("Access-Control-Allow-Origin", "*");
    request.response.headers.add("Access-Control-Allow-Headers", "*");
    request.response.headers.add("Access-Control-Allow-Methods", "GET, POST, OPTIONS");

    if (method == "OPTIONS") {
      request.response.statusCode = HttpStatus.ok;
      await request.response.close();
      return;
    }

    // Automatically check for pairing headers on every request to auto-discover peers
    final clientIp = request.connectionInfo?.remoteAddress.address ?? "";
    final deviceNameHeader = request.headers.value("X-Device-Name");
    final deviceOSHeader = request.headers.value("X-Device-OS");
    if (deviceNameHeader != null && deviceOSHeader != null && clientIp.isNotEmpty) {
      _registerOrUpdatePeer(deviceNameHeader, clientIp, deviceOSHeader);
    }

    try {
      if (method == "GET" && path == "/") {
        // Serve embedded glassmorphic HTML panel
        request.response.headers.contentType = ContentType.html;
        request.response.write(INDEX_HTML);
        await request.response.close();
      } 
      else if (method == "GET" && path == "/api/status") {
        // Return host JSON status
        final files = await _getReceivedFiles();
        final peersList = peers.map((p) => {
          'name': p.name,
          'ip': p.ip,
          'port': p.port,
          'os': p.os
        }).toList();

        final status = {
          'my_name': deviceName,
          'my_ip': localIp,
          'my_port': tcpPort,
          'my_os': 'Android',
          'last_clip': lastReceivedClip,
          'peers': peersList,
          'files': files
        };

        request.response.headers.contentType = ContentType.json;
        request.response.write(json.encode(status));
        await request.response.close();
      } 
      else if (method == "POST" && (path == "/api/upload" || path == "/api/send_file")) {
        // File upload stream endpoint
        final filenameQuery = request.uri.queryParameters['filename'] ?? request.uri.queryParameters['file'];
        final filenameHeader = request.headers.value("X-File-Name");
        final rawFilename = filenameQuery ?? filenameHeader ?? "Uploaded_File_${DateTime.now().millisecondsSinceEpoch}";

        // 1. Sanitize the filename to prevent directory traversal or system conflicts
        final filename = sanitizeFilename(rawFilename);

        final sizeQuery = request.uri.queryParameters['size'];
        final sizeHeader = request.headers.value("X-File-Size");
        final fileSize = int.tryParse(sizeQuery ?? sizeHeader ?? "") ?? request.contentLength;

        // 2. Security: Prompt user to Accept or Decline the file transfer request
        final senderName = deviceNameHeader ?? "Web Browser";
        if (onConfirmTransfer != null) {
          final accepted = await onConfirmTransfer!(senderName, filename, fileSize);
          if (!accepted) {
            request.response.statusCode = HttpStatus.forbidden;
            request.response.headers.contentType = ContentType.json;
            request.response.write(json.encode({'status': 'declined', 'error': 'Transfer declined by receiver'}));
            await request.response.close();
            return;
          }
        }

        final dir = await getExternalStorageDirectory() ?? await getApplicationDocumentsDirectory();
        final uniqueFilename = await getUniqueFilename(dir.path, filename);
        final savePath = p.join(dir.path, uniqueFilename);

        final transfer = TransferItem(
          filename: uniqueFilename,
          filepath: savePath,
          totalBytes: fileSize,
          direction: TransferDirection.receive,
          state: TransferState.running,
        );
        transfers.add(transfer);
        notifyListeners();

        final file = File(savePath);
        final iosink = file.openWrite();

        int received = 0;
        int lastPrintBytes = 0;
        final stopwatch = Stopwatch()..start();
        var lastPrintTime = stopwatch.elapsedMilliseconds;

        _activeTransferIps.add(clientIp);

        // Acquire lock to serialize writes and prevent storage speed thrashing
        await _writeLock.acquire();

        try {
          await for (var chunk in request) {
            iosink.add(chunk);
            received += chunk.length;
            lastPrintBytes += chunk.length;

            transfer.bytesTransferred = received;
            touchPeer(clientIp); // Touch liveness from incoming bytes

            final now = stopwatch.elapsedMilliseconds;
            final diff = now - lastPrintTime;
            if (diff >= 200) {
              transfer.speedMBps = (lastPrintBytes / (1024.0 * 1024.0)) / (diff / 1000.0);
              lastPrintTime = now;
              lastPrintBytes = 0;
              notifyListeners();
            }
          }
        } catch (e) {
          debugPrint("Error receiving bytes: $e");
        } finally {
          await iosink.close();
          _writeLock.release();
          _activeTransferIps.remove(clientIp);
        }
        stopwatch.stop();

        final success = (received == fileSize && fileSize > 0);
        transfer.state = success ? TransferState.completed : TransferState.failed;
        transfer.speedMBps = 0.0;

        // 3. Cleanup: If transfer was interrupted or failed, delete the partial temporary file
        if (!success) {
          try {
            if (await file.exists()) {
              await file.delete();
            }
          } catch(e) {
            debugPrint("Failed to delete partial file: $e");
          }
        }
        
        notifyListeners();

        request.response.statusCode = success ? HttpStatus.ok : HttpStatus.internalServerError;
        request.response.headers.contentType = ContentType.json;
        request.response.write(json.encode({'status': success ? 'ok' : 'failed'}));
        await request.response.close();
      } 
      else if (method == "POST" && (path == "/api/clipboard" || path == "/api/send_clip")) {
        // Clipboard update endpoint
        final bytesBuilder = BytesBuilder();
        await for (var chunk in request) {
          bytesBuilder.add(chunk);
        }
        String body = utf8.decode(bytesBuilder.takeBytes());
        String text = "";
        
        if (request.headers.contentType?.mimeType == "application/json") {
          final data = json.decode(body) as Map<String, dynamic>;
          text = data['text'] ?? "";
        } else {
          text = body;
        }

        if (text.isNotEmpty) {
          lastReceivedClip = text;
          clipboardHistory.add(text);
          if (clipboardHistory.length > 10) {
            clipboardHistory.removeAt(0);
          }
          await Clipboard.setData(ClipboardData(text: text));
          notifyListeners();
        }

        request.response.statusCode = HttpStatus.ok;
        request.response.headers.contentType = ContentType.json;
        request.response.write(json.encode({'status': 'ok'}));
        await request.response.close();
      } 
      else if (method == "GET" && path == "/api/download") {
        // File download endpoint
        final filename = request.uri.queryParameters['file'] ?? "";
        if (filename.isEmpty) {
          request.response.statusCode = HttpStatus.badRequest;
          request.response.write("Missing 'file' parameter");
          await request.response.close();
          return;
        }

        final dir = await getExternalStorageDirectory() ?? await getApplicationDocumentsDirectory();
        final file = File(p.join(dir.path, filename));

        if (await file.exists()) {
          final size = await file.length();
          request.response.headers.contentType = ContentType.parse("application/octet-stream");
          request.response.headers.add("Content-Disposition", "attachment; filename=\"$filename\"");
          request.response.contentLength = size;

          await request.response.addStream(file.openRead());
          await request.response.close();
        } else {
          request.response.statusCode = HttpStatus.notFound;
          request.response.write("File not found");
          await request.response.close();
        }
      } 
      else {
        request.response.statusCode = HttpStatus.notFound;
        request.response.write("Not Found");
        await request.response.close();
      }
    } catch (e) {
      debugPrint("HTTP server handle error: $e");
      try {
        request.response.statusCode = HttpStatus.internalServerError;
        await request.response.close();
      } catch(_) {}
    }
  }

  void _registerOrUpdatePeer(String name, String ip, String os) {
    if (ip == localIp) return;
    final existingIndex = peers.indexWhere((p) => p.ip == ip && p.port == tcpPort);
    if (existingIndex != -1) {
      peers[existingIndex].name = name;
      peers[existingIndex].os = os;
      peers[existingIndex].lastSeen = DateTime.now();
    } else {
      peers.add(PeerDevice(
        name: name,
        ip: ip,
        port: tcpPort,
        os: os,
        lastSeen: DateTime.now(),
      ));
    }
    notifyListeners();
  }

  Future<List<Map<String, dynamic>>> _getReceivedFiles() async {
    final dir = await getExternalStorageDirectory() ?? await getApplicationDocumentsDirectory();
    final list = <Map<String, dynamic>>[];
    try {
      if (await dir.exists()) {
        await for (var entity in dir.list()) {
          if (entity is File) {
            list.add({
              'name': p.basename(entity.path),
              'size': await entity.length(),
            });
          }
        }
      }
    } catch(e) {
      debugPrint("Error listing files: $e");
    }
    return list;
  }

  // Client request: Send file to a peer via HTTP POST
  Future<void> sendFile(PeerDevice peer, File file) async {
    final filepath = file.path;
    final filename = p.basename(filepath);
    final size = await file.length();

    final transfer = TransferItem(
      filename: filename,
      filepath: filepath,
      totalBytes: size,
      direction: TransferDirection.send,
      state: TransferState.running,
    );
    transfers.add(transfer);
    notifyListeners();

    HttpClient? client;
    try {
      _activeTransferIps.add(peer.ip);
      client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
      
      final url = Uri.parse("http://${peer.ip}:${peer.port}/api/upload");
      final request = await client.postUrl(url);
      
      // Set metadata headers
      request.headers.set("X-File-Name", filename);
      request.headers.set("X-File-Size", size.toString());
      request.headers.set("X-Device-Name", deviceName);
      request.headers.set("X-Device-OS", "Android");
      request.contentLength = size;

      final fileStream = file.openRead();
      int sent = 0;
      int lastPrintBytes = 0;
      final stopwatch = Stopwatch()..start();
      var lastPrintTime = stopwatch.elapsedMilliseconds;

      await for (var chunk in fileStream) {
        request.add(chunk);
        sent += chunk.length;
        lastPrintBytes += chunk.length;

        transfer.bytesTransferred = sent;
        touchPeer(peer.ip); // Touch liveness from outgoing bytes

        final now = stopwatch.elapsedMilliseconds;
        final diff = now - lastPrintTime;
        if (diff >= 200) {
          transfer.speedMBps = (lastPrintBytes / (1024.0 * 1024.0)) / (diff / 1000.0);
          lastPrintTime = now;
          lastPrintBytes = 0;
          notifyListeners();
        }
      }
      await request.flush();
      stopwatch.stop();

      final response = await request.close();
      if (response.statusCode == 200) {
        transfer.state = TransferState.completed;
      } else {
        throw Exception("Server returned code ${response.statusCode}");
      }
      transfer.speedMBps = 0.0;
    } catch (e) {
      transfer.state = TransferState.failed;
      transfer.error = e.toString();
      transfer.speedMBps = 0.0;
    } finally {
      client?.close();
      _activeTransferIps.remove(peer.ip);
      notifyListeners();
    }
  }

  // Client request: Send clipboard text to a peer via HTTP POST
  Future<void> sendClipboardText(PeerDevice peer, String text) async {
    HttpClient? client;
    try {
      client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
      final url = Uri.parse("http://${peer.ip}:${peer.port}/api/clipboard");
      final request = await client.postUrl(url);
      
      request.headers.set("X-Device-Name", deviceName);
      request.headers.set("X-Device-OS", "Android");
      request.headers.contentType = ContentType.text;
      
      request.write(text);
      await request.close();
    } catch (e) {
      debugPrint("Error sending clipboard: $e");
    } finally {
      client?.close();
    }
  }

  // Unicast pairing check
  Future<bool> manualConnect(String ip) async {
    HttpClient? client;
    try {
      client = HttpClient()..connectionTimeout = const Duration(seconds: 4);
      final url = Uri.parse("http://$ip:$tcpPort/api/status");
      final request = await client.getUrl(url);
      
      request.headers.set("X-Device-Name", deviceName);
      request.headers.set("X-Device-OS", "Android");
      
      final response = await request.close();
      if (response.statusCode == 200) {
        final bytesBuilder = BytesBuilder();
        await for (var chunk in response) {
          bytesBuilder.add(chunk);
        }
        final body = utf8.decode(bytesBuilder.takeBytes());
        final data = json.decode(body) as Map<String, dynamic>;
        
        final pcName = data['my_name'] ?? "Desktop-Peer";
        final pcOs = data['my_os'] ?? "Windows";
        
        _registerOrUpdatePeer(pcName, ip, pcOs);
        return true;
      }
      return false;
    } catch(e) {
      debugPrint("Manual connect error to $ip: $e");
      return false;
    } finally {
      client?.close();
    }
  }

  Future<String> getUniqueFilename(String dirPath, String filename) async {
    final file = File(p.join(dirPath, filename));
    if (!await file.exists()) return filename;

    final stem = p.basenameWithoutExtension(filename);
    final ext = p.extension(filename);
    int counter = 1;
    while (true) {
      final candidate = "$stem ($counter)$ext";
      final candidateFile = File(p.join(dirPath, candidate));
      if (!await candidateFile.exists()) {
        return candidate;
      }
      counter++;
    }
  }

  String sanitizeFilename(String filename) {
    var s = p.basename(filename);
    final invalidChars = RegExp(r'[<>:"/\\|?*\x00-\x1F]');
    s = s.replaceAll(invalidChars, '_');
    if (s.isEmpty) s = "unnamed_file";
    return s;
  }
}

class DiskWriteLock {
  Completer<void>? _current;

  Future<void> acquire() async {
    while (_current != null) {
      await _current!.future;
    }
    _current = Completer<void>();
  }

  void release() {
    final temp = _current;
    _current = null;
    temp?.complete();
  }
}
