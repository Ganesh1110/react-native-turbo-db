import { TurboDB } from '../index';
import { SyncManager } from '../SyncManager';

jest.mock('react-native', () => ({
  TurboModuleRegistry: {
    get: jest.fn(() => ({
      install: jest.fn(() => true),
      getDocumentsDirectory: jest.fn(() => '/tmp'),
    })),
    getEnforcing: jest.fn(() => ({
      install: jest.fn(() => true),
      getDocumentsDirectory: jest.fn(() => '/tmp'),
    })),
  },
}));

const storage = new Map<string, any>();

(global as any).NativeDB = {
  initStorage: jest.fn(() => true),
  insertRec: jest.fn((key: string, value: any) => {
    storage.set(key, value);
    return true;
  }),
  findRec: jest.fn((key: string) => storage.get(key)),
  remove: jest.fn((key: string) => storage.delete(key)),
  getAllKeys: jest.fn(() => Array.from(storage.keys())),
  getAllKeysAsync: jest.fn(async () => Array.from(storage.keys())),
  getAsync: jest.fn(async (key) => storage.get(key)),
  setAsync: jest.fn(async ({ key, value }) => {
    storage.set(key, value);
    return true;
  }),
  removeAsync: jest.fn(async (key) => storage.delete(key)),
  getLocalChangesAsync: jest.fn(async () => ({ changes: [] })),
  applyRemoteChangesAsync: jest.fn(async () => true),
  markPushedAsync: jest.fn(async () => true),
};

describe('TurboDB Pro API (Sync & Migrations)', () => {
  let db: TurboDB;

  beforeEach(() => {
    db = new TurboDB('/tmp/test.db');
    storage.clear();
    jest.clearAllMocks();
  });

  test('SyncManager: pause, resume, network status', async () => {
    const adapter = {
      pullChanges: jest.fn(async () => ({
        changes: [],
        latest_remote_version: 1,
      })),
      pushChanges: jest.fn(async () => []),
    };
    const sync = new SyncManager(db, adapter, { autoSync: false });

    sync.pause();
    await sync.forceSync();
    expect(adapter.pullChanges).not.toHaveBeenCalled();

    sync.resume();
    sync.pause();
    jest.clearAllMocks();

    sync.setNetworkStatus(false);
    await sync.forceSync();
    expect(adapter.pullChanges).not.toHaveBeenCalled();

    sync.resume();
    sync.setNetworkStatus(true);
    await sync.forceSync();
    expect(adapter.pullChanges).toHaveBeenCalled();
    sync.stop();
  });

  test('Migrations: migrate from v0 to v1', async () => {
    const migration = jest.fn(async (migrateDb: TurboDB) => {
      await migrateDb.setAsync('migrated', true);
    });

    await db.migrate(0, 1, migration);

    expect(migration).toHaveBeenCalled();
    expect(storage.get('migrated')).toBe(true);
    expect(storage.get('__sys_db_version')).toBe(1);
  });
  test('Offline Queue: enqueue and flush', async () => {
    await db.enqueueMutation({ type: 'set', key: 'q1', value: 'queued' });
    expect(storage.get('q1')).toBeUndefined();

    await db.flushQueue();
    expect(storage.get('q1')).toBe('queued');

    const queue = await db.getAsync('__sys_mutation_queue');
    expect(queue).toEqual([]);
  });
});
