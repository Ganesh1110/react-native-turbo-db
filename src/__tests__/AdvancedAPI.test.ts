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
  findRec: jest.fn((key: string) => {
    return storage.get(key);
  }),
  remove: jest.fn((key: string) => {
    return storage.delete(key);
  }),
  getAllKeys: jest.fn(() => Array.from(storage.keys())),
  getAllKeysAsync: jest.fn(async () => Array.from(storage.keys())),
  getAsync: jest.fn(async (key) => storage.get(key)),
  setAsync: jest.fn(async ({ key, value }) => {
    storage.set(key, value);
    return true;
  }),
  rangeQuery: jest.fn((start, end) => {
    return Array.from(storage.entries())
      .filter(([k]) => k >= start && k <= end)
      .map(([key, value]) => ({ key, value }));
  }),
  rangeQueryAsync: jest.fn(async ({ startKey, endKey }) => {
    return Array.from(storage.entries())
      .filter(([k]) => k >= startKey && k <= endKey)
      .map(([key, value]) => ({ key, value }));
  }),
  getMultipleAsync: jest.fn(async (keys) => {
    const res: any = {};
    keys.forEach((k: string) => {
      if (storage.has(k)) res[k] = storage.get(k);
    });
    return res;
  }),
  flush: jest.fn(),
};

describe('TurboDB Advanced API Extensions', () => {
  let db: TurboDB;

  beforeEach(() => {
    db = new TurboDB('/tmp/test.db');
    storage.clear();
    jest.clearAllMocks();
  });

  test('getByPrefix(): returns matching keys', () => {
    db.set('user:1', 'Alice');
    db.set('user:2', 'Bob');
    db.set('post:1', 'Hello');

    const results = db.getByPrefix('user:');
    expect(results.length).toBe(2);
    expect(results[0]?.value).toBe('Alice');
  });

  test('query(): filters and sorts results', async () => {
    db.set('u1', { name: 'Zoe', age: 30 });
    db.set('u2', { name: 'Alice', age: 25 });
    db.set('u3', { name: 'Bob', age: 35 });

    const results = await db.query({
      filter: (u) => u.age > 20,
      sort: (a, b) => a.name.localeCompare(b.name),
      limit: 2,
    });

    expect(results.length).toBe(2);
    expect(results[0].name).toBe('Alice');
    expect(results[1].name).toBe('Bob');
  });

  test('secondary indexing: createIndex and queryByIndex', async () => {
    await db.createIndex('role');

    db.set('user:1', { name: 'Admin', role: 'admin' });
    db.set('user:2', { name: 'Guest', role: 'guest' });

    const admins = await db.queryByIndex('role', 'admin');
    expect(admins.length).toBe(1);
    expect(admins[0].name).toBe('Admin');
  });

  test('streaming: streamKeys returns generator', async () => {
    db.set('a', 1);
    db.set('b', 2);

    const keys = [];
    for await (const key of db.streamKeys()) {
      keys.push(key);
    }
    expect(keys).toContain('a');
    expect(keys).toContain('b');
  });

  test('transaction: executes block and flushes', async () => {
    await db.transaction((tx) => {
      tx.set('tx1', 'val1');
      tx.set('tx2', 'val2');
    });

    expect(db.get('tx1')).toBe('val1');
    expect((global as any).NativeDB.flush).toHaveBeenCalled();
  });
});
