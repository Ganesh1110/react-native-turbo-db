package com.turbodb;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * KeyStoreManager — Android hardware-backed key management for TurboDB.
 *
 * Master key: stored in SharedPreferences (legacy path, kept for compatibility).
 *
 * Secure items: stored in SharedPreferences (AES-256-GCM encrypted), with the
 * AES key protected by the Android Keystore hardware-backed key
 * ("TurboDBSecureItemKey"). On devices with a hardware Secure Element or
 * StrongBox, the key never leaves the hardware. Requires API 23+.
 */
public class KeyStoreManager {

  // ── Master key (legacy DB encryption key) ────────────────────────────────
  private static final String PREF_NAME       = "TurboDBPrefs";
  private static final String KEY_MASTER_KEY  = "master_key";
  private static Context context;

  public static void init(Context ctx) {
    context = ctx.getApplicationContext();
  }

  public static byte[] getMasterKey() {
    if (context == null) return new byte[32];

    SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
    String encodedKey = prefs.getString(KEY_MASTER_KEY, null);

    if (encodedKey == null) {
      byte[] key = new byte[32];
      new SecureRandom().nextBytes(key);
      encodedKey = Base64.encodeToString(key, Base64.DEFAULT);
      prefs.edit().putString(KEY_MASTER_KEY, encodedKey).apply();
      return key;
    }

    return Base64.decode(encodedKey, Base64.DEFAULT);
  }

  // ── Hardware Secure Item Storage ──────────────────────────────────────────
  // Keystore alias for AES-256-GCM key used to protect secure items.
  private static final String KEYSTORE_ALIAS  = "TurboDBSecureItemKey";
  private static final String SECURE_PREF     = "TurboDBSecureItems";
  private static final String ANDROID_KEYSTORE = "AndroidKeyStore";
  private static final int    GCM_TAG_LEN     = 128; // bits
  private static final int    GCM_IV_LEN      = 12;  // bytes

  /** Generates (or retrieves) the AES-256-GCM key backed by the Android Keystore. */
  private static SecretKey getOrCreateSecureKey() throws GeneralSecurityException, IOException {
    KeyStore ks = KeyStore.getInstance(ANDROID_KEYSTORE);
    ks.load(null);

    if (ks.containsAlias(KEYSTORE_ALIAS)) {
      KeyStore.SecretKeyEntry entry = (KeyStore.SecretKeyEntry) ks.getEntry(KEYSTORE_ALIAS, null);
      if (entry != null) return entry.getSecretKey();
    }

    KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(
        KEYSTORE_ALIAS,
        KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
        .setKeySize(256)
        .setUserAuthenticationRequired(false)
        .build();

    KeyGenerator kg = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEYSTORE);
    kg.init(spec);
    return kg.generateKey();
  }

  /**
   * Stores a string value securely using AES-256-GCM with a hardware-backed key.
   * Returns true on success, false on failure.
   */
  public static boolean setSecureItem(String key, String value) {
    if (context == null) return false;
    try {
      SecretKey secretKey = getOrCreateSecureKey();
      Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
      cipher.init(Cipher.ENCRYPT_MODE, secretKey);
      byte[] iv = cipher.getIV();
      byte[] ciphertext = cipher.doFinal(value.getBytes(StandardCharsets.UTF_8));

      // Pack as Base64(iv) + ":" + Base64(ciphertext)
      String packed = Base64.encodeToString(iv, Base64.NO_WRAP)
          + ":" + Base64.encodeToString(ciphertext, Base64.NO_WRAP);

      SharedPreferences prefs = context.getSharedPreferences(SECURE_PREF, Context.MODE_PRIVATE);
      prefs.edit().putString(key, packed).apply();
      return true;
    } catch (Exception e) {
      return false;
    }
  }

  /**
   * Retrieves a previously stored secure item.
   * Returns null if the key is not found or decryption fails.
   */
  public static String getSecureItem(String key) {
    if (context == null) return null;
    try {
      SharedPreferences prefs = context.getSharedPreferences(SECURE_PREF, Context.MODE_PRIVATE);
      String packed = prefs.getString(key, null);
      if (packed == null) return null;

      int sep = packed.indexOf(':');
      if (sep < 0) return null;

      byte[] iv = Base64.decode(packed.substring(0, sep), Base64.NO_WRAP);
      byte[] ciphertext = Base64.decode(packed.substring(sep + 1), Base64.NO_WRAP);

      SecretKey secretKey = getOrCreateSecureKey();
      Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
      cipher.init(Cipher.DECRYPT_MODE, secretKey, new GCMParameterSpec(GCM_TAG_LEN, iv));
      byte[] plain = cipher.doFinal(ciphertext);
      return new String(plain, StandardCharsets.UTF_8);
    } catch (Exception e) {
      return null;
    }
  }

  /**
   * Permanently removes a secure item. Idempotent — returns true even if not found.
   */
  public static boolean deleteSecureItem(String key) {
    if (context == null) return false;
    SharedPreferences prefs = context.getSharedPreferences(SECURE_PREF, Context.MODE_PRIVATE);
    prefs.edit().remove(key).apply();
    return true;
  }
}
