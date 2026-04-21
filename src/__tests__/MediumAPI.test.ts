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
  setMulti: jest.fn((entries: Record<string, any>) => {
    Object.keys(entries).forEach((key) => storage.set(key, entries[key]));
    return true;
  }),
  setAsync: jest.fn(async ({ key, value }) => {
    storage.set(key, value);
    return true;
  }),
  getAsync: jest.fn(async (key) => storage.get(key)),
  removeAsync: jest.fn(async (key) => storage.delete(key)),
};

describe('TurboDB Medium API Extensions', () => {
  let db: TurboDB;

  beforeEach(() => {
    db = new TurboDB('/tmp/test.db');
    storage.clear();
    jest.clearAllMocks();
  });

  test('merge(): partial updates', () => {
    db.set('user', { name: 'Ganesh', age: 25 });
    db.merge('user', { age: 26 });

    const user = db.get('user');
    expect(user).toEqual({ name: 'Ganesh', age: 26 });
  });

  test('subscribe(): listener for specific key', () => {
    const callback = jest.fn();
    db.subscribe('count', callback);

    db.set('count', 1);
    expect(callback).toHaveBeenCalledWith(1);

    db.set('count', 2);
    expect(callback).toHaveBeenCalledWith(2);
  });

  test('subscribeAll(): listener for all changes', () => {
    const callback = jest.fn();
    db.subscribeAll(callback);

    db.set('a', 1);
    expect(callback).toHaveBeenCalledWith({ type: 'set', key: 'a', value: 1 });

    db.remove('a');
    expect(callback).toHaveBeenCalledWith({
      type: 'remove',
      key: 'a',
      value: undefined,
    });
  });

  test('removeMultiple(): removes several keys', () => {
    db.set('k1', 1);
    db.set('k2', 2);
    db.set('k3', 3);

    db.removeMultiple(['k1', 'k3']);
    expect(db.has('k1')).toBe(false);
    expect(db.has('k2')).toBe(true);
    expect(db.has('k3')).toBe(false);
  });

  test('setMultiple(): alias for setMulti with notifications', () => {
    const callback = jest.fn();
    db.subscribeAll(callback);

    db.setMultiple({ a: 1, b: 2 });
    expect(storage.get('a')).toBe(1);
    expect(storage.get('b')).toBe(2);
    expect(callback).toHaveBeenCalledTimes(2);
  });
});
