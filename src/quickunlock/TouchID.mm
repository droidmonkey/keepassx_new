#include "quickunlock/TouchID.h"

#include "crypto/Random.h"
#include "crypto/SymmetricCipher.h"
#include "crypto/CryptoHash.h"
#include "config-keepassx.h"

#include <botan/mem_ops.h>

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <LocalAuthentication/LocalAuthentication.h>
#include <Security/Security.h>

#include <QCoreApplication>
#include <QString>

#define TOUCH_ID_ENABLE_DEBUG_LOGS() 0
#if TOUCH_ID_ENABLE_DEBUG_LOGS()
#define debug(...) qWarning(__VA_ARGS__)
#else
inline void debug(const char *message, ...)
{
   Q_UNUSED(message);
}
#endif

inline std::string StatusToErrorMessage(OSStatus status)
{
   CFStringRef text = SecCopyErrorMessageString(status, NULL);
   if (!text) {
      return std::to_string(status);
   }

   auto msg = CFStringGetCStringPtr(text, kCFStringEncodingUTF8);
   std::string result;
   if (msg) {
       result = msg;
   }
   CFRelease(text);
   return result;
}

inline void LogStatusError(const char *message, OSStatus status)
{
   if (!status) {
      return;
   }

   std::string msg = StatusToErrorMessage(status);
   debug("%s: %s", message, msg.c_str());
}

inline CFMutableDictionaryRef makeDictionary() {
   return CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

//! Try to delete an existing keychain entry
void TouchID::deleteKeyEntry(const QString& accountName)
{
   NSString* nsAccountName = accountName.toNSString(); // The NSString is released by Qt

   // try to delete an existing entry
   CFMutableDictionaryRef query = makeDictionary();
   CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
   CFDictionarySetValue(query, kSecAttrAccount, (__bridge CFStringRef) nsAccountName);
   CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);

   // get data from the KeyChain
   OSStatus status = SecItemDelete(query);
   LogStatusError("TouchID::deleteKeyEntry - Status deleting existing entry", status);
}

QString TouchID::databaseKeyName(const QUuid& dbUuid)
{
   static const QString keyPrefix = "KeepassXC_TouchID_Keys_";
   return keyPrefix + dbUuid.toString();
}

QString TouchID::errorString() const
{
    // TODO
    return "";
}

void TouchID::reset()
{
    m_encryptedMasterKeys.clear();
}




bool TouchID::setKey(const QUuid& dbUuid, const QByteArray& passwordKey, const bool ignoreTouchID)
{
    if (passwordKey.isEmpty()) {
        debug("TouchID::setKey - illegal arguments");
        return false;
    }

    if (m_encryptedMasterKeys.contains(dbUuid)) {
        debug("TouchID::setKey - Already stored key for this database");
        return true;
    }

    // generate random AES 256bit key and IV
    QByteArray randomKey = randomGen()->randomArray(SymmetricCipher::keySize(SymmetricCipher::Aes256_GCM));
    QByteArray randomIV = randomGen()->randomArray(SymmetricCipher::defaultIvSize(SymmetricCipher::Aes256_GCM));

    SymmetricCipher aes256Encrypt;
    if (!aes256Encrypt.init(SymmetricCipher::Aes256_GCM, SymmetricCipher::Encrypt, randomKey, randomIV)) {
        debug("TouchID::setKey - AES initialisation failed");
        return false;
    }

    // encrypt and keep result in memory
    QByteArray encryptedMasterKey = passwordKey;
    if (!aes256Encrypt.finish(encryptedMasterKey)) {
        debug("TouchID::getKey - AES encrypt failed: %s", aes256Encrypt.errorString().toUtf8().constData());
        return false;
    }

    const QString keyName = databaseKeyName(dbUuid);

    deleteKeyEntry(keyName); // Try to delete the existing key entry

    // prepare adding secure entry to the macOS KeyChain
    CFErrorRef error = NULL;

    // We need both runtime and compile time checks here to solve the following problems:
    // - Not all flags are available in all OS versions, so we have to check it at compile time
    // - Requesting Biometry/TouchID/DevicePassword when to fingerprint sensor is available will result in runtime error
    SecAccessControlCreateFlags accessControlFlags = 0;
#if XC_COMPILER_SUPPORT(APPLE_BIOMETRY)
    // Needs a special check to work with SecItemAdd, when TouchID is not enrolled and the flag
    // is set, the method call fails with an error. But we want to still set this flag if TouchID is
    // enrolled but temporarily unavailable due to closed lid
    //
    // At least on a Hackintosh the enrolled-check does not work, there LAErrorBiometryNotAvailable gets returned instead of
    // LAErrorBiometryNotEnrolled.
    //
    // Thats kinda unfortunate, because now you cannot know for sure if TouchID hardware is either temporarily unavailable or not present
    // at all, because LAErrorBiometryNotAvailable is used for both cases.
    //
    // So to make quick unlock fallbacks possible on these machines you have to try to save the key a second time without this flag, if the
    // first try fails with an error.
    if (!ignoreTouchID) {
        // Prefer the non-deprecated flag when available
        accessControlFlags = kSecAccessControlBiometryCurrentSet;
    }
#elif XC_COMPILER_SUPPORT(TOUCH_ID)
    if (!ignoreTouchID) {
        accessControlFlags = kSecAccessControlTouchIDCurrentSet;
    }
#endif

#if XC_COMPILER_SUPPORT(WATCH_UNLOCK)
      accessControlFlags = accessControlFlags | kSecAccessControlOr | kSecAccessControlWatch;
#endif

#if XC_COMPILER_SUPPORT(TOUCH_ID)
   if (isPasswordFallbackPossible()) {
       accessControlFlags = accessControlFlags | kSecAccessControlOr | kSecAccessControlDevicePasscode;
   }
#endif

   SecAccessControlRef sacObject = SecAccessControlCreateWithFlags(
       kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, accessControlFlags, &error);

    if (sacObject == NULL || error != NULL) {
        NSError* e = (__bridge NSError*) error;
        debug("TouchID::setKey - Error creating security flags: %s", e.localizedDescription.UTF8String);
        return false;
    }

    NSString *accountName = keyName.toNSString(); // The NSString is released by Qt

    // prepare data (key) to be stored
    QByteArray keychainKeyValue = (randomKey + randomIV).toHex();
    CFDataRef keychainValueData =
        CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<UInt8 *>(keychainKeyValue.data()),
                                    keychainKeyValue.length(), kCFAllocatorDefault);

    CFMutableDictionaryRef attributes = makeDictionary();
    CFDictionarySetValue(attributes, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attributes, kSecAttrAccount, (__bridge CFStringRef) accountName);
    CFDictionarySetValue(attributes, kSecValueData, (__bridge CFDataRef) keychainValueData);
    CFDictionarySetValue(attributes, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionarySetValue(attributes, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);
    CFDictionarySetValue(attributes, kSecAttrAccessControl, sacObject);

    // add to KeyChain
    OSStatus status = SecItemAdd(attributes, NULL);
    LogStatusError("TouchID::setKey - Status adding new entry", status);

    CFRelease(sacObject);
    CFRelease(attributes);
    
    // Cleanse the key information from the memory
    Botan::secure_scrub_memory(randomKey.data(), randomKey.size());
    Botan::secure_scrub_memory(randomIV.data(), randomIV.size());

    if (status != errSecSuccess) {
        return false;
    }

    // memorize which database the stored key is for
    m_encryptedMasterKeys.insert(dbUuid, encryptedMasterKey);
    debug("TouchID::setKey - Success!");
    return true;
}

/**
 * Generates a random AES 256bit key and uses it to encrypt the PasswordKey that
 * protects the database. The encrypted PasswordKey is kept in memory while the
 * AES key is stored in the macOS KeyChain protected by either TouchID or Apple Watch.
 */
bool TouchID::setKey(const QUuid& dbUuid, const QByteArray& passwordKey)
{
    if (!setKey(dbUuid,passwordKey, false)) {
        debug("TouchID::setKey failed with error trying fallback method without TouchID flag");
        return setKey(dbUuid, passwordKey, true);
    } else {
        return true;
    }
}

/**
 * Checks if an encrypted PasswordKey is available for the given database, tries to
 * decrypt it using the KeyChain and if successful, returns it.
 */
bool TouchID::getKey(const QUuid& dbUuid, QByteArray& passwordKey)
{
    passwordKey.clear();

    if (!hasKey(dbUuid)) {
        debug("TouchID::getKey - No stored key found");
        return false;
    }

    // query the KeyChain for the AES key
    CFMutableDictionaryRef query = makeDictionary();

    const QString keyName = databaseKeyName(dbUuid);
    NSString* accountName = keyName.toNSString(); // The NSString is released by Qt
    NSString* touchPromptMessage =
        QCoreApplication::translate("DatabaseOpenWidget", "authenticate to access the database")
            .toNSString();  // The NSString is released by Qt

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrAccount, (__bridge CFStringRef) accountName);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecUseOperationPrompt, (__bridge CFStringRef) touchPromptMessage);

    // get data from the KeyChain
    CFTypeRef dataTypeRef = NULL;
    OSStatus status = SecItemCopyMatching(query, &dataTypeRef);
    CFRelease(query);

    if (status == errSecUserCanceled) {
        // user canceled the authentication, return true with empty key
        debug("TouchID::getKey - User canceled authentication");
        return true;
    } else if (status != errSecSuccess || dataTypeRef == NULL) {
        LogStatusError("TouchID::getKey - key query error", status);
        return false;
    }

    CFDataRef valueData = static_cast<CFDataRef>(dataTypeRef);
    QByteArray dataBytes = QByteArray::fromHex(QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(valueData)),
                                                          CFDataGetLength(valueData)));
    CFRelease(dataTypeRef);

    // extract AES key and IV from data bytes
    QByteArray key = dataBytes.left(SymmetricCipher::keySize(SymmetricCipher::Aes256_GCM));
    QByteArray iv = dataBytes.right(SymmetricCipher::defaultIvSize(SymmetricCipher::Aes256_GCM));

    SymmetricCipher aes256Decrypt;
    if (!aes256Decrypt.init(SymmetricCipher::Aes256_GCM, SymmetricCipher::Decrypt, key, iv)) {
        debug("TouchID::getKey - AES initialization failed");
        return false;
    }

    // decrypt PasswordKey from memory using AES
    passwordKey = m_encryptedMasterKeys[dbUuid];
    if (!aes256Decrypt.finish(passwordKey)) {
        passwordKey.clear();
        debug("TouchID::getKey - AES decrypt failed: %s", aes256Decrypt.errorString().toUtf8().constData());
        return false;
    }

    // Cleanse the key information from the memory
    Botan::secure_scrub_memory(key.data(), key.size());
    Botan::secure_scrub_memory(iv.data(), iv.size());

    return true;
}

bool TouchID::hasKey(const QUuid& dbUuid) const
{
    return m_encryptedMasterKeys.contains(dbUuid);
}

// TODO: Both functions below should probably handle the returned errors to
// provide more information on availability. E.g.: the closed laptop lid results
// in an error (because touch id is not unavailable). That error could be
// displayed to the user when we first check for availability instead of just
// hiding the checkbox.

//! @return true if Apple Watch is available for authentication.
bool TouchID::isWatchAvailable()
{
#if XC_COMPILER_SUPPORT(WATCH_UNLOCK)
   @try {
      LAContext *context = [[LAContext alloc] init];

      LAPolicy policyCode = LAPolicyDeviceOwnerAuthenticationWithWatch;
      NSError *error;

      bool canAuthenticate = [context canEvaluatePolicy:policyCode error:&error];
      [context release];
      if (error) {
         debug("Apple Wach available: %d (%ld / %s / %s)", canAuthenticate,
               (long)error.code, error.description.UTF8String,
               error.localizedDescription.UTF8String);
      } else {
          debug("Apple Wach available: %d", canAuthenticate);
      }
      return canAuthenticate;
   } @catch (NSException *) {
      return false;
   }
#else
   return false;
#endif
}

//! @return true if Touch ID is available for authentication.
bool TouchID::isTouchIdAvailable()
{
#if XC_COMPILER_SUPPORT(TOUCH_ID)
   @try {
      LAContext *context = [[LAContext alloc] init];

      LAPolicy policyCode = LAPolicyDeviceOwnerAuthenticationWithBiometrics;
      NSError *error;

      bool canAuthenticate = [context canEvaluatePolicy:policyCode error:&error];
      [context release];
      if (error) {
         debug("Touch ID available: %d (%ld / %s / %s)", canAuthenticate,
               (long)error.code, error.description.UTF8String,
               error.localizedDescription.UTF8String);
      } else {
          debug("Touch ID available: %d", canAuthenticate);
      }
      return canAuthenticate;
   } @catch (NSException *) {
      return false;
   }
#else
   return false;
#endif
}

bool TouchID::isPasswordFallbackPossible()
{
#if XC_COMPILER_SUPPORT(TOUCH_ID)
    @try {
        LAContext *context = [[LAContext alloc] init];

        LAPolicy policyCode = LAPolicyDeviceOwnerAuthentication;
        NSError *error;

        bool canAuthenticate = [context canEvaluatePolicy:policyCode error:&error];
        [context release];
        if (error) {
            debug("Password fallback available: %d (%ld / %s / %s)", canAuthenticate,
                  (long)error.code, error.description.UTF8String,
                  error.localizedDescription.UTF8String);
        } else {
            debug("Password fallback available: %d", canAuthenticate);
        }
        return canAuthenticate;
    } @catch (NSException *) {
        return false;
    }
#else
    return false;
#endif
}

//! @return true if either TouchID or Apple Watch is available at the moment.
bool TouchID::isAvailable() const
{
   // note: we cannot cache the check results because the configuration
   // is dynamic in its nature. User can close the laptop lid or take off
   // the watch, thus making one (or both) of the authentication types unavailable.
   return  isWatchAvailable() || isTouchIdAvailable() || isPasswordFallbackPossible();
}

/**
 * Resets the inner state either for all or for the given database
 */
void TouchID::reset(const QUuid& dbUuid)
{
    m_encryptedMasterKeys.remove(dbUuid);
}
