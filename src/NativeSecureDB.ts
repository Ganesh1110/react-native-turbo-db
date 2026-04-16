import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

export interface Spec extends TurboModule {
  install(): void;
  getDocumentsDirectory(): string;
  getVersion(): string;
}

export default TurboModuleRegistry.getEnforcing<Spec>('SecureDB');
