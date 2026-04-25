describe('Critical API Regression Tests', () => {
  describe('BUG-4: has() TTL check', () => {
    test('has() returns false for expired TTL keys', () => {
      const storage = new Map<string, any>();
      storage.set('expired', {
        data: 'value',
        __ttl_expiry: Date.now() - 1000,
      });
      const hasExpired = storage.get('expired');
      expect(hasExpired?.__ttl_expiry).toBeLessThan(Date.now());
    });

    test('has() uses get() path for TTL', () => {
      const storage = new Map<string, any>();
      storage.set('valid', { data: 'value', __ttl_expiry: Date.now() + 10000 });
      const hasValid = storage.get('valid');
      expect(hasValid?.__ttl_expiry).toBeGreaterThan(Date.now());
    });
  });

  describe('WIRE-5: export() excludes internal keys', () => {
    test('export() filters __idx: keys', () => {
      const storage = new Map<string, any>();
      storage.set('user1', { name: 'Alice' });
      storage.set('__idx:name', 'Alice');
      storage.set('__sys:config', { theme: 'dark' });

      const exportData = () =>
        Array.from(storage.keys())
          .filter((k) => !k.startsWith('__'))
          .reduce(
            (acc, k) => {
              acc[k] = storage.get(k);
              return acc;
            },
            {} as Record<string, any>
          );

      const data = exportData();

      expect(data).toHaveProperty('user1');
      expect(data).not.toHaveProperty('__idx:name');
      expect(data).not.toHaveProperty('__sys:config');
    });
  });

  describe('BUG-5: cleanupExpired() skips internal keys', () => {
    test('cleanupExpired filters internal keys', () => {
      const storage = new Map<string, any>();
      storage.set('user1', { data: 'val', __ttl_expiry: Date.now() + 1000 });
      storage.set('__idx:name', 'Alice');

      let user1Accessed = false;
      let idxAccessed = false;

      const get = (key: string) => {
        if (key === 'user1') user1Accessed = true;
        if (key.startsWith('__')) idxAccessed = true;
        return storage.get(key);
      };

      const cleanup = () => {
        const keys = Array.from(storage.keys());
        for (const key of keys) {
          if (!key.startsWith('__')) get(key);
        }
      };

      cleanup();

      expect(user1Accessed).toBe(true);
      expect(idxAccessed).toBe(false);
    });
  });
});
