# Trumpagotchi audio sample budget

## Voice build measured 12 October 2026

The firmware now contains all twelve selected voice reactions as read-only mono PCM16 at 16 kHz. Total payload is **413,418 bytes**, or **12.9193125 seconds**. The instrument and output still run at 44.1 kHz; playback resamples without changing pitch or duration.

| Resource | Used | Limit | Remaining |
| --- | ---: | ---: | ---: |
| Application image | 517,856 B | 581,564 B | **63,708 B (62.21 KiB)** |
| Data and BSS | 26,012 B | 98,304 B | 72,292 B |
| Large-buffer pool | 74,880 B | 344,064 B | 269,184 B |

The original 64 KiB growth reserve was provisional. The finished voice build leaves about 60 KiB, within the actual app slot. Target compilation passes its memory, RAM-only flash-code and no-soft-double checks. The fixed 609,657-byte update container is not the occupied application size.

Samples stay in flash; the player uses a bounded cursor rather than copying the bank into RAM. Only one recorded reaction plays at a time. Speech is mixed dry and temporarily lowers the instrument mix. The keyboard is now dry. Removing echo and plate processing frees223,508bytes of RAM pool, reducing its use to74,880bytes. This RAM saving is separate from the application flash space available for voice clips.

## Quality and preparation

Original exports are 48 kHz mono PCM16. They are preserved unchanged in samples/voice-originals/. The voice importer uses windowed-sinc low-pass resampling to 16 kHz, removes DC, only reduces excessive peaks, and applies brief boundary fades. It preserves complete clip duration, with no speed-up or silence trimming.

The current bank uses uncompressed PCM. ADPCM is not implemented. Lower rates or compression remain possible later, but all twelve clips currently fit without them. Final audio quality and target CPU deadlines still need a physical device check.

## Reproduction and provenance

Run python3 tools/import_voice_samples.py to regenerate converted WAVs and firmware/src/dsp/voice_bank.h. voice_bank.json records the exact source and converted-payload hashes, durations and trigger mappings. The retained Fart Machine One importer defaults to 22.05 kHz and is not the voice-bank path.

Do not count calibration, persistent pet storage or OTA staging as additional sample space. Those regions retain their existing ownership. See VOICE-MAPPING.md for triggers and playback limits.
