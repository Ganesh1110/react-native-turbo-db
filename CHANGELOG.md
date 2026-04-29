# Changelog

All notable changes to this project will be documented in this file.

## [1.2.0] - 2026-04-29

### Added (Performance Release - v1.2.0)

- **Data-Level LRU Cache**: Cache deserialized JSI values for hot keys, avoiding repeated mmap reads and BinarySerializer deserialization
- **Zero-Copy Path**: Direct decode for simple types (null, boolean, number, short strings) without full binary deserialization
- **Read-Ahead Prefetch**: Prefetch adjacent B+Tree leaf nodes during range queries to improve sequential read performance
- **Compression Infrastructure**: Added zlib-based compression support for large values (optional, threshold: 256+ bytes)

### Performance Improvements

- Hot key reads now served from LRU cache (avoids mmap + deserialize)
- Simple types bypass serialization (zero-copy decode)
- Range queries benefit from B+Tree leaf prefetching

## [0.1.0] - 2026-04-19

### Added

- Initial release
- JSI-driven embedded key-value database for React Native
- Secure storage with hardware-backed encryption support
- ACID compliance for data integrity
- SSR/isomorphic support
- Web support via index.web.ts

### Features

- Fast key-value storage operations
- Range queries
- Batch operations (setMulti, getMultiple)
- Async variants for all operations
- TypeScript support with full type definitions
