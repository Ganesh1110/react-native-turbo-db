import { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  ScrollView,
  ActivityIndicator,
} from 'react-native';
import { TurboDB, SyncManager } from 'rn-turbo-db';
import type { SyncAdapter, SyncRecord, SyncAck } from 'rn-turbo-db';

/**
 * A simulate remote server in memory for demonstration.
 */
class MockServer {
  private storage: Map<string, SyncRecord> = new Map();
  private remoteVersion: number = 100;

  getChanges(sinceVersion: number) {
    const changes: SyncRecord[] = [];
    for (const record of this.storage.values()) {
      if (record.remote_version > sinceVersion) {
        changes.push(record);
      }
    }
    return { changes, latest_remote_version: this.remoteVersion };
  }

  push(changes: SyncRecord[]): SyncAck[] {
    const acks: SyncAck[] = [];
    for (const change of changes) {
      this.remoteVersion++;
      const serverRecord: SyncRecord = {
        ...change,
        remote_version: this.remoteVersion,
      };
      this.storage.set(change.key, serverRecord);
      acks.push({ key: change.key, remote_version: this.remoteVersion });
    }
    return acks;
  }

  addRemoteData(key: string, data: any) {
    this.remoteVersion++;
    this.storage.set(key, {
      key,
      data,
      remote_version: this.remoteVersion,
      updated_at: Date.now(),
      logical_clock: 0,
      is_deleted: false,
    });
  }
}

const mockServer = new MockServer();

export const SyncPage = ({ db }: { db: TurboDB }) => {
  const [logs, setLogs] = useState<string[]>([]);
  const [isSyncing, setIsSyncing] = useState(false);
  const syncManagerRef = useRef<SyncManager | null>(null);

  const addLog = (msg: string) => {
    setLogs((prev) => [
      `[${new Date().toLocaleTimeString()}] ${msg}`,
      ...prev.slice(0, 49),
    ]);
  };

  useEffect(() => {
    const adapter: SyncAdapter = {
      pullChanges: async (since) => {
        addLog(`📡 Pulling from server (since version ${since})...`);
        await new Promise((r) => setTimeout(r, 800)); // Simulate latency
        const res = mockServer.getChanges(since);
        addLog(`📥 Received ${res.changes.length} changes from server.`);
        return res;
      },
      pushChanges: async (changes) => {
        addLog(`📤 Pushing ${changes.length} local changes to server...`);
        await new Promise((r) => setTimeout(r, 800));
        const acks = mockServer.push(changes);
        addLog(`✅ Server ACKed ${acks.length} records.`);
        return acks;
      },
    };

    const manager = new SyncManager(db, adapter, {
      autoSync: false,
    });

    const sub = manager.onSyncEvent((event, error) => {
      if (event === 'started') setIsSyncing(true);
      if (event === 'error') {
        addLog(`❌ Sync Error: ${error?.message}`);
        setIsSyncing(false);
      }
      if (event === 'push_success' || event === 'stopped') setIsSyncing(false);
      addLog(`🔄 Event: ${event.toUpperCase()}`);
    });

    syncManagerRef.current = manager;
    manager.start();

    return () => {
      sub();
      manager.stop();
    };
  }, [db]);

  const handleManualSync = async () => {
    if (isSyncing) return;
    addLog('🚀 Manual sync triggered.');
    await syncManagerRef.current?.forceSync();
  };

  const handleAddLocal = async () => {
    const id = Math.random().toString(36).substring(7);
    const key = `note_${id}`;
    const value = {
      title: `Local Note ${id}`,
      content: 'I was created while offline',
    };
    await db.setAsync(key, value);
    addLog(`📝 Added LOCAL record: ${key}`);
  };

  const handleAddRemote = () => {
    const id = Math.random().toString(36).substring(7);
    const key = `server_${id}`;
    mockServer.addRemoteData(key, {
      title: `Remote Note ${id}`,
      content: 'A background change',
    });
    addLog(`🌐 Added REMOTE record to server: ${key}`);
  };

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Offline-First Sync Engine</Text>
        <Text style={styles.subtitle}>
          Native LWW Conflict Resolution (C++)
        </Text>

        <View style={styles.buttonGrid}>
          <TouchableOpacity
            style={styles.actionButton}
            onPress={handleAddLocal}
          >
            <Text style={styles.buttonText}>📝 Add Local</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={[styles.actionButton, { borderColor: '#8B5CF6' }]}
            onPress={handleAddRemote}
          >
            <Text style={[styles.buttonText, { color: '#C4B5FD' }]}>
              🌐 Add Remote
            </Text>
          </TouchableOpacity>
        </View>

        <TouchableOpacity
          style={[styles.syncButton, isSyncing && styles.syncButtonDisabled]}
          onPress={handleManualSync}
          disabled={isSyncing}
        >
          {isSyncing ? (
            <ActivityIndicator size="small" color="#fff" />
          ) : (
            <Text style={styles.syncButtonText}>🔄 Run Full Sync Cycle</Text>
          )}
        </TouchableOpacity>
      </View>

      <View style={[styles.card, { flex: 1, paddingBottom: 0 }]}>
        <Text style={styles.logTitle}>Sync Logs</Text>
        <ScrollView
          style={styles.logScroll}
          contentContainerStyle={{ paddingBottom: 20 }}
        >
          {logs.length === 0 ? (
            <Text style={styles.emptyLog}>Ready for synchronization...</Text>
          ) : (
            logs.map((log, i) => (
              <Text key={i} style={styles.logText}>
                {log}
              </Text>
            ))
          )}
        </ScrollView>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, gap: 20 },
  card: {
    backgroundColor: 'rgba(15, 15, 40, 0.6)',
    borderRadius: 32,
    padding: 24,
    borderWidth: 1,
    borderColor: 'rgba(99, 102, 241, 0.15)',
  },
  title: { color: '#FFF', fontSize: 22, fontWeight: '900', marginBottom: 6 },
  subtitle: {
    color: '#6366F1',
    fontSize: 12,
    fontWeight: '700',
    textTransform: 'uppercase',
    letterSpacing: 1,
  },
  buttonGrid: {
    flexDirection: 'row',
    gap: 12,
    marginTop: 25,
    marginBottom: 15,
  },
  actionButton: {
    flex: 1,
    padding: 16,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: '#38BDF8',
    alignItems: 'center',
  },
  buttonText: { color: '#7DD3FC', fontSize: 13, fontWeight: '800' },
  syncButton: {
    backgroundColor: '#6366F1',
    padding: 18,
    borderRadius: 20,
    alignItems: 'center',
    shadowColor: '#6366F1',
    shadowOffset: { width: 0, height: 10 },
    shadowOpacity: 0.3,
    shadowRadius: 15,
    elevation: 8,
  },
  syncButtonDisabled: { opacity: 0.6 },
  syncButtonText: {
    color: '#FFF',
    fontSize: 15,
    fontWeight: '900',
    letterSpacing: 0.5,
  },
  logTitle: {
    color: '#F8FAFC',
    fontSize: 16,
    fontWeight: '800',
    marginBottom: 15,
  },
  logScroll: { backgroundColor: '#050511', borderRadius: 20, padding: 15 },
  emptyLog: {
    color: '#475569',
    textAlign: 'center',
    marginTop: 40,
    fontStyle: 'italic',
  },
  logText: {
    color: '#94A3B8',
    fontSize: 12,
    fontFamily: 'monospace',
    marginBottom: 8,
    lineHeight: 16,
  },
});
