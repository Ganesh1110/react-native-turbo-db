import { TurboDB } from '../index';

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
  getAllKeys: jest.fn(() => Array.from(storage.keys())),
  getAllKeysAsync: jest.fn(async () => Array.from(storage.keys())),
  setMultiAsync: jest.fn(async (entries) => {
    Object.entries(entries).forEach(([k, v]) => storage.set(k, v));
    return true;
  }),
  getMultipleAsync: jest.fn(async (keys) => {
    const res: any = {};
    keys.forEach((k: string) => {
      if (storage.has(k)) res[k] = storage.get(k);
    });
    return res;
  }),
  getStats: jest.fn(() => ({ treeHeight: 1, nodeCount: 1 })),
  getDatabasePath: jest.fn(() => '/path/to/db'),
  getWALPath: jest.fn(() => '/path/to/wal'),
  repair: jest.fn(() => true),
};

describe('TurboDB Enterprise API', () => {
  let db: TurboDB;

  beforeEach(() => {
    db = new TurboDB('/tmp/test.db');
    storage.clear();
    jest.clearAllMocks();
  });

  test('export and import data', async () => {
    db.set('a', 1);
    db.set('b', 2);

    const data = await db.export();
    expect(data).toEqual({ a: 1, b: 2 });

    storage.clear();
    await db.import({ c: 3 });
    expect(db.get('c')).toBe(3);
  });

  test('getMetrics(): returns engine stats', () => {
    const metrics = db.getMetrics();
    expect(metrics.treeHeight).toBe(1);
    expect(metrics.path).toBe('/path/to/db');
  });

  test('compact(): triggers native repair', async () => {
    const success = await db.compact();
    expect(success).toBe(true);
    expect((global as any).NativeDB.repair).toHaveBeenCalled();
  });

  test('use(): plugin system hooks', () => {
    const onSet = jest.fn();
    db.use({ name: 'test-plugin', onSet });

    db.set('plug', 'val');
    expect(onSet).toHaveBeenCalledWith('plug', 'val');
  });
});
