// KoreanTTS.h - Korean TTS and Smart Recommendation for Weasel
// Ported from macOS Squirrel-TTS by yaja

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace weasel {

// Unicode Jamo mapping tables
class JamoMapper {
public:
    // Map combining jamo (U+1100-U+11FF) to compatibility jamo (U+3130-U+318F)
    static uint32_t CombiningToCompatibility(uint32_t codepoint);

    // Extract individual jamo from Korean text (decompose syllables)
    static std::vector<uint32_t> ExtractJamo(const std::wstring& text);

    // Check if a character is Korean
    static bool IsKorean(wchar_t ch);
    static bool IsKoreanScalar(uint32_t value);
};

// Audio player for Korean pronunciation
class KoreanAudioPlayer {
public:
    KoreanAudioPlayer();
    ~KoreanAudioPlayer();

    // Play pronunciation for a Korean jamo/syllable
    void Play(const std::wstring& text);

    // Stop current playback
    void Stop();

    // Set the sound directory path
    void SetSoundDir(const std::wstring& dir) { m_soundDir = dir; }

private:
    std::wstring m_soundDir;
    std::mutex m_mutex;

    // Fallback to Windows TTS if audio file not found
    void FallbackToTTS(wchar_t ch);

    // Convert jamo to speakable syllable for TTS
    std::wstring JamoToSpeakable(wchar_t ch);
};

// Input source type for history tracking
enum class InputSource {
    Typed,      // Normal input commit
    Clipboard,  // Clipboard content
    Deleted,    // User deleted (blacklist)
    Stream      // From text stream (disabled)
};

// History item structure
struct InputHistoryItem {
    std::wstring text;
    std::chrono::system_clock::time_point timestamp;
    InputSource source;
    int count;

    InputHistoryItem() : count(1), source(InputSource::Typed) {}
    InputHistoryItem(const std::wstring& t, InputSource s = InputSource::Typed, int c = 1)
        : text(t), source(s), count(c), timestamp(std::chrono::system_clock::now()) {}

    // Calculate max age based on usage count
    std::chrono::seconds MaxAge() const;

    // Check if expired
    bool IsExpired() const;
};

// Input history manager - tracks typed words for recommendations
class InputHistoryManager {
public:
    InputHistoryManager();

    // Load history from file
    std::vector<InputHistoryItem> Load();

    // Save item to history
    std::vector<InputHistoryItem> Save(const InputHistoryItem& item,
                                        std::vector<InputHistoryItem>& existing);

    // Remove deleted status (unblock from blacklist)
    std::vector<InputHistoryItem> RemoveDeleted(const std::wstring& text,
                                                 std::vector<InputHistoryItem>& existing);

    // Get recommendations matching prefix
    struct Recommendation {
        std::wstring text;
        InputSource source;
    };
    std::vector<Recommendation> GetRecommendations(
        const std::wstring& prefix,
        const std::vector<InputHistoryItem>& history,
        int limit);

private:
    std::wstring m_historyFile;
    static const int MAX_ITEMS = 500;
};

// Text stream manager - tracks recent text for context
class TextStreamManager {
public:
    TextStreamManager();

    // Load text stream
    std::wstring Load();

    // Append text to stream
    std::wstring Append(const std::wstring& text);

    // Load blacklist
    std::unordered_set<std::wstring> LoadBlacklist();

    // Add to blacklist
    void AddToBlacklist(const std::wstring& text);

    // Remove from blacklist
    void RemoveFromBlacklist(const std::wstring& text);

private:
    std::wstring m_streamFile;
    std::wstring m_blacklistFile;
    static const int MAX_CHARS = 500;
};

// Main Korean TTS handler - integrates with RimeWithWeasel
class KoreanTTSHandler {
public:
    KoreanTTSHandler();

    // Initialize with Rime user data path
    void Initialize(const std::wstring& userDataPath);

    // Handle preedit update - called when composition changes
    void OnPreeditUpdate(const std::wstring& preedit, const std::wstring& schemaId);

    // Handle commit - called when text is committed
    void OnCommit(const std::wstring& text);

    // Handle backspace tracking for blacklist
    void OnBackspace(bool hasComposition);

    // Reset backspace tracking (called on other key press)
    void ResetBackspaceTracking();

    // Check and record clipboard content
    void CheckClipboard();

    // Get recommendations for current input
    std::vector<InputHistoryManager::Recommendation> GetRecommendations(
        const std::wstring& prefix, int limit);

    // Reload history (call periodically)
    void ReloadHistoryIfNeeded();

    // Check if schema is Korean
    bool IsKoreanSchema(const std::wstring& schemaId);

private:
    KoreanAudioPlayer m_audioPlayer;
    InputHistoryManager m_historyManager;
    TextStreamManager m_textStreamManager;

    std::wstring m_lastSpokenKorean;
    std::vector<InputHistoryItem> m_inputHistory;
    std::chrono::system_clock::time_point m_lastHistoryLoad;

    // Commit tracking for blacklist
    std::wstring m_pendingCommitText;
    std::chrono::system_clock::time_point m_pendingCommitTime;
    int m_backspaceCount;

    // Clipboard tracking
    DWORD m_lastClipboardSequence;

    std::wstring m_userDataPath;
    std::mutex m_mutex;
};

// Utility functions
namespace KoreanUtils {
    // Check if text is pure English/numbers
    bool IsPureEnglishOrNumber(const std::wstring& text);

    // Check for repeating patterns (ABAB)
    bool HasRepeatPattern(const std::wstring& text);

    // Split text by removing English and numbers
    std::vector<std::wstring> SplitByRemovingEnglishAndNumbers(const std::wstring& text);

    // Check if starts with useless prefix (function words)
    bool StartsWithUselessPrefix(const std::wstring& text);

    // Get Rime user data path
    std::wstring GetRimeUserDataPath();
}

} // namespace weasel
