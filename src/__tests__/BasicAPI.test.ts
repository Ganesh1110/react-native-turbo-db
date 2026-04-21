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
  clearStorage: jest.fn(() => {
    storage.clear();
    return true;
  }),
};

describe('TurboDB Basic API Extensions', () => {
  let db: TurboDB;

  beforeEach(() => {
    db = new TurboDB('/tmp/test.db');
    storage.clear();
  });

  test('length(): returns correct count', () => {
    expect(db.length()).toBe(0);
    db.set('a', 1);
    db.set('b', 2);
    expect(db.length()).toBe(2);
  });

  test('getOrDefault(): returns value or default', () => {
    db.set('a', 100);
    expect(db.getOrDefault('a', 0)).toBe(100);
    expect(db.getOrDefault('b', 404)).toBe(404);
  });

  test('setIfNotExists(): only sets if key is missing', () => {
    expect(db.setIfNotExists('a', 1)).toBe(true);
    expect(db.get('a')).toBe(1);

    expect(db.setIfNotExists('a', 2)).toBe(false);
    expect(db.get('a')).toBe(1);
  });

  test('compareAndSet(): updates only on match', () => {
    db.set('status', 'idle');

    // Fail match
    expect(db.compareAndSet('status', 'running', 'paused')).toBe(false);
    expect(db.get('status')).toBe('idle');

    // Success match
    expect(db.compareAndSet('status', 'idle', 'running')).toBe(true);
    expect(db.get('status')).toBe('running');
  });

  test('compareAndSet(): works with objects', () => {
    const user = { id: 1, name: 'Ganesh' };
    db.set('user', user);

    expect(
      db.compareAndSet(
        'user',
        { id: 1, name: 'Ganesh' },
        { id: 1, name: 'Updated' }
      )
    ).toBe(true);
    expect(db.get('user').name).toBe('Updated');
  });

  test('TTL support: expires after time', async () => {
    jest.useFakeTimers();

    db.setWithTTL('expiring', 'hello', 1000);
    expect(db.get('expiring')).toBe('hello');

    // Advance time by 1001ms
    jest.advanceTimersByTime(1001);

    expect(db.get('expiring')).toBeUndefined();
    expect(storage.has('expiring')).toBe(false);

    jest.useRealTimers();
  });
});
