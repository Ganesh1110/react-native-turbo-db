import { useState, useMemo, useEffect } from 'react';
import {
  SafeAreaView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
  ScrollView,
  ActivityIndicator,
  Alert,
} from 'react-native';
import { SecureDB } from 'react-native-secure-db';
// @ts-ignore - Assuming MMKV is installed in the project environment
import { MMKV } from 'react-native-mmkv';

const ITEM_COUNT = 1000; // Reduced for smoother UI during mixed tests

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
  const [dbPath, setDbPath] = useState('');
  const [allKeys, setAllKeys] = useState<string[]>([]);
  const [rangeResults, setRangeResults] = useState<any[]>([]);

  const db = useMemo(() => {
    const docPath = SecureDB.getDocumentsDirectory();
    const dbFile = `${docPath}/secure_v1.db`;
    return new SecureDB(dbFile, 10 * 1024 * 1024); // 10MB
  }, []);

  const mmkv = useMemo(() => {
    try {
      return new MMKV();
    } catch {
      console.warn('MMKV not available, skipping that part of the benchmark');
      return null;
    }
  }, []);

  useEffect(() => {
    setDbPath(SecureDB.getDocumentsDirectory());
    refreshKeys();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const refreshKeys = () => {
    try {
      const keys = db.getAllKeys();
      setAllKeys(keys);
    } catch (e) {
      console.error('Refresh Keys Error:', e);
    }
  };

  const runBenchmark = async () => {
    setIsRunning(true);
    setSecureDBResults(null);
    setMmkvResults(null);

    await new Promise((resolve) => setTimeout(resolve, 500));

    const testData = {
      id: 'user_' + Math.random().toString(36).substr(2, 9),
      name: 'Benchmark User',
      balance: 1234.56,
      metadata: { role: 'admin', level: 99 },
      tags: ['jsi', 'secure', 'fast', 'sodium'],
    };

    // --- SecureDB Benchmark ---
    try {
      console.log('Starting SecureDB clear...');
      db.clear();
      console.log('SecureDB clear done.');

      const sWriteStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        if (i % 100 === 0) console.log(`SecureDB writing: ${i}/${ITEM_COUNT}`);
        db.set(`key_${i.toString().padStart(4, '0')}`, testData);
      }
      console.log('SecureDB flushing...');
      db.flush();
      const sWriteEnd = performance.now();
      console.log('SecureDB write benchmark done.');

      const sReadStart = performance.now();
      for (let i = 0; i < ITEM_COUNT; i++) {
        if (i % 100 === 0) console.log(`SecureDB reading: ${i}/${ITEM_COUNT}`);
        db.get(`key_${i.toString().padStart(4, '0')}`);
      }
      const sReadEnd = performance.now();
      console.log('SecureDB read benchmark done.');
      setSecureDBResults({
        write: sWriteEnd - sWriteStart,
        read: sReadEnd - sReadStart,
      });
      refreshKeys();
    } catch (e) {
      console.error('SecureDB Bench Error:', e);
      Alert.alert('SecureDB Error', String(e));
    }

    // --- MMKV Benchmark ---
    if (mmkv) {
      try {
        mmkv.clearAll();
        const jsonStr = JSON.stringify(testData);

        const mWriteStart = performance.now();
        for (let i = 0; i < ITEM_COUNT; i++) {
          mmkv.set(`key_${i.toString().padStart(4, '0')}`, jsonStr);
        }
        const mWriteEnd = performance.now();

        const mReadStart = performance.now();
        for (let i = 0; i < ITEM_COUNT; i++) {
          const val = mmkv.getString(`key_${i.toString().padStart(4, '0')}`);
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

  const testRangeQuery = () => {
    try {
      // Query keys between key_0010 and key_0020
      const results = db.rangeQuery('key_0010', 'key_0020');
      setRangeResults(results);
      Alert.alert(
        'Range Query',
        `Found ${results.length} items in range [key_0010, key_0020]`
      );
    } catch (e) {
      console.error('Range Query Error:', e);
    }
  };

  const handleFlush = () => {
    db.flush();
    Alert.alert('Success', 'Write buffer flushed to disk tree.');
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
          <Text style={styles.title}>SecureDB</Text>
          <Text style={styles.subtitle}>
            JSI + Libsodium + MMAP ({ITEM_COUNT} ops)
          </Text>
        </View>

        <View style={styles.metaCard}>
          <Text style={styles.metaLabel}>Storage Path:</Text>
          <Text
            style={styles.metaValue}
            numberOfLines={1}
            ellipsizeMode="middle"
          >
            {dbPath || 'Loading...'}
          </Text>
          <View style={{ height: 8 }} />
          <Text style={styles.metaLabel}>Total Keys:</Text>
          <Text style={styles.metaValue}>{allKeys.length}</Text>
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

        <View style={styles.actionRow}>
          <TouchableOpacity
            style={[
              styles.button,
              isRunning && styles.buttonDisabled,
              { flex: 2 },
            ]}
            onPress={runBenchmark}
            disabled={isRunning}
          >
            {isRunning ? (
              <ActivityIndicator color="#020617" />
            ) : (
              <Text style={styles.buttonText}>Run Benchmark</Text>
            )}
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.secondaryButton, { flex: 1, marginLeft: 12 }]}
            onPress={handleFlush}
          >
            <Text style={styles.secondaryButtonText}>Flush</Text>
          </TouchableOpacity>
        </View>

        <View style={styles.actionRow}>
          <TouchableOpacity
            style={styles.outlineButton}
            onPress={testRangeQuery}
          >
            <Text style={styles.outlineButtonText}>Test Range Query</Text>
          </TouchableOpacity>

          <TouchableOpacity
            style={[styles.outlineButton, { marginLeft: 12 }]}
            onPress={refreshKeys}
          >
            <Text style={styles.outlineButtonText}>Refresh Keys</Text>
          </TouchableOpacity>
        </View>

        {rangeResults.length > 0 && (
          <View style={styles.rangeCard}>
            <Text style={styles.cardTitle}>Range Query Snippet</Text>
            {rangeResults.slice(0, 3).map((item, i) => (
              <Text key={i} style={styles.rangeText}>
                {item.key}: {JSON.stringify(item.value).substring(0, 40)}...
              </Text>
            ))}
          </View>
        )}

        <View style={styles.infoBox}>
          <Text style={styles.infoTitle}>Architecture Highlights</Text>
          <Text style={styles.infoText}>
            •{' '}
            <Text style={{ fontWeight: 'bold', color: '#fff' }}>
              Sodium XChaCha20-Poly1305:
            </Text>{' '}
            AEAD encryption at the block level.
          </Text>
          <Text style={styles.infoText}>
            •{' '}
            <Text style={{ fontWeight: 'bold', color: '#fff' }}>
              Page-Level WAL:
            </Text>{' '}
            Write Ahead Logging ensures atomicity even on crash.
          </Text>
          <Text style={styles.infoText}>
            •{' '}
            <Text style={{ fontWeight: 'bold', color: '#fff' }}>
              Hybrid B+ Tree:
            </Text>{' '}
            Buffered writes with disk-persisted B+ Tree indexing.
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#020617' },
  scrollContent: { padding: 24 },
  header: { alignItems: 'center', marginVertical: 30 },
  title: {
    fontSize: 42,
    fontWeight: '900',
    color: '#F8FAFC',
    letterSpacing: -1,
  },
  subtitle: {
    fontSize: 14,
    color: '#64748B',
    marginTop: 4,
    textTransform: 'uppercase',
  },

  metaCard: {
    backgroundColor: '#0F172A',
    borderRadius: 16,
    padding: 16,
    marginBottom: 20,
    borderLeftWidth: 4,
    borderLeftColor: '#38BDF8',
  },
  metaLabel: {
    color: '#475569',
    fontSize: 11,
    fontWeight: 'bold',
    textTransform: 'uppercase',
  },
  metaValue: { color: '#94A3B8', fontSize: 13, marginTop: 2 },

  card: {
    backgroundColor: '#0F172A',
    borderRadius: 24,
    padding: 24,
    borderWidth: 1,
    borderColor: '#1E293B',
    marginBottom: 20,
  },
  cardTitle: {
    color: '#F8FAFC',
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 20,
  },
  statRow: { marginVertical: 8 },
  statRowLabel: {
    color: '#475569',
    fontSize: 12,
    fontWeight: 'bold',
    marginBottom: 10,
  },
  statValues: { flexDirection: 'row', justifyContent: 'space-between' },
  valBox: { flex: 1 },
  valText: { color: '#38BDF8', fontSize: 22, fontWeight: 'bold' },
  mmkvColor: { color: '#F472B6' },
  valSubText: { color: '#475569', fontSize: 11, marginTop: 2 },
  divider: { height: 1, backgroundColor: '#1E293B', marginVertical: 12 },

  actionRow: { flexDirection: 'row', marginBottom: 12 },
  button: {
    backgroundColor: '#F8FAFC',
    paddingVertical: 16,
    borderRadius: 12,
    alignItems: 'center',
    justifyContent: 'center',
  },
  buttonDisabled: { opacity: 0.5 },
  buttonText: { color: '#020617', fontSize: 16, fontWeight: 'bold' },

  secondaryButton: {
    backgroundColor: '#1E293B',
    paddingVertical: 16,
    borderRadius: 12,
    alignItems: 'center',
    justifyContent: 'center',
  },
  secondaryButtonText: { color: '#F8FAFC', fontSize: 16, fontWeight: 'bold' },

  outlineButton: {
    flex: 1,
    borderWidth: 1,
    borderColor: '#334155',
    paddingVertical: 12,
    borderRadius: 12,
    alignItems: 'center',
  },
  outlineButtonText: { color: '#94A3B8', fontSize: 14, fontWeight: '500' },

  rangeCard: {
    backgroundColor: 'rgba(56, 189, 248, 0.05)',
    padding: 20,
    borderRadius: 16,
    marginBottom: 20,
    borderWidth: 1,
    borderColor: 'rgba(56, 189, 248, 0.1)',
  },
  rangeText: {
    color: '#38BDF8',
    fontSize: 12,
    fontFamily: 'monospace',
    marginBottom: 6,
  },

  infoBox: {
    marginTop: 20,
    padding: 20,
    backgroundColor: '#0F172A',
    borderRadius: 16,
  },
  infoTitle: {
    color: '#F8FAFC',
    fontSize: 15,
    fontWeight: 'bold',
    marginBottom: 12,
  },
  infoText: {
    color: '#64748B',
    fontSize: 13,
    lineHeight: 20,
    marginBottom: 10,
  },
});
