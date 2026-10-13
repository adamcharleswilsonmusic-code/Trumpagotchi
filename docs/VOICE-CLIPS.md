# Trumpagotchi voice collection plan

Updated 12 October 2026. The original shopping list below is retained for reference. Adam has now supplied twelve exported clips and their mappings; they are bundled in the installed FM-1_703 voice build and current local FM-1_704 revision. See [implemented sound mapping](VOICE-MAPPING.md) for the actual selected bank. Source URLs have not been assigned to individual exports.

## Sources

These DVIDS recording pages explicitly show Public Domain (checked 12 October 2026):

- [Congress address, 4 March 2025](https://www.dvidshub.net/video/954789/president-trump-addresses-joint-session-congress) — 1:46:01; try for approval and boasting.
- [Quantico speech, 30 September 2025](https://www.dvidshub.net/video/984058/trump-speaks-marine-corps-base-quantico) — 1:13:20; try for exaggerated praise and complaints.
- [President Trump Talks to Press, 6 January 2019](https://www.dvidshub.net/video/653573/president-trump-talks-press) — 3:41, White House Communications Agency; try first for brief refusals and irritation.
- [Thanksgiving visit to troops, November 2019](https://www.dvidshub.net/video/725456/trump-thanksgiving-visit-troops-afghanistan) — 39:16; try for upbeat greetings and appreciative reactions. Page date29November, description event28November.

Suggested usefulness is an editorial inference from event type, not confirmation of any phrase in its audio. Keep the official recording source URL with every selected clip.

## Essential bank: 12 clips

| Situation | Count | Delivery to collect | Reuse |
| --- | ---: | --- | --- |
| Burger / snack approval | 2 | Short enthusiastic approval, one milder alternate | Manual feeding and self-served snacks |
| Praise celebration | 2 | Smug thanks or self-congratulation | Happy care animation |
| News frustration | 2 | Brief annoyed complaint or dismissive reaction | Sulking / bad mood can reuse |
| Action refused / cooldown | 1 | Short no or impatient response | Full mood, cooldown and other refused care |
| Hungry | 1 | Demanding or impatient call | Low food; occasional notification |
| Tired / bedtime | 1 | Low-energy goodbye or weary reaction | Nap and bedtime; synthesized snoring if wanted |
| Cleaned room | 1 | Brief approving reaction | CLEAN after wrappers or poo |
| Music / bongos | 1 | Delighted approval or brief celebratory exclamation | Only while user plays; avoid repeating every note |
| Morning / wake | 1 | Upbeat greeting or return-to-business delivery | Dawn and daytime waking |

## Optional extras: 4 clips

- Bored: attention-seeking complaint.
- Sulking: more indignant complaint than the news variants.
- Poo: brief grunt or sheepish exclamation.
- Overfed: satisfied or exaggerated reaction to an extra burger.

These make 16 clips in total. Reuse is enough for the core build; no dedicated clip is needed for each walking or sitting frame. All examples describe desired delivery, not quotations verified in these recordings. Any joined-word dialogue must be presented as edited fictional game dialogue, not genuine quotations or endorsement.

## Collection and playback constraints

Aim initially for roughly12seconds total across selected clips, mostly0.4–1.0seconds each. Keep complete, intelligible words; longer usable takes are preferable to damaged trims. Preserve raw originals and source URL, exact in/out timestamps, exact spoken text, duration, reaction category and public-domain notice for each candidate. Choose one clean clip per essential situation before searching for alternates.

The selected twelve-clip bank is now implemented and measured: 12.9193125seconds, 413,418bytes at16kHz mono PCM16. The current app leaves61,712bytes of headroom. The old shopping-list estimate has been superseded by that measured build. ADPCM could allow longer clips later but is not implemented. See SAMPLE-BUDGET.md.

Play one reaction at a time; suppress idle voice during sleep and avoid repeated calls on every timer tick. Prioritize explicit user care over idle complaints. Music reactions should not repeatedly cover the keyboard or bongos. Voice hooks and throttling are now implemented; the actual behavior is documented in VOICE-MAPPING.md.
