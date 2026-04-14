import React, { useState, useCallback } from 'react';
import {
  SafeAreaView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
  ScrollView,
  NativeModules,
  ActivityIndicator,
} from 'react-native';
import SecureDB from 'react-native-secure-db';

declare const global: { NativeDB: any };

const ITEM_COUNT = 1000;

export default function App() {
  const [isRunning, setIsRunning] = useState(false);
  const [results, setResults] = useState<{
    write: string;
    read: string;
    throughput: string;
  } | null>(null);

  const initDB = useCallback(() => {
    SecureDB.install();
    const docPath = NativeModules.SecureDBInstaller?.getDocumentDirectory() || '/tmp';
    const dbFile = `${docPath}/benchmark.db`;
    // 10MB allocation for heavy benchmark
    global.NativeDB.initStorage(dbFile, 10 * 1024 * 1024);
  }, []);

  const runBenchmark = async () => {
    setIsRunning(true);
    setResults(null);

    // Give UI thread a breath
    await new Promise((resolve) => setTimeout(resolve, 100));

    try {
      initDB();
      global.NativeDB.clearStorage();

      const testData = {
        id: "bench_user_123",
        name: "Benchmark User",
        metadata: { role: "admin", level: 99 },
        tags: ["jsi", "secure", "fast"]
      };

      // 1. Bulk Write Benchmark
      const writeStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        global.NativeDB.insertRec(`key_${i}`, testData);
      }
      const writeEnd = performance.now();
      const writeTime = writeEnd - writeStart;

      // 2. Bulk Read Benchmark
      const readStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        global.NativeDB.findRec(`key_${i}`);
      }
      const readEnd = performance.now();
      const readTime = readEnd - readStart;

      const totalOps = ITEM_COUNT * 2;
      const totalTimeS = (writeTime + readTime) / 1000;
      const throughput = (totalOps / totalTimeS).toFixed(0);

      setResults({
        write: `${writeTime.toFixed(2)}ms`,
        read: `${readTime.toFixed(2)}ms`,
        throughput: `${throughput} ops/sec`,
      });
    } catch (e: any) {
      console.error(e);
    } finally {
      setIsRunning(false);
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.scrollContent}>
        <View style={styles.header}>
          <Text style={styles.title}>SecureDB</Text>
          <Text style={styles.subtitle}>JSI Storage Benchmark</Text>
        </View>

        <View style={styles.statsContainer}>
          <View style={styles.statCard}>
            <Text style={styles.statLabel}>Bulk Write (1k)</Text>
            {results ? (
              <Text style={styles.statValue}>{results.write}</Text>
            ) : (
              <Text style={styles.statPlaceholder}>--</Text>
            )}
          </View>

          <View style={styles.statCard}>
            <Text style={styles.statLabel}>Bulk Read (1k)</Text>
            {results ? (
              <Text style={styles.statValue}>{results.read}</Text>
            ) : (
              <Text style={styles.statPlaceholder}>--</Text>
            )}
          </View>
        </View>

        {results && (
          <View style={styles.throughputCard}>
            <Text style={styles.throughputLabel}>Total Throughput</Text>
            <Text style={styles.throughputValue}>{results.throughput}</Text>
          </View>
        )}

        <TouchableOpacity 
          style={[styles.button, isRunning && styles.buttonDisabled]} 
          onPress={runBenchmark}
          disabled={isRunning}
        >
          {isRunning ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.buttonText}>Start Benchmark</Text>
          )}
        </TouchableOpacity>

        <View style={styles.metaInfo}>
          <Text style={styles.metaTitle}>Benchmark Methodology</Text>
          <Text style={styles.metaText}>• Sequential JSI synchronous calls</Text>
          <Text style={styles.metaText}>• Binary serialization via Msgpack</Text>
          <Text style={styles.metaText}>• C++ B+ Tree index traversal</Text>
          <Text style={styles.metaText}>• Hardware-backed AES-256-GCM context</Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0F172A', // Deep slate
  },
  scrollContent: {
    padding: 24,
    alignItems: 'center',
  },
  header: {
    marginTop: 40,
    marginBottom: 48,
    alignItems: 'center',
  },
  title: {
    fontSize: 42,
    fontWeight: '900',
    color: '#38BDF8', // Cyan 400
    letterSpacing: -1,
  },
  subtitle: {
    fontSize: 18,
    color: '#94A3B8', // Slate 400
    marginTop: 4,
  },
  statsContainer: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    width: '100%',
    marginBottom: 20,
  },
  statCard: {
    backgroundColor: '#1E293B',
    borderRadius: 16,
    padding: 20,
    width: '48%',
    borderWidth: 1,
    borderColor: '#334155',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 8,
    elevation: 5,
  },
  statLabel: {
    fontSize: 12,
    color: '#94A3B8',
    textTransform: 'uppercase',
    letterSpacing: 1,
    marginBottom: 8,
  },
  statValue: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#F8FAFC',
  },
  statPlaceholder: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#475569',
  },
  throughputCard: {
    backgroundColor: '#0F172A',
    borderColor: '#38BDF8',
    borderWidth: 1,
    borderRadius: 16,
    padding: 24,
    width: '100%',
    alignItems: 'center',
    marginBottom: 32,
  },
  throughputLabel: {
    fontSize: 14,
    color: '#38BDF8',
    textTransform: 'uppercase',
    letterSpacing: 2,
    marginBottom: 4,
  },
  throughputValue: {
    fontSize: 32,
    fontWeight: '900',
    color: '#F8FAFC',
  },
  button: {
    backgroundColor: '#38BDF8',
    paddingVertical: 18,
    paddingHorizontal: 40,
    borderRadius: 99,
    width: '100%',
    alignItems: 'center',
    shadowColor: '#38BDF8',
    shadowOffset: { width: 0, height: 8 },
    shadowOpacity: 0.4,
    shadowRadius: 12,
    elevation: 8,
  },
  buttonDisabled: {
    backgroundColor: '#1E293B',
    shadowOpacity: 0,
  },
  buttonText: {
    color: '#0F172A',
    fontSize: 18,
    fontWeight: 'bold',
  },
  metaInfo: {
    marginTop: 48,
    width: '100%',
    padding: 24,
    backgroundColor: 'rgba(30, 41, 59, 0.5)',
    borderRadius: 16,
  },
  metaTitle: {
    color: '#F8FAFC',
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 16,
  },
  metaText: {
    color: '#94A3B8',
    fontSize: 14,
    lineHeight: 22,
    marginBottom: 4,
  },
});