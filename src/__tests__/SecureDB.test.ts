import { SecureDB } from '../index';

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
  del: jest.fn((key: string) => {
    if (storage.has(key)) {
      storage.delete(key);
      return true;
    }
    return false;
  }),
  clearStorage: jest.fn(() => {
    storage.clear();
    return true;
  }),
  setMulti: jest.fn((entries: Record<string, any>) => {
    Object.keys(entries).forEach((key) => storage.set(key, entries[key]));
    return true;
  }),
  getAllKeys: jest.fn(() => Array.from(storage.keys())),
  clear: jest.fn(() => {
    storage.clear();
    return true;
  }),
};

describe('SecureDB', () => {
  let db: SecureDB;

  beforeEach(() => {
    db = new SecureDB('/tmp/test.db');
    storage.clear();
    jest.clearAllMocks();
  });

  it('initializes on first set', () => {
    db.set('foo', 'bar');
    expect((global as any).NativeDB.initStorage).toHaveBeenCalled();
  });

  it('can SET a value', () => {
    const success = db.set('test_key', { data: 'test_value' });
    expect(success).toBe(true);
    expect((global as any).NativeDB.insertRec).toHaveBeenCalledWith(
      'test_key',
      { data: 'test_value' }
    );
  });

  it('can GET a value', () => {
    db.set('test_key', { data: 'test_value' });
    const val = db.get('test_key');
    expect(val).toEqual({ data: 'test_value' });
  });

  it('returns undefined for non-existent key', () => {
    const val = db.get('missing');
    expect(val).toBeUndefined();
  });

  it('can DEL (delete) an existing key', () => {
    db.set('test_key', { data: 'test_value' });
    expect(db.get('test_key')).toEqual({ data: 'test_value' });

    const result = db.del('test_key');
    expect(result).toBe(true);
    expect(db.get('test_key')).toBeUndefined();
  });

  it('DEL returns false for non-existent key', () => {
    const result = db.del('non_existent_key');
    expect(result).toBe(false);
  });

  it('can CLEAR all data', () => {
    db.set('key1', 'value1');
    db.set('key2', 'value2');
    expect(db.getAllKeys().length).toBe(2);

    const success = db.clear();
    expect(success).toBe(true);
    expect(db.getAllKeys().length).toBe(0);
  });

  it('can get ALL KEYS', () => {
    db.set('key1', 'value1');
    db.set('key2', 'value2');
    db.set('key3', 'value3');

    const keys = db.getAllKeys();
    expect(keys).toContain('key1');
    expect(keys).toContain('key2');
    expect(keys).toContain('key3');
    expect(keys.length).toBe(3);
  });

  it('can use setMulti for bulk insert', () => {
    const entries = {
      bulk_key1: { data: 'bulk_value1' },
      bulk_key2: { data: 'bulk_value2' },
      bulk_key3: { data: 'bulk_value3' },
    };

    const success = db.setMulti(entries);
    expect(success).toBe(true);
    expect(db.getAllKeys().length).toBe(3);
  });

  it('DELETE removes key from getAllKeys', () => {
    db.set('key1', 'value1');
    db.set('key2', 'value2');
    expect(db.getAllKeys().length).toBe(2);

    db.del('key1');
    const keys = db.getAllKeys();
    expect(keys).not.toContain('key1');
    expect(keys).toContain('key2');
    expect(keys.length).toBe(1);
  });
});
