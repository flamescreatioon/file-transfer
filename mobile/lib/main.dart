import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/airbridge_state.dart';
import 'views/home_view.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(
    ChangeNotifierProvider(
      create: (_) => AirBridgeState(),
      child: const AirBridgeApp(),
    ),
  );
}

class AirBridgeApp extends StatelessWidget {
  const AirBridgeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AirBridge',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        primarySwatch: Colors.blue,
        fontFamily: 'Inter', // Fallback to system sans-serif if not loaded
        scaffoldBackgroundColor: const Color(0xFF0F172A),
      ),
      home: const HomeView(),
    );
  }
}
