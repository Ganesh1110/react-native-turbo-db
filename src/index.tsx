import { TurboModuleRegistry } from 'react-native';
import type { Spec } from './NativeSecureDB';

const NativeSecureDB = TurboModuleRegistry.get<Spec>('SecureDB');

declare const global: {
  NativeDB: {
    initStorage(path: string, size: number): boolean;
    insertRec(key: string, obj: any): boolean;
    findRec(key: string): any;
    clearStorage(): boolean;
    benchmark(): number;
  };
};

export class SecureDB {
  static install() {
    if (!NativeSecureDB) {
      console.error(
        "SecureDB: Native module 'SecureDB' not found. " +
          'Ensure you have rebuilt the native app (npx react-native run-ios / run-android) ' +
          'and that the module is correctly linked.'
      );
      return;
    }
    NativeSecureDB.install();
  }

  private isInitialized = false;

  constructor(
    private path: string,
    private size: number = 10 * 1024 * 1024
  ) {}

  private ensureInitialized() {
    if (!this.isInitialized) {
      if (!NativeSecureDB) {
        throw new Error(
          'SecureDB: Native module not found. Check your native build.'
        );
      }
      SecureDB.install();

      if (typeof global.NativeDB === 'undefined') {
        throw new Error(
          'SecureDB: NativeDB JSI object not found after install(). Check native logs.'
        );
      }

      const success = global.NativeDB.initStorage(this.path, this.size);
      if (!success) {
        throw new Error(`Failed to initialize SecureDB at ${this.path}`);
      }
      this.isInitialized = true;
    }
  }

  set(key: string, value: any): boolean {
    this.ensureInitialized();
    return global.NativeDB.insertRec(key, value);
  }

  get<T = any>(key: string): T | undefined {
    this.ensureInitialized();
    return global.NativeDB.findRec(key);
  }

  clear(): boolean {
    this.ensureInitialized();
    return global.NativeDB.clearStorage();
  }

  benchmark(): number {
    this.ensureInitialized();
    return global.NativeDB.benchmark();
  }
}

export default SecureDB;
