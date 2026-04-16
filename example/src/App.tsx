import { useState, useMemo, useEffect } from 'react';
import {
  SafeAreaView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
  ScrollView,
  TextInput,
  Alert,
} from 'react-native';
import { SecureDB } from 'react-native-secure-db';

export default function App() {
  const [key, setKey] = useState('');
  const [value, setValue] = useState('');
  const [dbPath, setDbPath] = useState('');
  const [allKeys, setAllKeys] = useState<string[]>([]);
  const [getResult, setGetResult] = useState<string>('');

  const db = useMemo(() => {
    const docPath = SecureDB.getDocumentsDirectory();
    const dbFile = `${docPath}/secure_v1.db`;
    return new SecureDB(dbFile, 10 * 1024 * 1024); // 10MB
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

  const handleSet = () => {
    if (!key) {
      Alert.alert('Error', 'Please enter a key');
      return;
    }
    try {
      // We can store objects, strings, numbers, etc.
      // If value looks like JSON, parse it, else store as string
      let dataToStore: any = value;
      try {
        dataToStore = JSON.parse(value);
      } catch {
        dataToStore = value;
      }

      const success = db.set(key, dataToStore);
      if (success) {
        Alert.alert('Success', `Stored key: ${key}`);
        setKey('');
        setValue('');
        refreshKeys();
      } else {
        Alert.alert('Error', 'Failed to store value');
      }
    } catch (e) {
      Alert.alert('Error', String(e));
    }
  };

  const handleGet = () => {
    if (!key) {
      Alert.alert('Error', 'Please enter a key to retrieve');
      return;
    }
    try {
      const result = db.get(key);
      if (result === undefined) {
        setGetResult('Key not found');
      } else {
        setGetResult(JSON.stringify(result, null, 2));
      }
    } catch (e) {
      Alert.alert('Error', String(e));
    }
  };

  const handleDelete = () => {
    if (!key) {
      Alert.alert('Error', 'Please enter a key to delete');
      return;
    }
    try {
      const success = db.remove(key);
      if (success) {
        Alert.alert('Success', `Deleted key: ${key}`);
        setKey('');
        setGetResult('');
        refreshKeys();
      } else {
        Alert.alert('Error', 'Key not found or failed to delete');
      }
    } catch (e) {
      Alert.alert('Error', String(e));
    }
  };

  const handleClearAll = () => {
    Alert.alert('Confirm', 'Are you sure you want to delete everything?', [
      { text: 'Cancel', style: 'cancel' },
      {
        text: 'Delete All',
        style: 'destructive',
        onPress: () => {
          try {
            db.clear();
            setAllKeys([]);
            setGetResult('');
            Alert.alert('Success', 'Database cleared');
          } catch (e) {
            Alert.alert('Error', String(e));
          }
        },
      },
    ]);
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.scrollContent}>
        <View style={styles.header}>
          <Text style={styles.title}>SecureDB</Text>
          <Text style={styles.subtitle}>Native JSI CRUD Demo</Text>
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
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Operations</Text>

          <Text style={styles.inputLabel}>Key</Text>
          <TextInput
            style={styles.input}
            value={key}
            onChangeText={setKey}
            placeholder="Enter key..."
            placeholderTextColor="#475569"
          />

          <Text style={styles.inputLabel}>Value (String or JSON)</Text>
          <TextInput
            style={[styles.input, { height: 80 }]}
            value={value}
            onChangeText={setValue}
            placeholder='e.g. "Hello" or {"id": 1}'
            placeholderTextColor="#475569"
            multiline
          />

          <View style={styles.buttonRow}>
            <TouchableOpacity style={styles.button} onPress={handleSet}>
              <Text style={styles.buttonText}>Set</Text>
            </TouchableOpacity>

            <TouchableOpacity
              style={[styles.button, { backgroundColor: '#38BDF8' }]}
              onPress={handleGet}
            >
              <Text style={[styles.buttonText, { color: '#fff' }]}>Get</Text>
            </TouchableOpacity>

            <TouchableOpacity
              style={[styles.button, { backgroundColor: '#EF4444' }]}
              onPress={handleDelete}
            >
              <Text style={[styles.buttonText, { color: '#fff' }]}>Delete</Text>
            </TouchableOpacity>
          </View>

          <TouchableOpacity
            style={[
              styles.outlineButton,
              { marginTop: 12, borderColor: '#EF4444' },
            ]}
            onPress={handleClearAll}
          >
            <Text style={[styles.outlineButtonText, { color: '#EF4444' }]}>
              Clear Entire Database
            </Text>
          </TouchableOpacity>
        </View>

        {getResult !== '' && (
          <View style={styles.resultCard}>
            <Text style={styles.cardTitle}>Get Result</Text>
            <Text style={styles.resultText}>{getResult}</Text>
          </View>
        )}

        <View style={styles.card}>
          <View style={styles.rowBetween}>
            <Text style={styles.cardTitle}>All Keys ({allKeys.length})</Text>
            <TouchableOpacity onPress={refreshKeys}>
              <Text style={styles.refreshText}>Refresh</Text>
            </TouchableOpacity>
          </View>

          <View style={styles.keyList}>
            {allKeys.length === 0 ? (
              <Text style={styles.emptyText}>No keys stored yet.</Text>
            ) : (
              allKeys.map((k) => (
                <TouchableOpacity
                  key={k}
                  onPress={() => setKey(k)}
                  style={styles.keyItem}
                >
                  <Text style={styles.keyText}>• {k}</Text>
                </TouchableOpacity>
              ))
            )}
          </View>
        </View>

        <View style={styles.infoBox}>
          <Text style={styles.infoTitle}>Architecture Summary</Text>
          <Text style={styles.infoText}>
            • <Text style={styles.boldWhite}>XChaCha20-Poly1305:</Text>{' '}
            Authenticated encryption for all data blocks.
          </Text>
          <Text style={styles.infoText}>
            • <Text style={styles.boldWhite}>Memory-Mapped (MMAP):</Text> Direct
            kernel-level file access for high performance.
          </Text>
          <Text style={styles.infoText}>
            • <Text style={styles.boldWhite}>Zero-Copy JSI:</Text> Synchronous
            native communication without bridge overhead.
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
    marginBottom: 16,
  },

  inputLabel: {
    color: '#94A3B8',
    fontSize: 12,
    marginBottom: 8,
    fontWeight: '600',
  },
  input: {
    backgroundColor: '#020617',
    borderRadius: 12,
    padding: 12,
    color: '#F8FAFC',
    fontSize: 16,
    marginBottom: 16,
    borderWidth: 1,
    borderColor: '#334155',
  },

  buttonRow: { flexDirection: 'row', justifyContent: 'space-between' },
  button: {
    flex: 1,
    backgroundColor: '#F8FAFC',
    paddingVertical: 12,
    borderRadius: 10,
    alignItems: 'center',
    marginHorizontal: 4,
  },
  buttonText: { color: '#020617', fontSize: 14, fontWeight: 'bold' },

  outlineButton: {
    paddingVertical: 12,
    borderRadius: 10,
    alignItems: 'center',
    borderWidth: 1,
  },
  outlineButtonText: { fontSize: 14, fontWeight: 'bold' },

  resultCard: {
    backgroundColor: 'rgba(56, 189, 248, 0.05)',
    padding: 20,
    borderRadius: 16,
    marginBottom: 20,
    borderWidth: 1,
    borderColor: 'rgba(56, 189, 248, 0.1)',
  },
  resultText: { color: '#38BDF8', fontSize: 14, fontFamily: 'monospace' },

  rowBetween: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 12,
  },
  refreshText: { color: '#38BDF8', fontSize: 14, fontWeight: 'bold' },

  keyList: { marginTop: 8 },
  keyItem: {
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#1E293B',
  },
  keyText: { color: '#94A3B8', fontSize: 14 },
  emptyText: {
    color: '#475569',
    fontSize: 14,
    fontStyle: 'italic',
    textAlign: 'center',
  },

  infoBox: {
    marginTop: 10,
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
  boldWhite: { fontWeight: 'bold', color: '#fff' },
});
