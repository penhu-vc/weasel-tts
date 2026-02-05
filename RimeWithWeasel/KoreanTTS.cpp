// KoreanTTS.cpp - Korean TTS and Smart Recommendation for Weasel
// Ported from macOS Squirrel-TTS by yaja

#include "stdafx.h"
#include "KoreanTTS.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <sapi.h>  // Windows Speech API
#include <ShlObj.h>

#pragma comment(lib, "sapi.lib")

namespace weasel {

// =============================================================================
// JamoMapper Implementation
// =============================================================================

uint32_t JamoMapper::CombiningToCompatibility(uint32_t value) {
    // Initial consonants: U+1100-U+1112 -> U+3131-U+314E
    static const std::unordered_map<uint32_t, uint32_t> initialMap = {
        {0x1100, 0x3131}, {0x1101, 0x3132}, {0x1102, 0x3134}, {0x1103, 0x3137},
        {0x1104, 0x3138}, {0x1105, 0x3139}, {0x1106, 0x3141}, {0x1107, 0x3142},
        {0x1108, 0x3143}, {0x1109, 0x3145}, {0x110A, 0x3146}, {0x110B, 0x3147},
        {0x110C, 0x3148}, {0x110D, 0x3149}, {0x110E, 0x314A}, {0x110F, 0x314B},
        {0x1110, 0x314C}, {0x1111, 0x314D}, {0x1112, 0x314E}
    };

    // Vowels: U+1161-U+1175 -> U+314F-U+3163
    static const std::unordered_map<uint32_t, uint32_t> vowelMap = {
        {0x1161, 0x314F}, {0x1162, 0x3150}, {0x1163, 0x3151}, {0x1164, 0x3152},
        {0x1165, 0x3153}, {0x1166, 0x3154}, {0x1167, 0x3155}, {0x1168, 0x3156},
        {0x1169, 0x3157}, {0x116A, 0x3158}, {0x116B, 0x3159}, {0x116C, 0x315A},
        {0x116D, 0x315B}, {0x116E, 0x315C}, {0x116F, 0x315D}, {0x1170, 0x315E},
        {0x1171, 0x315F}, {0x1172, 0x3160}, {0x1173, 0x3161}, {0x1174, 0x3162},
        {0x1175, 0x3163}
    };

    // Final consonants: U+11A8-U+11C2 -> compatibility jamo
    static const std::unordered_map<uint32_t, uint32_t> finalMap = {
        {0x11A8, 0x3131}, {0x11A9, 0x3132}, {0x11AB, 0x3134}, {0x11AE, 0x3137},
        {0x11AF, 0x3139}, {0x11B7, 0x3141}, {0x11B8, 0x3142}, {0x11BA, 0x3145},
        {0x11BB, 0x3146}, {0x11BC, 0x3147}, {0x11BD, 0x3148}, {0x11BE, 0x314A},
        {0x11BF, 0x314B}, {0x11C0, 0x314C}, {0x11C1, 0x314D}, {0x11C2, 0x314E}
    };

    if (value >= 0x1100 && value <= 0x1112) {
        auto it = initialMap.find(value);
        return it != initialMap.end() ? it->second : value;
    }
    if (value >= 0x1161 && value <= 0x1175) {
        auto it = vowelMap.find(value);
        return it != vowelMap.end() ? it->second : value;
    }
    if (value >= 0x11A8 && value <= 0x11C2) {
        auto it = finalMap.find(value);
        return it != finalMap.end() ? it->second : value;
    }
    return value;
}

std::vector<uint32_t> JamoMapper::ExtractJamo(const std::wstring& text) {
    std::vector<uint32_t> result;

    // Initial consonant jamo values
    static const uint32_t initialJamo[] = {
        0x1100, 0x1101, 0x1102, 0x1103, 0x1104, 0x1105, 0x1106, 0x1107,
        0x1108, 0x1109, 0x110A, 0x110B, 0x110C, 0x110D, 0x110E, 0x110F,
        0x1110, 0x1111, 0x1112
    };

    // Medial vowel jamo values
    static const uint32_t medialJamo[] = {
        0x1161, 0x1162, 0x1163, 0x1164, 0x1165, 0x1166, 0x1167, 0x1168,
        0x1169, 0x116A, 0x116B, 0x116C, 0x116D, 0x116E, 0x116F, 0x1170,
        0x1171, 0x1172, 0x1173, 0x1174, 0x1175
    };

    // Final consonant jamo values
    static const uint32_t finalJamo[] = {
        0, 0x11A8, 0x11A9, 0x11AA, 0x11AB, 0x11AC, 0x11AD, 0x11AE,
        0x11AF, 0x11B0, 0x11B1, 0x11B2, 0x11B3, 0x11B4, 0x11B5, 0x11B6,
        0x11B7, 0x11B8, 0x11B9, 0x11BA, 0x11BB, 0x11BC, 0x11BD, 0x11BE,
        0x11BF, 0x11C0, 0x11C1, 0x11C2
    };

    for (wchar_t ch : text) {
        uint32_t value = static_cast<uint32_t>(ch);

        // Skip spaces
        if (value == 0x0020) continue;

        if (value >= 0xAC00 && value <= 0xD7AF) {
            // Complete syllable - decompose into jamo
            uint32_t syllableIndex = value - 0xAC00;
            uint32_t initialIndex = syllableIndex / 588;
            uint32_t medialIndex = (syllableIndex % 588) / 28;
            uint32_t finalIndex = syllableIndex % 28;

            // Initial consonant
            if (initialIndex < 19) {
                result.push_back(initialJamo[initialIndex]);
            }

            // Medial vowel
            if (medialIndex < 21) {
                result.push_back(medialJamo[medialIndex]);
            }

            // Final consonant (if present)
            if (finalIndex > 0 && finalIndex < 28) {
                result.push_back(finalJamo[finalIndex]);
            }
        }
        else if ((value >= 0x1100 && value <= 0x11FF) ||
                 (value >= 0x3130 && value <= 0x318F)) {
            // Already jamo - add directly
            result.push_back(value);
        }
    }

    return result;
}

bool JamoMapper::IsKorean(wchar_t ch) {
    return IsKoreanScalar(static_cast<uint32_t>(ch));
}

bool JamoMapper::IsKoreanScalar(uint32_t value) {
    return (value >= 0xAC00 && value <= 0xD7AF) ||  // Syllables
           (value >= 0x1100 && value <= 0x11FF) ||  // Combining jamo
           (value >= 0x3130 && value <= 0x318F);    // Compatibility jamo
}

// =============================================================================
// KoreanAudioPlayer Implementation
// =============================================================================

KoreanAudioPlayer::KoreanAudioPlayer() {
    // Default sound directory - will be set by handler
    m_soundDir = L"";
}

KoreanAudioPlayer::~KoreanAudioPlayer() {
    Stop();
}

void KoreanAudioPlayer::Play(const std::wstring& text) {
    if (text.empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    wchar_t ch = text[0];
    uint32_t value = static_cast<uint32_t>(ch);

    // Map to compatibility jamo for filename
    uint32_t mappedValue = JamoMapper::CombiningToCompatibility(value);

    // Build filename (e.g., "3131.mp3" for ㄱ)
    wchar_t filename[32];
    swprintf_s(filename, L"%X.mp3", mappedValue);

    std::wstring audioPath = m_soundDir + L"\\" + filename;

    // Stop any currently playing audio
    Stop();

    // Check if file exists
    if (GetFileAttributesW(audioPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        // Play using Windows Multimedia API
        // Use MCI for better control over MP3 playback
        std::wstring mciCommand = L"open \"" + audioPath + L"\" type mpegvideo alias koreanSound";
        mciSendStringW(mciCommand.c_str(), nullptr, 0, nullptr);
        mciSendStringW(L"play koreanSound", nullptr, 0, nullptr);
    } else {
        // Fallback to TTS
        FallbackToTTS(ch);
    }
}

void KoreanAudioPlayer::Stop() {
    mciSendStringW(L"stop koreanSound", nullptr, 0, nullptr);
    mciSendStringW(L"close koreanSound", nullptr, 0, nullptr);
}

void KoreanAudioPlayer::FallbackToTTS(wchar_t ch) {
    std::wstring speakable = JamoToSpeakable(ch);

    // Use Windows SAPI for TTS
    ISpVoice* pVoice = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                                   IID_ISpVoice, (void**)&pVoice);
    if (SUCCEEDED(hr) && pVoice) {
        // Try to find Korean voice
        ISpObjectTokenCategory* pCategory = nullptr;
        if (SUCCEEDED(SpGetCategoryFromId(SPCAT_VOICES, &pCategory))) {
            IEnumSpObjectTokens* pEnum = nullptr;
            if (SUCCEEDED(pCategory->EnumTokens(L"Language=412", nullptr, &pEnum))) {
                ISpObjectToken* pToken = nullptr;
                if (SUCCEEDED(pEnum->Next(1, &pToken, nullptr)) && pToken) {
                    pVoice->SetVoice(pToken);
                    pToken->Release();
                }
                pEnum->Release();
            }
            pCategory->Release();
        }

        pVoice->Speak(speakable.c_str(), SPF_ASYNC, nullptr);
        pVoice->Release();
    }
}

std::wstring KoreanAudioPlayer::JamoToSpeakable(wchar_t ch) {
    // Map Jamo to pronounceable syllables
    static const std::unordered_map<wchar_t, std::wstring> jamoMap = {
        // Consonants
        {L'\u3131', L"\uADF8"}, {L'\u3134', L"\uB290"}, {L'\u3137', L"\uB4DC"},
        {L'\u3139', L"\uB974"}, {L'\u3141', L"\uBB34"}, {L'\u3142', L"\uBE0C"},
        {L'\u3145', L"\uC2A4"}, {L'\u3147', L"\uC751"}, {L'\u3148', L"\uC988"},
        {L'\u314A', L"\uCE20"}, {L'\u314B', L"\uD06C"}, {L'\u314C', L"\uD2B8"},
        {L'\u314D', L"\uD504"}, {L'\u314E', L"\uD750"},
        // Double consonants
        {L'\u3132', L"\uAED8"}, {L'\u3138', L"\uB5F0"}, {L'\u3143', L"\uBE60"},
        {L'\u3146', L"\uC4F0"}, {L'\u3149', L"\uC9DC"},
        // Vowels
        {L'\u314F', L"\uC544"}, {L'\u3153', L"\uC5B4"}, {L'\u3157', L"\uC624"},
        {L'\u315C', L"\uC6B0"}, {L'\u3161', L"\uC73C"}, {L'\u3163', L"\uC774"},
        {L'\u3150', L"\uC560"}, {L'\u3154', L"\uC5D0"}, {L'\u3151', L"\uC57C"},
        {L'\u3155', L"\uC5EC"}, {L'\u315B', L"\uC694"}, {L'\u3160', L"\uC720"},
        {L'\u3152', L"\uC598"}, {L'\u3156', L"\uC608"}, {L'\u3158', L"\uC640"},
        {L'\u3159', L"\uC65C"}, {L'\u315A', L"\uC678"}, {L'\u315D', L"\uC6CC"},
        {L'\u315E', L"\uC6E8"}, {L'\u315F', L"\uC704"}, {L'\u3162', L"\uC758"}
    };

    auto it = jamoMap.find(ch);
    if (it != jamoMap.end()) {
        return it->second;
    }
    return std::wstring(1, ch);
}

// =============================================================================
// InputHistoryItem Implementation
// =============================================================================

std::chrono::seconds InputHistoryItem::MaxAge() const {
    if (count <= 2) return std::chrono::hours(24);           // 1 day
    if (count <= 9) return std::chrono::hours(24 * 7);       // 7 days
    if (count <= 29) return std::chrono::hours(24 * 30);     // 30 days
    if (count <= 99) return std::chrono::hours(24 * 90);     // 90 days
    return std::chrono::hours(24 * 365);                      // 1 year
}

bool InputHistoryItem::IsExpired() const {
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp);
    return age > MaxAge();
}

// =============================================================================
// InputHistoryManager Implementation
// =============================================================================

InputHistoryManager::InputHistoryManager() {
    m_historyFile = KoreanUtils::GetRimeUserDataPath() + L"\\input_history.json";
}

std::vector<InputHistoryItem> InputHistoryManager::Load() {
    std::vector<InputHistoryItem> items;

    std::wifstream file(m_historyFile);
    if (!file.is_open()) return items;

    // Simple JSON parsing (basic implementation)
    // In production, use nlohmann/json or similar library
    std::wstring line;
    InputHistoryItem current;
    bool inItem = false;

    while (std::getline(file, line)) {
        // Basic parsing - look for "text", "source", "count", "timestamp"
        if (line.find(L"\"text\"") != std::wstring::npos) {
            size_t start = line.find(L"\"", line.find(L":") + 1) + 1;
            size_t end = line.rfind(L"\"");
            if (start < end) {
                current.text = line.substr(start, end - start);
            }
            inItem = true;
        }
        else if (line.find(L"\"count\"") != std::wstring::npos) {
            size_t pos = line.find(L":") + 1;
            current.count = std::stoi(line.substr(pos));
        }
        else if (line.find(L"\"source\"") != std::wstring::npos) {
            if (line.find(L"typed") != std::wstring::npos) current.source = InputSource::Typed;
            else if (line.find(L"clipboard") != std::wstring::npos) current.source = InputSource::Clipboard;
            else if (line.find(L"deleted") != std::wstring::npos) current.source = InputSource::Deleted;
        }
        else if (line.find(L"}") != std::wstring::npos && inItem) {
            if (!current.IsExpired()) {
                items.push_back(current);
            }
            current = InputHistoryItem();
            inItem = false;
        }
    }

    return items;
}

std::vector<InputHistoryItem> InputHistoryManager::Save(
    const InputHistoryItem& item,
    std::vector<InputHistoryItem>& existing) {

    InputHistoryItem newItem = item;

    // Find existing and accumulate count
    auto it = std::find_if(existing.begin(), existing.end(),
        [&item](const InputHistoryItem& e) { return e.text == item.text; });

    if (it != existing.end()) {
        if (item.source != InputSource::Deleted) {
            newItem.count = it->count + 1;
        }
        existing.erase(it);
    }

    // Insert at front
    existing.insert(existing.begin(), newItem);

    // Remove expired
    existing.erase(
        std::remove_if(existing.begin(), existing.end(),
            [](const InputHistoryItem& i) { return i.IsExpired(); }),
        existing.end());

    // Limit size
    if (existing.size() > MAX_ITEMS) {
        existing.resize(MAX_ITEMS);
    }

    // Save to file (simple JSON format)
    std::wofstream file(m_historyFile);
    if (file.is_open()) {
        file << L"[\n";
        for (size_t i = 0; i < existing.size(); ++i) {
            const auto& e = existing[i];
            file << L"  {\n";
            file << L"    \"text\": \"" << e.text << L"\",\n";
            file << L"    \"count\": " << e.count << L",\n";
            std::wstring sourceStr = L"typed";
            if (e.source == InputSource::Clipboard) sourceStr = L"clipboard";
            else if (e.source == InputSource::Deleted) sourceStr = L"deleted";
            file << L"    \"source\": \"" << sourceStr << L"\"\n";
            file << L"  }";
            if (i < existing.size() - 1) file << L",";
            file << L"\n";
        }
        file << L"]\n";
    }

    return existing;
}

std::vector<InputHistoryItem> InputHistoryManager::RemoveDeleted(
    const std::wstring& text,
    std::vector<InputHistoryItem>& existing) {

    existing.erase(
        std::remove_if(existing.begin(), existing.end(),
            [&text](const InputHistoryItem& i) {
                return i.text == text && i.source == InputSource::Deleted;
            }),
        existing.end());

    // Save updated list
    std::wofstream file(m_historyFile);
    if (file.is_open()) {
        file << L"[\n";
        for (size_t i = 0; i < existing.size(); ++i) {
            const auto& e = existing[i];
            file << L"  {\n";
            file << L"    \"text\": \"" << e.text << L"\",\n";
            file << L"    \"count\": " << e.count << L",\n";
            std::wstring sourceStr = L"typed";
            if (e.source == InputSource::Clipboard) sourceStr = L"clipboard";
            else if (e.source == InputSource::Deleted) sourceStr = L"deleted";
            file << L"    \"source\": \"" << sourceStr << L"\"\n";
            file << L"  }";
            if (i < existing.size() - 1) file << L",";
            file << L"\n";
        }
        file << L"]\n";
    }

    return existing;
}

std::vector<InputHistoryManager::Recommendation> InputHistoryManager::GetRecommendations(
    const std::wstring& prefix,
    const std::vector<InputHistoryItem>& history,
    int limit) {

    std::vector<Recommendation> results;

    // Collect blacklist
    std::unordered_set<std::wstring> blacklist;
    for (const auto& item : history) {
        if (item.source == InputSource::Deleted) {
            blacklist.insert(item.text);
        }
    }

    // Match statistics
    struct MatchInfo {
        int count;
        std::chrono::system_clock::time_point lastTime;
        InputSource source;
    };
    std::unordered_map<std::wstring, MatchInfo> matches;

    for (const auto& item : history) {
        if (item.source == InputSource::Deleted || blacklist.count(item.text)) {
            continue;
        }

        // typed needs >= 2 uses, clipboard needs >= 1
        if (item.source == InputSource::Typed && item.count < 2) {
            continue;
        }

        // Prefix match
        if (item.text.find(prefix) == 0 && item.text != prefix) {
            auto& m = matches[item.text];
            m.count++;
            if (item.timestamp > m.lastTime) m.lastTime = item.timestamp;
            m.source = item.source;
        }

        // Substring match
        size_t pos = item.text.find(prefix);
        if (pos != std::wstring::npos && pos > 0) {
            std::wstring substring = item.text.substr(pos);
            if (substring != prefix && substring.length() > prefix.length()) {
                auto& m = matches[substring];
                m.count++;
                if (item.timestamp > m.lastTime) m.lastTime = item.timestamp;
                m.source = item.source;
            }
        }
    }

    // Filter out blacklisted
    for (auto it = matches.begin(); it != matches.end(); ) {
        if (blacklist.count(it->first)) {
            it = matches.erase(it);
        } else {
            ++it;
        }
    }

    // Sort by count and time
    std::vector<std::pair<std::wstring, MatchInfo>> sorted(matches.begin(), matches.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) {
            if (a.second.count != b.second.count) return a.second.count > b.second.count;
            return a.second.lastTime > b.second.lastTime;
        });

    // Take top results
    for (int i = 0; i < std::min(limit, (int)sorted.size()); ++i) {
        results.push_back({sorted[i].first, sorted[i].second.source});
    }

    return results;
}

// =============================================================================
// TextStreamManager Implementation
// =============================================================================

TextStreamManager::TextStreamManager() {
    std::wstring basePath = KoreanUtils::GetRimeUserDataPath();
    m_streamFile = basePath + L"\\text_stream.txt";
    m_blacklistFile = basePath + L"\\blacklist.txt";
}

std::wstring TextStreamManager::Load() {
    std::wifstream file(m_streamFile);
    if (!file.is_open()) return L"";

    std::wstringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::wstring TextStreamManager::Append(const std::wstring& text) {
    std::wstring stream = Load();
    stream += text;

    // Keep only last MAX_CHARS
    if (stream.length() > MAX_CHARS) {
        stream = stream.substr(stream.length() - MAX_CHARS);
    }

    std::wofstream file(m_streamFile);
    if (file.is_open()) {
        file << stream;
    }

    return stream;
}

std::unordered_set<std::wstring> TextStreamManager::LoadBlacklist() {
    std::unordered_set<std::wstring> blacklist;

    std::wifstream file(m_blacklistFile);
    if (!file.is_open()) return blacklist;

    std::wstring line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            blacklist.insert(line);
        }
    }

    return blacklist;
}

void TextStreamManager::AddToBlacklist(const std::wstring& text) {
    auto blacklist = LoadBlacklist();
    blacklist.insert(text);

    std::wofstream file(m_blacklistFile);
    if (file.is_open()) {
        for (const auto& item : blacklist) {
            file << item << L"\n";
        }
    }
}

void TextStreamManager::RemoveFromBlacklist(const std::wstring& text) {
    auto blacklist = LoadBlacklist();
    blacklist.erase(text);

    std::wofstream file(m_blacklistFile);
    if (file.is_open()) {
        for (const auto& item : blacklist) {
            file << item << L"\n";
        }
    }
}

// =============================================================================
// KoreanTTSHandler Implementation
// =============================================================================

KoreanTTSHandler::KoreanTTSHandler()
    : m_backspaceCount(0)
    , m_lastClipboardSequence(0) {
    m_lastHistoryLoad = std::chrono::system_clock::now() - std::chrono::hours(1);
}

void KoreanTTSHandler::Initialize(const std::wstring& userDataPath) {
    m_userDataPath = userDataPath;

    // Set sound directory
    std::wstring soundDir = userDataPath + L"\\korean-sounds";
    m_audioPlayer.SetSoundDir(soundDir);

    // Load initial history
    m_inputHistory = m_historyManager.Load();
    m_lastHistoryLoad = std::chrono::system_clock::now();
}

void KoreanTTSHandler::OnPreeditUpdate(const std::wstring& preedit, const std::wstring& schemaId) {
    if (!IsKoreanSchema(schemaId)) {
        m_lastSpokenKorean.clear();
        return;
    }

    if (preedit.empty()) {
        m_lastSpokenKorean.clear();
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto currentJamo = JamoMapper::ExtractJamo(preedit);
    auto previousJamo = JamoMapper::ExtractJamo(m_lastSpokenKorean);

    // New jamo was added - speak the last one
    if (currentJamo.size() > previousJamo.size()) {
        if (!currentJamo.empty()) {
            wchar_t newJamo = static_cast<wchar_t>(currentJamo.back());
            m_audioPlayer.Play(std::wstring(1, newJamo));
        }
    }
    // Jamo changed (replaced) - speak the last one
    else if (currentJamo.size() == previousJamo.size() && currentJamo != previousJamo) {
        if (!currentJamo.empty()) {
            wchar_t newJamo = static_cast<wchar_t>(currentJamo.back());
            m_audioPlayer.Play(std::wstring(1, newJamo));
        }
    }

    m_lastSpokenKorean = preedit;
}

void KoreanTTSHandler::OnCommit(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_lastSpokenKorean.clear();
    m_backspaceCount = 0;

    // Append to text stream
    m_textStreamManager.Append(text);

    // Handle pending commit (confirm it was correct)
    if (!m_pendingCommitText.empty()) {
        InputHistoryItem item(m_pendingCommitText, InputSource::Typed);
        m_inputHistory = m_historyManager.Save(item, m_inputHistory);

        // Remove from blacklist if previously deleted
        m_textStreamManager.RemoveFromBlacklist(m_pendingCommitText);
        m_inputHistory = m_historyManager.RemoveDeleted(m_pendingCommitText, m_inputHistory);
    }

    // Set new pending commit
    if (text.length() > 1) {
        m_pendingCommitText = text;
        m_pendingCommitTime = std::chrono::system_clock::now();
    } else {
        m_pendingCommitText.clear();
    }
}

void KoreanTTSHandler::OnBackspace(bool hasComposition) {
    if (hasComposition || m_pendingCommitText.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    m_backspaceCount++;

    // Only blacklist if deleted enough characters
    if (m_backspaceCount >= static_cast<int>(m_pendingCommitText.length())) {
        InputHistoryItem item(m_pendingCommitText, InputSource::Deleted);
        m_inputHistory = m_historyManager.Save(item, m_inputHistory);
        m_textStreamManager.AddToBlacklist(m_pendingCommitText);

        m_pendingCommitText.clear();
        m_backspaceCount = 0;
    }
}

void KoreanTTSHandler::ResetBackspaceTracking() {
    m_backspaceCount = 0;
}

void KoreanTTSHandler::CheckClipboard() {
    DWORD sequence = GetClipboardSequenceNumber();
    if (sequence == m_lastClipboardSequence) {
        return;
    }
    m_lastClipboardSequence = sequence;

    if (!OpenClipboard(nullptr)) return;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* text = static_cast<wchar_t*>(GlobalLock(hData));
        if (text) {
            std::wstring clipText(text);
            GlobalUnlock(hData);

            // Record clipboard content if it's long enough
            if (clipText.length() >= 3) {
                std::lock_guard<std::mutex> lock(m_mutex);
                InputHistoryItem item(clipText, InputSource::Clipboard);
                m_inputHistory = m_historyManager.Save(item, m_inputHistory);
            }
        }
    }

    CloseClipboard();
}

std::vector<InputHistoryManager::Recommendation> KoreanTTSHandler::GetRecommendations(
    const std::wstring& prefix, int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_historyManager.GetRecommendations(prefix, m_inputHistory, limit);
}

void KoreanTTSHandler::ReloadHistoryIfNeeded() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastHistoryLoad);

    if (elapsed.count() > 30) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inputHistory = m_historyManager.Load();
        m_lastHistoryLoad = now;
    }
}

bool KoreanTTSHandler::IsKoreanSchema(const std::wstring& schemaId) {
    return schemaId.find(L"hangeul") != std::wstring::npos ||
           schemaId.find(L"korean") != std::wstring::npos ||
           schemaId.find(L"hangul") != std::wstring::npos;
}

// =============================================================================
// KoreanUtils Implementation
// =============================================================================

namespace KoreanUtils {

bool IsPureEnglishOrNumber(const std::wstring& text) {
    for (wchar_t ch : text) {
        if (!((ch >= L'a' && ch <= L'z') ||
              (ch >= L'A' && ch <= L'Z') ||
              (ch >= L'0' && ch <= L'9') ||
              ch == L' ' || ch == L'_' || ch == L'-' || ch == L'.')) {
            return false;
        }
    }
    return true;
}

bool HasRepeatPattern(const std::wstring& text) {
    size_t len = text.length();
    if (len < 4) return false;

    for (size_t subLen = 2; subLen <= len / 2; ++subLen) {
        std::wstring sub = text.substr(0, subLen);
        std::wstring repeated;
        while (repeated.length() < len) {
            repeated += sub;
        }
        if (repeated.substr(0, len) == text) {
            return true;
        }
    }
    return false;
}

std::vector<std::wstring> SplitByRemovingEnglishAndNumbers(const std::wstring& text) {
    std::vector<std::wstring> result;
    std::wstring current;

    for (wchar_t ch : text) {
        bool isEnglishOrNumber = (ch >= L'a' && ch <= L'z') ||
                                  (ch >= L'A' && ch <= L'Z') ||
                                  (ch >= L'0' && ch <= L'9');
        if (isEnglishOrNumber) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += ch;
        }
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

bool StartsWithUselessPrefix(const std::wstring& text) {
    if (text.empty()) return false;

    // Common Chinese function words that shouldn't start recommendations
    static const std::unordered_set<wchar_t> uselessPrefixes = {
        L'\u7684', L'\u662F', L'\u4E86', L'\u6709', L'\u5728', L'\u548C',
        L'\u6211', L'\u4F60', L'\u4ED6', L'\u5979', L'\u5B83', L'\u9019',
        L'\u90A3', L'\u5C31', L'\u90FD', L'\u4E5F', L'\u8981', L'\u6703',
        L'\u80FD', L'\u53EF'
    };

    return uselessPrefixes.count(text[0]) > 0;
}

std::wstring GetRimeUserDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::wstring(path) + L"\\Rime";
    }
    return L"";
}

} // namespace KoreanUtils

} // namespace weasel
