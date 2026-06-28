// Resource management for bitmaps and sounds using Win32 GDI.

#pragma once

// clang-format off
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#pragma comment(lib, "winmm.lib")
// clang-format on

#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#define MAIN_ICON 101

// This macro is defined in windows.h, so we get rid of it.
#ifdef max
# undef max
#endif

using ImageId = std::uint32_t;
using Font = std::unordered_map<char, ImageId>;
using SoundId = std::uint32_t;

// Special images.
namespace ImageType {
constexpr ImageId IMG_TEXT = std::numeric_limits<std::uint32_t>::max() - 1;
constexpr ImageId IMG_PROGBAR = std::numeric_limits<std::uint32_t>::max() - 2;
}  // namespace ImageType

class BitmapManager {
private:
  std::unordered_map<ImageId, HBITMAP> bmps;
  std::unordered_map<ImageId, std::pair<int, int>> dims;
  ImageId next_id = 1;

public:
  ~BitmapManager() {
    for (auto& [_, bmp] : bmps)
      if (bmp) DeleteObject(bmp);
    bmps.clear();
    dims.clear();
  }

  ImageId load(const std::wstring& filename) {
    HBITMAP hbmp = (HBITMAP)LoadImage(
      nullptr,
      filename.c_str(),
      IMAGE_BITMAP,
      0,
      0,
      LR_LOADFROMFILE | LR_CREATEDIBSECTION
    );

    if (!hbmp) {
      MessageBox(nullptr, (L"Cannot load image " + filename).c_str(), L"Error", MB_OK);
      return 0;
    }

    ImageId id = next_id++;
    bmps[id] = hbmp;

    BITMAP bmp;
    GetObject(hbmp, sizeof(bmp), &bmp);
    dims[id] = {bmp.bmWidth, bmp.bmHeight};

    return id;
  }

  void unload(ImageId id) {
    auto it = bmps.find(id);
    if (it == bmps.end()) return;
    DeleteObject(it->second);
    bmps.erase(it);
  }

  std::tuple<HBITMAP, int, int> get(ImageId id) const {
    auto it = bmps.find(id);
    return it == bmps.end() ? std::make_tuple(nullptr, 0, 0)
                            : std::make_tuple(it->second, dims.at(id).first, dims.at(id).second);
  }
};

class SoundManager {
private:
  struct WaveBuffer {
    WAVEFORMATEX format{};
    std::vector<BYTE> data;
  };

  std::unordered_map<SoundId, WaveBuffer> sounds;
  SoundId next_id = 1;

  std::mutex bgm_mutex;
  HWAVEOUT bgm_device = nullptr;
  WAVEHDR bgm_header{};
  WaveBuffer bgm_buffer;

  static void show_error(const std::wstring& message) {
    MessageBox(nullptr, message.c_str(), L"Error", MB_OK);
  }

  static uint32_t read_u32_le(const BYTE* ptr) {
    return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
           (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
  }

  static bool load_wave_file(const std::wstring& filename, WaveBuffer& buffer) {
    HANDLE file = CreateFileW(
      filename.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
      show_error(L"Cannot open sound file " + filename);
      return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 44 ||
        size.QuadPart > static_cast<LONGLONG>(std::numeric_limits<DWORD>::max())) {
      CloseHandle(file);
      show_error(L"Invalid wave file " + filename);
      return false;
    }

    std::vector<BYTE> raw(static_cast<size_t>(size.QuadPart));
    DWORD bytes_read = 0;
    if (!ReadFile(file, raw.data(), static_cast<DWORD>(raw.size()), &bytes_read, nullptr) ||
        bytes_read != raw.size()) {
      CloseHandle(file);
      show_error(L"Cannot read wave file " + filename);
      return false;
    }
    CloseHandle(file);

    if (std::memcmp(raw.data(), "RIFF", 4) != 0 || std::memcmp(raw.data() + 8, "WAVE", 4) != 0) {
      show_error(L"Unsupported wave header in " + filename);
      return false;
    }

    const BYTE* cursor = raw.data() + 12;
    size_t remaining = raw.size() - 12;

    const BYTE* fmt_section = nullptr;
    uint32_t fmt_size = 0;
    const BYTE* data_section = nullptr;
    uint32_t data_size = 0;

    while (remaining >= 8) {
      const BYTE* chunk_id = cursor;
      uint32_t chunk_size = read_u32_le(cursor + 4);
      cursor += 8;
      remaining -= 8;

      if (chunk_size > remaining) break;

      if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
        fmt_section = cursor;
        fmt_size = chunk_size;
      } else if (std::memcmp(chunk_id, "data", 4) == 0) {
        data_section = cursor;
        data_size = chunk_size;
      }

      cursor += chunk_size;
      remaining -= chunk_size;

      if (chunk_size & 1) {
        if (remaining == 0) break;
        ++cursor;
        --remaining;
      }
    }

    if (fmt_section == nullptr || data_section == nullptr) {
      show_error(L"Missing wave chunks in " + filename);
      return false;
    }

    if (fmt_size < sizeof(PCMWAVEFORMAT)) {
      show_error(L"Unsupported fmt chunk in " + filename);
      return false;
    }

    PCMWAVEFORMAT pcm{};
    std::memcpy(&pcm, fmt_section, sizeof(PCMWAVEFORMAT));
    buffer.format.wFormatTag = pcm.wf.wFormatTag;
    buffer.format.nChannels = pcm.wf.nChannels;
    buffer.format.nSamplesPerSec = pcm.wf.nSamplesPerSec;
    buffer.format.nAvgBytesPerSec = pcm.wf.nAvgBytesPerSec;
    buffer.format.nBlockAlign = pcm.wf.nBlockAlign;
    buffer.format.wBitsPerSample = pcm.wBitsPerSample;

    if (buffer.format.wFormatTag != WAVE_FORMAT_PCM &&
        buffer.format.wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
      show_error(L"Unsupported wave format in " + filename);
      return false;
    }

    size_t extra = fmt_size > sizeof(PCMWAVEFORMAT) ? fmt_size - sizeof(PCMWAVEFORMAT) : 0;
    if (extra > std::numeric_limits<WORD>::max()) extra = std::numeric_limits<WORD>::max();
    buffer.format.cbSize = static_cast<WORD>(extra);

    if (data_size == 0 || data_size > std::numeric_limits<DWORD>::max()) {
      show_error(L"Invalid data chunk in " + filename);
      return false;
    }

    buffer.data.assign(data_section, data_section + data_size);
    return true;
  }

  static void report_wave_error(MMRESULT result, const std::wstring& context) {
    wchar_t buffer[256] = {};
    if (waveOutGetErrorTextW(result, buffer, 256) != MMSYSERR_NOERROR) {
      lstrcpynW(buffer, L"Unknown waveOut error", 256);
    }
    show_error(context + L": " + buffer);
  }

  void cleanup_bgm_locked() {
    if (!bgm_device) {
      bgm_buffer = WaveBuffer{};
      std::memset(&bgm_header, 0, sizeof(bgm_header));
      return;
    }

    waveOutReset(bgm_device);
    if (bgm_header.dwFlags & WHDR_PREPARED) {
      waveOutUnprepareHeader(bgm_device, &bgm_header, sizeof(bgm_header));
    }
    waveOutClose(bgm_device);
    bgm_device = nullptr;
    bgm_buffer = WaveBuffer{};
    std::memset(&bgm_header, 0, sizeof(bgm_header));
  }

public:
  ~SoundManager() { stop(); }

  SoundId load(const std::wstring& filename) {
    WaveBuffer buffer;
    if (!load_wave_file(filename, buffer)) return 0;

    SoundId id = next_id++;
    sounds[id] = std::move(buffer);
    return id;
  }

  void play_loop(SoundId sound_id) {
    if (sound_id == 0) return;

    auto it = sounds.find(sound_id);
    if (it == sounds.end()) {
      show_error(L"Invalid sound handle");
      return;
    }

    WaveBuffer new_buffer = it->second;

    std::lock_guard<std::mutex> lock(bgm_mutex);
    cleanup_bgm_locked();

    MMRESULT open_result =
      waveOutOpen(&bgm_device, WAVE_MAPPER, &new_buffer.format, 0, 0, CALLBACK_NULL);
    if (open_result != MMSYSERR_NOERROR) {
      report_wave_error(
        open_result,
        std::wstring(L"Cannot open audio device for sound #") + std::to_wstring(sound_id)
      );
      return;
    }

    bgm_buffer = std::move(new_buffer);
    std::memset(&bgm_header, 0, sizeof(bgm_header));
    bgm_header.lpData = reinterpret_cast<LPSTR>(bgm_buffer.data.data());
    bgm_header.dwBufferLength = static_cast<DWORD>(bgm_buffer.data.size());
    bgm_header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    bgm_header.dwLoops = 0xFFFFFFFF;

    MMRESULT prepare_result = waveOutPrepareHeader(bgm_device, &bgm_header, sizeof(bgm_header));
    if (prepare_result != MMSYSERR_NOERROR) {
      report_wave_error(
        prepare_result,
        std::wstring(L"Cannot prepare BGM for sound #") + std::to_wstring(sound_id)
      );
      cleanup_bgm_locked();
      return;
    }

    MMRESULT write_result = waveOutWrite(bgm_device, &bgm_header, sizeof(bgm_header));
    if (write_result != MMSYSERR_NOERROR) {
      report_wave_error(
        write_result,
        std::wstring(L"Cannot play BGM for sound #") + std::to_wstring(sound_id)
      );
      cleanup_bgm_locked();
    }
  }

  void play_once(SoundId sound_id) {
    if (sound_id == 0) return;

    auto it = sounds.find(sound_id);
    if (it == sounds.end()) {
      show_error(L"Invalid sound handle");
      return;
    }

    WaveBuffer buffer = it->second;

    std::thread([buffer = std::move(buffer), sound_id]() mutable {
      HWAVEOUT device = nullptr;
      MMRESULT open_result = waveOutOpen(&device, WAVE_MAPPER, &buffer.format, 0, 0, CALLBACK_NULL);
      if (open_result != MMSYSERR_NOERROR) {
        report_wave_error(
          open_result,
          std::wstring(L"Cannot open audio device for sound #") + std::to_wstring(sound_id)
        );
        return;
      }

      WAVEHDR header{};
      header.lpData = reinterpret_cast<LPSTR>(buffer.data.data());
      header.dwBufferLength = static_cast<DWORD>(buffer.data.size());

      MMRESULT prepare_result = waveOutPrepareHeader(device, &header, sizeof(header));
      if (prepare_result != MMSYSERR_NOERROR) {
        report_wave_error(
          prepare_result,
          std::wstring(L"Cannot prepare sound #") + std::to_wstring(sound_id)
        );
        waveOutClose(device);
        return;
      }

      MMRESULT write_result = waveOutWrite(device, &header, sizeof(header));
      if (write_result != MMSYSERR_NOERROR) {
        report_wave_error(
          write_result,
          std::wstring(L"Cannot play sound #") + std::to_wstring(sound_id)
        );
        waveOutUnprepareHeader(device, &header, sizeof(header));
        waveOutClose(device);
        return;
      }

      while ((header.dwFlags & WHDR_DONE) == 0) Sleep(5);

      waveOutUnprepareHeader(device, &header, sizeof(header));
      waveOutClose(device);
    }).detach();
  }

  void stop() {
    std::lock_guard<std::mutex> lock(bgm_mutex);
    cleanup_bgm_locked();
  }
};
