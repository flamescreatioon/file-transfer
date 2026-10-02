import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:file_picker/file_picker.dart';
import 'package:permission_handler/permission_handler.dart';
import '../services/airbridge_state.dart';

class HomeView extends StatefulWidget {
  const HomeView({Key? key}) : super(key: key);

  @override
  State<HomeView> createState() => _HomeViewState();
}

class _HomeViewState extends State<HomeView> {
  final TextEditingController _clipController = TextEditingController();

  @override
  void initState() {
    super.initState();
    _requestPermissions();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final state = Provider.of<AirBridgeState>(context, listen: false);
      
      state.onConfirmTransfer = (senderName, filename, size) async {
        if (!mounted) return false;
        
        final double sizeMB = size / (1024.0 * 1024.0);
        final accepted = await showDialog<bool>(
          context: context,
          barrierDismissible: false,
          builder: (ctx) => AlertDialog(
            backgroundColor: const Color(0xFF1E293B),
            title: const Text("File Transfer Request", style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold)),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  "Device '$senderName' wants to send you a file.",
                  style: const TextStyle(color: Colors.white70, fontSize: 15),
                ),
                const SizedBox(height: 16),
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: const Color(0xFF0F172A),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        "Name: $filename",
                        style: const TextStyle(color: Colors.white, fontWeight: FontWeight.w600, fontSize: 14),
                        overflow: TextOverflow.ellipsis,
                        maxLines: 2,
                      ),
                      const SizedBox(height: 6),
                      Text(
                        "Size: ${sizeMB.toStringAsFixed(2)} MB",
                        style: const TextStyle(color: Colors.blueAccent, fontSize: 13, fontWeight: FontWeight.bold),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                const Text(
                  "Do you want to accept this file?",
                  style: TextStyle(color: Colors.white70, fontSize: 14),
                ),
              ],
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: const Text("Decline", style: TextStyle(color: Colors.redAccent, fontWeight: FontWeight.bold)),
              ),
              ElevatedButton(
                onPressed: () => Navigator.pop(ctx, true),
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF10B981),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                ),
                child: const Text("Accept", style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold)),
              ),
            ],
          ),
        );
        return accepted ?? false;
      };

      state.startServices();
    });
  }

  Future<void> _requestPermissions() async {
    if (Platform.isAndroid) {
      await Permission.storage.request();
      // On Android 13+, request specific media permissions
      await Permission.photos.request();
      await Permission.videos.request();
    }
  }

  @override
  void dispose() {
    _clipController.dispose();
    super.dispose();
  }

  void _showDeviceActions(BuildContext context, PeerDevice peer) {
    showModalBottomSheet(
      context: context,
      backgroundColor: Colors.transparent,
      builder: (ctx) {
        return Container(
          decoration: const BoxDecoration(
            color: Color(0xFF1E293B),
            borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
          ),
          padding: const EdgeInsets.symmetric(vertical: 24, horizontal: 20),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  _getPlatformIcon(peer.os, size: 36),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          peer.name,
                          style: const TextStyle(
                            color: Colors.white,
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        Text(
                          peer.ip,
                          style: TextStyle(
                            color: Colors.white.withOpacity(0.6),
                            fontSize: 14,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 24),
              ElevatedButton.icon(
                onPressed: () async {
                  Navigator.pop(ctx);
                  final result = await FilePicker.pickFiles();
                  if (result != null && result.files.single.path != null) {
                    final file = File(result.files.single.path!);
                    if (context.mounted) {
                      await Provider.of<AirBridgeState>(context, listen: false)
                          .sendFile(peer, file);
                    }
                  }
                },
                icon: const Icon(Icons.file_upload, color: Colors.white),
                label: const Text("Send File", style: TextStyle(color: Colors.white)),
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF3B82F6),
                  padding: const EdgeInsets.symmetric(vertical: 14),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                ),
              ),
              const SizedBox(height: 12),
              ElevatedButton.icon(
                onPressed: () {
                  Navigator.pop(ctx);
                  _showClipboardSendDialog(context, peer);
                },
                icon: const Icon(Icons.copy, color: Colors.white),
                label: const Text("Send Clipboard Text", style: TextStyle(color: Colors.white)),
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF10B981),
                  padding: const EdgeInsets.symmetric(vertical: 14),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                ),
              ),
              const SizedBox(height: 20),
            ],
          ),
        );
      },
    );
  }

  void _showClipboardSendDialog(BuildContext context, PeerDevice peer) {
    showDialog(
      context: context,
      builder: (ctx) {
        return AlertDialog(
          backgroundColor: const Color(0xFF1E293B),
          title: Text(
            "Send Text to ${peer.name}",
            style: const TextStyle(color: Colors.white),
          ),
          content: TextField(
            controller: _clipController,
            style: const TextStyle(color: Colors.white),
            maxLines: 4,
            decoration: InputDecoration(
              hintText: "Type or paste text/links here...",
              hintStyle: TextStyle(color: Colors.white.withOpacity(0.4)),
              filled: true,
              fillColor: const Color(0xFF0F172A),
              border: OutlineInputBorder(
                borderRadius: BorderRadius.circular(12),
                borderSide: BorderSide.none,
              ),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx),
              child: const Text("Cancel", style: TextStyle(color: Colors.grey)),
            ),
            ElevatedButton(
              onPressed: () {
                final text = _clipController.text.trim();
                if (text.isNotEmpty) {
                  Provider.of<AirBridgeState>(context, listen: false)
                      .sendClipboardText(peer, text);
                  _clipController.clear();
                }
                Navigator.pop(ctx);
              },
              style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF10B981)),
              child: const Text("Send", style: TextStyle(color: Colors.white)),
            ),
          ],
        );
      },
    );
  }

  void _showManualConnectDialog(BuildContext context) {
    final TextEditingController ipController = TextEditingController();
    bool connecting = false;

    showDialog(
      context: context,
      builder: (ctx) {
        return StatefulBuilder(
          builder: (context, setState) {
            return AlertDialog(
              backgroundColor: const Color(0xFF1E293B),
              title: const Text("Connect to Device by IP", style: TextStyle(color: Colors.white)),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const Text(
                    "If your device doesn't appear automatically, enter its IP address below. (You can find it on your PC's screen)",
                    style: TextStyle(color: Colors.grey, fontSize: 13),
                  ),
                  const SizedBox(height: 16),
                  TextField(
                    controller: ipController,
                    style: const TextStyle(color: Colors.white),
                    decoration: InputDecoration(
                      hintText: "e.g., 192.168.1.15",
                      hintStyle: TextStyle(color: Colors.white.withOpacity(0.3)),
                      filled: true,
                      fillColor: const Color(0xFF0F172A),
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(12),
                        borderSide: BorderSide.none,
                      ),
                    ),
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: connecting ? null : () => Navigator.pop(ctx),
                  child: const Text("Cancel", style: TextStyle(color: Colors.grey)),
                ),
                ElevatedButton(
                  onPressed: connecting
                      ? null
                      : () async {
                          final ip = ipController.text.trim();
                          if (ip.isEmpty) return;

                          setState(() {
                            connecting = true;
                          });

                          final success = await Provider.of<AirBridgeState>(context, listen: false)
                              .manualConnect(ip);

                          if (ctx.mounted) {
                            Navigator.pop(ctx);
                            ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(
                                content: Text(success
                                    ? "Successfully paired with $ip!"
                                    : "Failed to connect to $ip. Make sure the server is running and your Wi-Fi is correct."),
                                backgroundColor: success ? Colors.green : Colors.red,
                              ),
                            );
                          }
                        },
                  style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF3B82F6)),
                  child: connecting
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                          ),
                        )
                      : const Text("Connect", style: TextStyle(color: Colors.white)),
                ),
              ],
            );
          },
        );
      },
    );
  }

  Widget _getPlatformIcon(String os, {double size = 28}) {
    switch (os.toLowerCase()) {
      case 'windows':
        return Icon(Icons.laptop_windows, color: Colors.blue, size: size);
      case 'linux':
        return Icon(Icons.computer, color: Colors.amber, size: size);
      case 'android':
        return Icon(Icons.phone_android, color: Colors.green, size: size);
      default:
        return Icon(Icons.devices, color: Colors.grey, size: size);
    }
  }

  @override
  Widget build(BuildContext context) {
    final state = Provider.of<AirBridgeState>(context);

    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [
              Color(0xFF0F172A),
              Color(0xFF1E1B4B),
              Color(0xFF0F172A),
            ],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
        ),
        child: SafeArea(
          child: Column(
            children: [
              // Header
              Padding(
                padding: const EdgeInsets.all(20.0),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          "AIRBRIDGE",
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 24,
                            fontWeight: FontWeight.w900,
                            letterSpacing: 1.5,
                          ),
                        ),
                        const SizedBox(height: 4),
                        Row(
                          children: [
                            Container(
                              width: 8,
                              height: 8,
                              decoration: BoxDecoration(
                                color: state.isRunning ? Colors.green : Colors.red,
                                shape: BoxShape.circle,
                              ),
                            ),
                            const SizedBox(width: 8),
                            Text(
                              state.isRunning
                                  ? "Active on ${state.localIp}:${AirBridgeState.tcpPort}"
                                  : "Services Stopped",
                              style: TextStyle(
                                color: Colors.white.withOpacity(0.6),
                                fontSize: 13,
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                    IconButton(
                      icon: Icon(
                        state.isRunning ? Icons.stop_circle : Icons.play_circle,
                        color: state.isRunning ? Colors.redAccent : Colors.greenAccent,
                        size: 32,
                      ),
                      onPressed: () {
                        if (state.isRunning) {
                          state.stopServices();
                        } else {
                          state.startServices();
                        }
                      },
                    ),
                  ],
                ),
              ),

              // Main Panels
              Expanded(
                child: ListView(
                  padding: const EdgeInsets.symmetric(horizontal: 20),
                  children: [
                    // Discovered peers card
                    _buildGlassCard(
                      title: "Discovered Devices",
                      action: IconButton(
                        icon: const Icon(Icons.add, color: Colors.blueAccent),
                        onPressed: () => _showManualConnectDialog(context),
                        tooltip: "Connect to Device by IP",
                      ),
                      child: state.peers.isEmpty
                          ? Padding(
                              padding: const EdgeInsets.symmetric(vertical: 24.0),
                              child: Center(
                                child: Column(
                                  children: [
                                    const SizedBox(
                                      width: 20,
                                      height: 20,
                                      child: CircularProgressIndicator(
                                        strokeWidth: 2,
                                        valueColor: AlwaysStoppedAnimation<Color>(Colors.white30),
                                      ),
                                    ),
                                    const SizedBox(height: 12),
                                    Text(
                                      "Scanning Wi-Fi network...",
                                      style: TextStyle(
                                        color: Colors.white.withOpacity(0.4),
                                        fontSize: 14,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                            )
                          : ListView.separated(
                              shrinkWrap: true,
                              physics: const NeverScrollableScrollPhysics(),
                              itemCount: state.peers.length,
                              separatorBuilder: (c, i) => Divider(color: Colors.white.withOpacity(0.1)),
                              itemBuilder: (context, index) {
                                final peer = state.peers[index];
                                return ListTile(
                                  contentPadding: EdgeInsets.zero,
                                  leading: _getPlatformIcon(peer.os),
                                  title: Text(
                                    peer.name,
                                    style: const TextStyle(
                                      color: Colors.white,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                  subtitle: Text(
                                    peer.ip,
                                    style: TextStyle(color: Colors.white.withOpacity(0.5)),
                                  ),
                                  trailing: const Icon(Icons.chevron_right, color: Colors.white30),
                                  onTap: () => _showDeviceActions(context, peer),
                                );
                              },
                            ),
                    ),
                    const SizedBox(height: 20),

                    // Active Transfers Card
                    if (state.transfers.isNotEmpty) ...[
                      _buildGlassCard(
                        title: "Active Transfers",
                        child: ListView.separated(
                          shrinkWrap: true,
                          physics: const NeverScrollableScrollPhysics(),
                          itemCount: state.transfers.length,
                          separatorBuilder: (c, i) => Divider(color: Colors.white.withOpacity(0.1)),
                          itemBuilder: (context, index) {
                            final tx = state.transfers[index];
                            final progress = tx.totalBytes == 0
                                ? 0.0
                                : tx.bytesTransferred / tx.totalBytes;
                            final percent = (progress * 100).toStringAsFixed(1);
                            
                            Color stateColor = Colors.blue;
                            String stateLabel = "${tx.speedMBps.toStringAsFixed(1)} MB/s";
                            if (tx.state == TransferState.completed) {
                              stateColor = Colors.green;
                              stateLabel = "Completed";
                            } else if (tx.state == TransferState.failed) {
                              stateColor = Colors.red;
                              stateLabel = "Failed";
                            }

                            return Padding(
                              padding: const EdgeInsets.symmetric(vertical: 8.0),
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Row(
                                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                    children: [
                                      Expanded(
                                        child: Text(
                                          tx.filename,
                                          style: const TextStyle(
                                            color: Colors.white,
                                            fontWeight: FontWeight.w600,
                                          ),
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                        ),
                                      ),
                                      Icon(
                                        tx.direction == TransferDirection.send
                                            ? Icons.arrow_upward
                                            : Icons.arrow_downward,
                                        color: Colors.white30,
                                        size: 16,
                                      ),
                                    ],
                                  ),
                                  const SizedBox(height: 6),
                                  LinearProgressIndicator(
                                    value: progress,
                                    backgroundColor: Colors.white.withOpacity(0.1),
                                    valueColor: AlwaysStoppedAnimation<Color>(stateColor),
                                    borderRadius: BorderRadius.circular(4),
                                  ),
                                  const SizedBox(height: 6),
                                  Row(
                                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                    children: [
                                      Text(
                                        "$percent% • ${(tx.bytesTransferred / (1024 * 1024)).toStringAsFixed(1)} / ${(tx.totalBytes / (1024 * 1024)).toStringAsFixed(1)} MB",
                                        style: TextStyle(
                                          color: Colors.white.withOpacity(0.5),
                                          fontSize: 12,
                                        ),
                                      ),
                                      Text(
                                        stateLabel,
                                        style: TextStyle(
                                          color: stateColor,
                                          fontSize: 12,
                                          fontWeight: FontWeight.bold,
                                        ),
                                      ),
                                    ],
                                  ),
                                ],
                              ),
                            );
                          },
                        ),
                      ),
                      const SizedBox(height: 20),
                    ],

                    // Clipboard Sync Card
                    _buildGlassCard(
                      title: "Received Clipboard Text",
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          if (state.lastReceivedClip.isEmpty)
                            Padding(
                              padding: const EdgeInsets.symmetric(vertical: 12.0),
                              child: Text(
                                "No text received yet. Send something from your Windows/Linux C++ app to sync clipboards instantly.",
                                style: TextStyle(
                                  color: Colors.white.withOpacity(0.4),
                                  fontSize: 13,
                                  fontStyle: FontStyle.italic,
                                ),
                              ),
                            )
                          else Container(
                              padding: const EdgeInsets.all(12),
                              decoration: BoxDecoration(
                                color: Colors.white.withOpacity(0.05),
                                borderRadius: BorderRadius.circular(10),
                                border: Border.all(color: Colors.white.withOpacity(0.1)),
                              ),
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  SelectableText(
                                    state.lastReceivedClip,
                                    style: const TextStyle(
                                      color: Colors.white,
                                      fontSize: 14,
                                      fontFamily: 'monospace',
                                    ),
                                  ),
                                  const SizedBox(height: 8),
                                  Align(
                                    alignment: Alignment.bottomRight,
                                    child: InkWell(
                                      onTap: () {
                                        ScaffoldMessenger.of(context).showSnackBar(
                                          const SnackBar(
                                            content: Text("Copied to system clipboard"),
                                            duration: Duration(seconds: 1),
                                          ),
                                        );
                                      },
                                      child: Row(
                                        mainAxisSize: MainAxisSize.min,
                                        children: [
                                          Icon(
                                            Icons.check_circle_outline,
                                            color: Colors.greenAccent.withOpacity(0.8),
                                            size: 16,
                                          ),
                                          const SizedBox(width: 4),
                                          Text(
                                            "Synced",
                                            style: TextStyle(
                                              color: Colors.greenAccent.withOpacity(0.8),
                                              fontSize: 12,
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          if (state.clipboardHistory.length > 1) ...[
                            const SizedBox(height: 16),
                            const Text(
                              "History",
                              style: TextStyle(
                                color: Colors.white70,
                                fontSize: 13,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            const SizedBox(height: 8),
                            ...state.clipboardHistory.reversed.skip(1).map((clip) {
                              String display = clip.length > 60 ? "${clip.substring(0, 57)}..." : clip;
                              return Padding(
                                padding: const EdgeInsets.symmetric(vertical: 4.0),
                                child: InkWell(
                                  onTap: () {
                                    Clipboard.setData(ClipboardData(text: clip));
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      const SnackBar(
                                        content: Text("Copied historical item"),
                                        duration: Duration(seconds: 1),
                                      ),
                                    );
                                  },
                                  child: Row(
                                    children: [
                                      const Icon(Icons.history, color: Colors.white30, size: 16),
                                      const SizedBox(width: 8),
                                      Expanded(
                                        child: Text(
                                          display,
                                          style: TextStyle(
                                            color: Colors.white.withOpacity(0.5),
                                            fontSize: 12,
                                          ),
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              );
                            }).toList(),
                          ],
                        ],
                      ),
                    ),
                    const SizedBox(height: 40),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildGlassCard({required String title, Widget? action, required Widget child}) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white.withOpacity(0.04),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(
          color: Colors.white.withOpacity(0.08),
          width: 1,
        ),
      ),
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                title,
                style: const TextStyle(
                  color: Colors.white,
                  fontSize: 16,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 0.5,
                ),
              ),
              if (action != null) action,
            ],
          ),
          const SizedBox(height: 16),
          child,
        ],
      ),
    );
  }
}
