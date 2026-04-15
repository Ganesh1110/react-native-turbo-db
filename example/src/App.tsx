import { useState, useMemo } from 'react';
import {
  SafeAreaView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
  ScrollView,
  ActivityIndicator,
} from 'react-native';
import { SecureDB } from 'react-native-secure-db';
// @ts-ignore - Assuming MMKV is installed in the project environment
import { MMKV } from 'react-native-mmkv';

const ITEM_COUNT = 5000;

interface BenchResult {
  write: number;
  read: number;
}

export default function App() {
  const [isRunning, setIsRunning] = useState(false);
  const [secureDBResults, setSecureDBResults] = useState<BenchResult | null>(
    null
  );
  const [mmkvResults, setMmkvResults] = useState<BenchResult | null>(null);

  const db = useMemo(() => {
    const docPath = SecureDB.getDocumentsDirectory();
    const dbFile = `${docPath}/benchmark.db`;
    return new SecureDB(dbFile, 50 * 1024 * 1024); // 50MB for heavy bench
  }, []);

  const mmkv = useMemo(() => {
    try {
      return new MMKV();
    } catch {
      console.warn('MMKV not available, skipping that part of the benchmark');
      return null;
    }
  }, []);

  const runBenchmark = async () => {
    setIsRunning(true);
    setSecureDBResults(null);
    setMmkvResults(null);

    await new Promise((resolve) => setTimeout(resolve, 500));

    const testData = {
      id: 'user_' + Math.random(),
      name: 'Benchmark User',
      balance: 1234.56,
      metadata: { role: 'admin', level: 99 },
      tags: ['jsi', 'secure', 'fast', 'database', 'mobile'],
    };

    // --- SecureDB Benchmark ---
    try {
      db.clear();
      const sWriteStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        db.set(`key_${i}`, testData);
      }
      const sWriteEnd = performance.now();

      const sReadStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        db.get(`key_${i}`);
      }
      const sReadEnd = performance.now();

      setSecureDBResults({
        write: sWriteEnd - sWriteStart,
        read: sReadEnd - sReadStart,
      });
    } catch (e) {
      console.error('SecureDB Bench Error:', e);
    }

    // --- MMKV Benchmark ---
    if (mmkv) {
      try {
        mmkv.clearAll();
        const jsonStr = JSON.stringify(testData);

        const mWriteStart = performance.now();
        for (let i = 0; i < ITEM_COUNT; i++) {
          mmkv.set(`key_${i}`, jsonStr);
        }
        const mWriteEnd = performance.now();

        const mReadStart = performance.now();
        for (let i = 0; i < ITEM_COUNT; i++) {
          const val = mmkv.getString(`key_${i}`);
          if (val) JSON.parse(val);
        }
        const mReadEnd = performance.now();

        setMmkvResults({
          write: mWriteEnd - mWriteStart,
          read: mReadEnd - mReadStart,
        });
      } catch (e) {
        console.error('MMKV Bench Error:', e);
      }
    }

    setIsRunning(false);
  };

  const renderStatRow = (
    label: string,
    secureVal?: number,
    mmkvVal?: number
  ) => (
    <View style={styles.statRow}>
      <Text style={styles.statRowLabel}>{label}</Text>
      <View style={styles.statValues}>
        <View style={styles.valBox}>
          <Text style={styles.valText}>
            {secureVal ? `${secureVal.toFixed(1)}ms` : '--'}
          </Text>
          <Text style={styles.valSubText}>SecureDB</Text>
        </View>
        <View style={styles.valBox}>
          <Text style={[styles.valText, styles.mmkvColor]}>
            {mmkvVal ? `${mmkvVal.toFixed(1)}ms` : '--'}
          </Text>
          <Text style={styles.valSubText}>MMKV</Text>
        </View>
      </View>
    </View>
  );

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.scrollContent}>
        <View style={styles.header}>
          <Text style={styles.title}>Storage Duel</Text>
          <Text style={styles.subtitle}>
            SecureDB vs MMKV ({ITEM_COUNT} ops)
          </Text>
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Performance Comparison</Text>
          {renderStatRow(
            'Bulk Write',
            secureDBResults?.write,
            mmkvResults?.write
          )}
          <View style={styles.divider} />
          {renderStatRow('Bulk Read', secureDBResults?.read, mmkvResults?.read)}
        </View>

        {secureDBResults && mmkvResults && (
          <View style={styles.winCard}>
            <Text style={styles.winText}>
              {secureDBResults.write < mmkvResults.write
                ? 'SecureDB is faster at Writing! 🔥'
                : 'MMKV is faster at Writing!'}
            </Text>
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
            <Text style={styles.buttonText}>Run Head-to-Head</Text>
          )}
        </TouchableOpacity>

        <View style={styles.infoBox}>
          <Text style={styles.infoTitle}>Why this benchmark matters?</Text>
          <Text style={styles.infoText}>
            • SecureDB uses Page-Level AES-256-GCM Hardware encryption.
          </Text>
          <Text style={styles.infoText}>
            • MMKV is an ultra-fast Key-Value store using protobuf.
          </Text>
          <Text style={styles.infoText}>
            • This test includes JSON serialization/deserialization overhead for
            both.
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#020617' },
  scrollContent: { padding: 24 },
  header: { alignItems: 'center', marginVertical: 40 },
  title: { fontSize: 40, fontWeight: '900', color: '#F8FAFC' },
  subtitle: { fontSize: 16, color: '#94A3B8', marginTop: 8 },
  card: {
    backgroundColor: '#0F172A',
    borderRadius: 24,
    padding: 24,
    borderWidth: 1,
    borderColor: '#1E293B',
    marginBottom: 24,
  },
  cardTitle: {
    color: '#F8FAFC',
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 24,
  },
  statRow: { marginVertical: 12 },
  statRowLabel: {
    color: '#94A3B8',
    fontSize: 14,
    textTransform: 'uppercase',
    marginBottom: 12,
  },
  statValues: { flexDirection: 'row', justifyContent: 'space-between' },
  valBox: { flex: 1 },
  valText: { color: '#38BDF8', fontSize: 24, fontWeight: 'bold' },
  mmkvColor: { color: '#F472B6' },
  valSubText: { color: '#475569', fontSize: 12, marginTop: 2 },
  divider: { height: 1, backgroundColor: '#1E293B', marginVertical: 16 },
  button: {
    backgroundColor: '#F8FAFC',
    paddingVertical: 18,
    borderRadius: 16,
    alignItems: 'center',
    shadowColor: '#fff',
    shadowOpacity: 0.1,
    shadowRadius: 20,
    elevation: 10,
  },
  buttonDisabled: { opacity: 0.5 },
  buttonText: { color: '#020617', fontSize: 18, fontWeight: 'bold' },
  winCard: {
    backgroundColor: 'rgba(56, 189, 248, 0.1)',
    padding: 16,
    borderRadius: 12,
    marginBottom: 24,
    alignItems: 'center',
    borderWidth: 1,
    borderColor: 'rgba(56, 189, 248, 0.3)',
  },
  winText: { color: '#38BDF8', fontWeight: 'bold', fontSize: 16 },
  infoBox: {
    marginTop: 40,
    padding: 20,
    backgroundColor: '#0F172A',
    borderRadius: 16,
  },
  infoTitle: {
    color: '#F8FAFC',
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 12,
  },
  infoText: { color: '#64748B', fontSize: 14, lineHeight: 22, marginBottom: 8 },
});
