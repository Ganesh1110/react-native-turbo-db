import React, { useState } from 'react';
import { Text, View, StyleSheet, Button, NativeModules } from 'react-native';
import SecureDB from 'react-native-secure-db';

declare const global: { NativeDB: any; };

export default function App() {
  const [result, setResult] = useState<string>('Ready.');
  const [readResult, setReadResult] = useState<string>('');

  const runPhase4Tests = () => {
    try {
      SecureDB.install();
      const docPath = NativeModules.SecureDBInstaller?.getDocumentDirectory() || '/tmp';
      const dbFile = `${docPath}/test_phase4.db`;
      
      const initStart = performance.now();
      // Phase 4 requires more capacity to store the B-Tree Nodes alongside standard payloads.
      // 1MB pre-allocation ensures the B+ Tree can layout natively without expanding limits.
      const success = global.NativeDB.initStorage(dbFile, 1024 * 1024);
      const initEnd = performance.now();
      
      if (!success) {
        setResult(`Failed to mount mmap at ${dbFile}`); 
        return;
      }
      
      // Test Fast-Binary Serialization + Batched Writing
      const writeStart = performance.now();
      const insertObj = {
          name: "React Native JSI Database",
          powerLevel: 9001,
          isAwesome: true,
          nested: { db: "mmap", Phase: 4 },
          features: ["zero-copy", "arena-alloc", "b-tree"]
      };
      
      global.NativeDB.insertRec("test_record_1", insertObj);
      const writeEnd = performance.now();
      
      // Test O(log N) Indexed Offset Reads + Binary Deserialization
      const readStart = performance.now();
      const obj = global.NativeDB.findRec("test_record_1");
      const readEnd = performance.now();
      
      setResult(`Init: ${(initEnd-initStart).toFixed(3)}ms | InsertRec: ${(writeEnd-writeStart).toFixed(3)}ms`);
      setReadResult(`FindRec [${(readEnd-readStart).toFixed(3)}ms]: ${JSON.stringify(obj)}`);
      
    } catch (e: any) {
      setResult(`Error: ${e.message}`);
    }
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Phase 4: B+ Tree Engine</Text>
      
      <Button title="Run Batched Serializer Tests" onPress={runPhase4Tests} />
      
      <Text style={styles.result}>{result}</Text>
      <Text style={styles.benchmark}>{readResult}</Text>
      
      <View style={styles.info}>
        <Text style={styles.infoTitle}>Under the Hood:</Text>
        <Text>1. Arena Allocator blocks heap leaks.</Text>
        <Text>2. Msgpack converts Object to Bytes.</Text>
        <Text>3. Buffered BTree tracks offset pointers.</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, alignItems: 'center', justifyContent: 'center', padding: 20, backgroundColor: '#f5f5f5' },
  title: { fontSize: 24, fontWeight: 'bold', marginBottom: 20 },
  result: { fontSize: 14, color: '#333', marginTop: 20, textAlign: 'center' },
  benchmark: { fontSize: 16, color: '#0066cc', marginTop: 10, fontWeight: 'bold' },
  info: { marginTop: 40, padding: 16, backgroundColor: '#fff', borderRadius: 8, width: '100%' },
  infoTitle: { fontSize: 14, fontWeight: '600', marginBottom: 8 },
});