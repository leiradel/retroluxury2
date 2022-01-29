#include "rl2_sound.h"
#include "rl2_heap.h"
#include "rl2_log.h"

#include <speex_resampler.h>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#include <inttypes.h>

#define RL2_SAMPLE_RATE 44100
#define RL2_SAMPLES_PER_VIDEO_FRAME (RL2_SAMPLE_RATE / 60)

#define TAG "SND "

typedef int16_t rl2_Sample;

// Make sure our sample has 16 bits
typedef char rl2_staticAssertSoundSampleMustBeInt16[sizeof(rl2_Sample) == sizeof(int16_t) ? 1 : -1];
// Make sure Speex sample has the same number of bits as we have
typedef char rl2_staticAssertSpeexSampleMustBeHh2Sample[sizeof(spx_int16_t) == sizeof(rl2_Sample) ? 1 : -1];

typedef enum {
    RL2_WAV,
    RL2_VORBIS
}
rl2_SoundType;

struct rl2_Sound {
    rl2_SoundType type;

    union {
        struct {
            rl2_Sample* frames;
            size_t num_frames;
        }
        wav;

        struct {
            rl2_File file;
        }
        vorbis;
    }
    source;
};

struct rl2_Voice {
    rl2_Sound sound;
    uint8_t volume;
    bool repeat;
    rl2_Finished finished_cb;

    rl2_Voice previous;
    rl2_Voice next;

    union {
        struct {
            size_t position;
        }
        wav;

        struct {
            OggVorbis_File ogg;
            long position;
        }
        vorbis;
    }
    state;
};

static int16_t rl2_audioFrames[RL2_SAMPLES_PER_VIDEO_FRAME * 2];
static rl2_Voice rl2_voiceList = NULL;

static bool rl2_resample(
    spx_uint32_t const in_rate,
    spx_int16_t const* const in_data, spx_uint32_t in_samples,
    spx_int16_t* const out_data, spx_uint32_t out_samples) {

    RL2_INFO(
        TAG "resampling from %u Hz to %d (%" PRIu32 " samples in, %" PRIu32 " samples out",
        in_rate, RL2_SAMPLE_RATE, in_samples, out_samples
    );

    int error;
    SpeexResamplerState* const resampler = speex_resampler_init(
        1, in_rate, RL2_SAMPLE_RATE, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

    if (resampler == NULL) {
        RL2_ERROR(TAG "error initializing resampler: %s", speex_resampler_strerror(error));
        return false;
    }

    error = speex_resampler_process_interleaved_int(resampler, in_data, &in_samples, out_data, &out_samples);

    if (error != RESAMPLER_ERR_SUCCESS) {
        RL2_ERROR(TAG "error resampling: %s", speex_resampler_strerror(error));
        speex_resampler_destroy(resampler);
        return false;
    }

    speex_resampler_destroy(resampler);
    return true;
}

static void rl2_addVoice(rl2_Voice const voice) {
    if (rl2_voiceList != NULL) {
        rl2_voiceList->previous = voice;
    }

    voice->next = rl2_voiceList;
    voice->previous = NULL;
    rl2_voiceList = voice;
}

static void rl2_removeVoice(rl2_Voice const voice) {
    if (voice->previous != NULL) {
        voice->previous->next = voice->next;
    }

    if (voice->next != NULL) {
        voice->next->previous = voice->previous;
    }

    if (rl2_voiceList == voice) {
        rl2_voiceList = voice->next;
    }

    rl2_free(voice);
}

// ##      ##    ###    ##     ## 
// ##  ##  ##   ## ##   ##     ## 
// ##  ##  ##  ##   ##  ##     ## 
// ##  ##  ## ##     ## ##     ## 
// ##  ##  ## #########  ##   ##  
// ##  ##  ## ##     ##   ## ##   
//  ###  ###  ##     ##    ###    

static size_t rl2_drwavRead(void* const userdata, void* const buffer, size_t const count) {
    rl2_File const file = (rl2_File)userdata;
    return rl2_read(file, buffer, count);
}

static drwav_bool32 rl2_drwavSeek(void* const userdata, int const offset, drwav_seek_origin const origin) {
    rl2_File const file = (rl2_File)userdata;
    return rl2_seek(file, offset, origin == drwav_seek_origin_start ? SEEK_SET : SEEK_CUR) == 0;
}

static char const* rl2_drwavError(drwav_result const error) {
    switch (error) {
        case DRWAV_SUCCESS: return "DRWAV_SUCCESS";
        case DRWAV_ERROR: return "DRWAV_ERROR";
        case DRWAV_INVALID_ARGS: return "DRWAV_INVALID_ARGS";
        case DRWAV_INVALID_OPERATION: return "DRWAV_INVALID_OPERATION";
        case DRWAV_OUT_OF_MEMORY: return "DRWAV_OUT_OF_MEMORY";
        case DRWAV_OUT_OF_RANGE: return "DRWAV_OUT_OF_RANGE";
        case DRWAV_ACCESS_DENIED: return "DRWAV_ACCESS_DENIED";
        case DRWAV_DOES_NOT_EXIST: return "DRWAV_DOES_NOT_EXIST";
        case DRWAV_ALREADY_EXISTS: return "DRWAV_ALREADY_EXISTS";
        case DRWAV_TOO_MANY_OPEN_FILES: return "DRWAV_TOO_MANY_OPEN_FILES";
        case DRWAV_INVALID_FILE: return "DRWAV_INVALID_FILE";
        case DRWAV_TOO_BIG: return "DRWAV_TOO_BIG";
        case DRWAV_PATH_TOO_LONG: return "DRWAV_PATH_TOO_LONG";
        case DRWAV_NAME_TOO_LONG: return "DRWAV_NAME_TOO_LONG";
        case DRWAV_NOT_DIRECTORY: return "DRWAV_NOT_DIRECTORY";
        case DRWAV_IS_DIRECTORY: return "DRWAV_IS_DIRECTORY";
        case DRWAV_DIRECTORY_NOT_EMPTY: return "DRWAV_DIRECTORY_NOT_EMPTY";
        case DRWAV_END_OF_FILE: return "DRWAV_END_OF_FILE";
        case DRWAV_NO_SPACE: return "DRWAV_NO_SPACE";
        case DRWAV_BUSY: return "DRWAV_BUSY";
        case DRWAV_IO_ERROR: return "DRWAV_IO_ERROR";
        case DRWAV_INTERRUPT: return "DRWAV_INTERRUPT";
        case DRWAV_UNAVAILABLE: return "DRWAV_UNAVAILABLE";
        case DRWAV_ALREADY_IN_USE: return "DRWAV_ALREADY_IN_USE";
        case DRWAV_BAD_ADDRESS: return "DRWAV_BAD_ADDRESS";
        case DRWAV_BAD_SEEK: return "DRWAV_BAD_SEEK";
        case DRWAV_BAD_PIPE: return "DRWAV_BAD_PIPE";
        case DRWAV_DEADLOCK: return "DRWAV_DEADLOCK";
        case DRWAV_TOO_MANY_LINKS: return "DRWAV_TOO_MANY_LINKS";
        case DRWAV_NOT_IMPLEMENTED: return "DRWAV_NOT_IMPLEMENTED";
        case DRWAV_NO_MESSAGE: return "DRWAV_NO_MESSAGE";
        case DRWAV_BAD_MESSAGE: return "DRWAV_BAD_MESSAGE";
        case DRWAV_NO_DATA_AVAILABLE: return "DRWAV_NO_DATA_AVAILABLE";
        case DRWAV_INVALID_DATA: return "DRWAV_INVALID_DATA";
        case DRWAV_TIMEOUT: return "DRWAV_TIMEOUT";
        case DRWAV_NO_NETWORK: return "DRWAV_NO_NETWORK";
        case DRWAV_NOT_UNIQUE: return "DRWAV_NOT_UNIQUE";
        case DRWAV_NOT_SOCKET: return "DRWAV_NOT_SOCKET";
        case DRWAV_NO_ADDRESS: return "DRWAV_NO_ADDRESS";
        case DRWAV_BAD_PROTOCOL: return "DRWAV_BAD_PROTOCOL";
        case DRWAV_PROTOCOL_UNAVAILABLE: return "DRWAV_PROTOCOL_UNAVAILABLE";
        case DRWAV_PROTOCOL_NOT_SUPPORTED: return "DRWAV_PROTOCOL_NOT_SUPPORTED";
        case DRWAV_PROTOCOL_FAMILY_NOT_SUPPORTED: return "DRWAV_PROTOCOL_FAMILY_NOT_SUPPORTED";
        case DRWAV_ADDRESS_FAMILY_NOT_SUPPORTED: return "DRWAV_ADDRESS_FAMILY_NOT_SUPPORTED";
        case DRWAV_SOCKET_NOT_SUPPORTED: return "DRWAV_SOCKET_NOT_SUPPORTED";
        case DRWAV_CONNECTION_RESET: return "DRWAV_CONNECTION_RESET";
        case DRWAV_ALREADY_CONNECTED: return "DRWAV_ALREADY_CONNECTED";
        case DRWAV_NOT_CONNECTED: return "DRWAV_NOT_CONNECTED";
        case DRWAV_CONNECTION_REFUSED: return "DRWAV_CONNECTION_REFUSED";
        case DRWAV_NO_HOST: return "DRWAV_NO_HOST";
        case DRWAV_IN_PROGRESS: return "DRWAV_IN_PROGRESS";
        case DRWAV_CANCELLED: return "DRWAV_CANCELLED";
        case DRWAV_MEMORY_ALREADY_MAPPED: return "DRWAV_MEMORY_ALREADY_MAPPED";
        case DRWAV_AT_END: return "DRWAV_AT_END";
        default: return "unknown drwav error";
    }
}

static rl2_Sound rl2_wavRead(rl2_File const file) {
    rl2_Sound sound = (rl2_Sound)rl2_alloc(sizeof(*sound));

    if (sound == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    drwav wav;

    if (!drwav_init(&wav, rl2_drwavRead, rl2_drwavSeek, file, NULL)) {
        RL2_ERROR(TAG "error loading WAV: %s", rl2_drwavError(drwav_uninit(&wav)));
error1:
        rl2_close(file);
        return NULL;
    }

    if (wav.channels > 2) {
        RL2_ERROR(TAG "too many channels in WAV: %u, we only support mono and stereo", wav.channels);
error2:
        drwav_uninit(&wav);
        goto error1;
    }

    sound->type = RL2_WAV;
    sound->source.wav.num_frames = wav.totalPCMFrameCount;
    sound->source.wav.frames = (rl2_Sample*)rl2_alloc(sound->source.wav.num_frames * sizeof(rl2_Sample) * 2);

    if (sound->source.wav.frames == NULL) {
        RL2_ERROR(TAG "out of memory");
        goto error3;
    }

    for (size_t i = 0; i < wav.totalPCMFrameCount; i++) {
        drwav_int16 frame[2];
        drwav_uint64 const num_read = drwav_read_pcm_frames_s16(&wav, 1, frame);

        if (num_read != 1) {
            RL2_ERROR(TAG "error reading samples: %s", rl2_drwavError(drwav_uninit(&wav)));
error3:
            rl2_free((void*)sound->source.wav.frames);
            goto error2;
        }

        sound->source.wav.frames[i * 2] = frame[0];
        sound->source.wav.frames[i * 2 + 1] = wav.channels == 1 ? frame[0] : frame[1];
    }

    drwav_uninit(&wav);
    rl2_close(file);

    if (wav.sampleRate != RL2_SAMPLE_RATE) {
        size_t const num_resampled = wav.totalPCMFrameCount * RL2_SAMPLE_RATE / wav.sampleRate;
        rl2_Sample* resampled = (rl2_Sample*)rl2_alloc(num_resampled * sizeof(rl2_Sample) * 2);

        if (resampled == NULL) {
            RL2_ERROR(TAG "out of memory");
            goto error3;
        }

        if (!rl2_resample(wav.sampleRate, sound->source.wav.frames, sound->source.wav.num_frames, resampled, num_resampled)) {
            // Error already logged
            rl2_free(resampled);
            goto error3;
        }

        rl2_free(sound->source.wav.frames);
        sound->source.wav.frames = resampled;
        sound->source.wav.num_frames = num_resampled;
    }

    return sound;
}

static void rl2_wavDestroy(rl2_Sound const sound) {
    rl2_free(sound->source.wav.frames);
    rl2_free(sound);
}

static rl2_Voice rl2_wavPlay(rl2_Sound const sound, uint8_t const volume, bool const repeat, rl2_Finished finished_cb)
{
    rl2_Voice voice = (rl2_Voice)rl2_alloc(sizeof(*voice));

    if (voice == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    voice->sound = sound;
    voice->volume = volume;
    voice->repeat = repeat;
    voice->finished_cb = finished_cb;
    voice->state.wav.position = 0;

    rl2_addVoice(voice);
    return voice;
}

static void rl2_wavStop(rl2_Voice const voice) {
    if (voice->finished_cb != NULL) {
        voice->finished_cb(voice);
    }

    rl2_removeVoice(voice);
}

static void rl2_wavMix(rl2_Voice const voice, int32_t* const buffer, size_t const num_frames)
{
    size_t const available_frames = voice->sound->source.wav.num_frames - voice->state.wav.position;
    size_t const frames_to_mix = available_frames < num_frames ? available_frames : num_frames;
    rl2_Sample const* const frames = voice->sound->source.wav.frames + voice->state.wav.position * 2;
    int32_t const volume = voice->volume + (voice->volume >= 128);

    for (size_t i = 0; i < frames_to_mix * 2; i++) {
        int32_t const sample = frames[i] * volume / 256;
        buffer[i] += sample;
    }

    voice->state.wav.position += frames_to_mix;

    if (voice->state.wav.position == voice->sound->source.wav.num_frames) {
        if (voice->repeat) {
            voice->state.wav.position = 0;

            if (frames_to_mix < num_frames) {
                rl2_wavMix(voice, buffer + frames_to_mix * 2, num_frames - frames_to_mix);
            }
        }
        else {
            if (voice->finished_cb != NULL) {
                voice->finished_cb(voice);
            }

            rl2_removeVoice(voice);
        }
    }
}

// ##     ##  #######  ########  ########  ####  ######  
// ##     ## ##     ## ##     ## ##     ##  ##  ##    ## 
// ##     ## ##     ## ##     ## ##     ##  ##  ##       
// ##     ## ##     ## ########  ########   ##   ######  
//  ##   ##  ##     ## ##   ##   ##     ##  ##        ## 
//   ## ##   ##     ## ##    ##  ##     ##  ##  ##    ## 
//    ###     #######  ##     ## ########  ####  ######  

#if 0
static rl2_Sound rl2_readVorbis(rl2_Filesys filesys, char const* path) {
    rl2_Sound sound = (rl2_Sound)rl2_alloc(sizeof(*sound));

    if (sound == NULL) {
        RL2_ERROR(TAG "out of memory");
        return NULL;
    }

    rl2_File const file = rl2_openFile(filesys, path);

    if (file == NULL) {
        // Error already logged
error1:
        rl2_free(sound);
        return NULL;
    }

    sound->type = RL2_VORBIS;
    sound->source.vorbis.file = file;
    return sound;
}

void hh2_destroyPcm(hh2_Pcm pcm) {
    for (unsigned i = 0; i < RL2_MAX_VOICES; i++) {
        if (hh2_voices[i].pcm == pcm) {
            hh2_voices[i].pcm = NULL;
            hh2_voices[i].position = 0;
        }
    }

    free(pcm);
}

bool hh2_playPcm(hh2_Pcm pcm) {
    for (unsigned i = 0; i < RL2_MAX_VOICES; i++) {
        if (hh2_voices[i].pcm == NULL) {
            hh2_voices[i].pcm = pcm;
            hh2_voices[i].position = 0;
            return true;
        }
    }

    return false;
}

void hh2_stopPcms(void) {
    for (unsigned i = 0; i < RL2_MAX_VOICES; i++) {
        hh2_voices[i].pcm = NULL;
        hh2_voices[i].position = 0;
    }
}

static void hh2_mixPcm(int32_t* const buffer, hh2_Voice* const voice) {
    size_t const buffer_free = RL2_SAMPLES_PER_VIDEO_FRAME;
    hh2_Pcm const pcm = voice->pcm;

    size_t const available = pcm->sample_count - voice->position;
    hh2_Sample const* const sample = pcm->samples + voice->position;

    if (available < buffer_free) {
        for (size_t i = 0; i < available; i++) {
            buffer[i] += sample[i];
        }

        voice->pcm = NULL;
        voice->position = 0;
    }
    else {
        for (size_t i = 0; i < buffer_free; i++) {
            buffer[i] += sample[i];
        }

        voice->position += buffer_free;
    }
}

int16_t const* hh2_soundMix(size_t* const frames) {
    int32_t buffer[RL2_SAMPLES_PER_VIDEO_FRAME];

    memset(buffer, 0, sizeof(buffer));

    for (unsigned i = 0; i < RL2_MAX_VOICES; i++) {
        if (hh2_voices[i].pcm) {
            hh2_mixPcm(buffer, hh2_voices + i);
        }
    }

    for (size_t i = 0, j = 0; i < RL2_SAMPLES_PER_VIDEO_FRAME; i++, j += 2) {
        int32_t const s32 = buffer[i];
        int16_t const s16 = s32 < -32768 ? -32768 : s32 > 32767 ? 32767 : s32;

        hh2_audioFrames[j] = s16;
        hh2_audioFrames[j + 1] = s16;
    }

    *frames = RL2_SAMPLES_PER_VIDEO_FRAME;
    return hh2_audioFrames;
}
#endif

rl2_Sound rl2_readSound(rl2_Filesys const filesys, char const* const path) {
    rl2_File const file = rl2_openFile(filesys, path);

    if (file == NULL) {
        // Error already logged
        return NULL;
    }

    uint8_t header[16];

    if (rl2_read(file, header, 8) != 8) {
        RL2_ERROR(TAG "error reading from sound \"%s\"", path);
        rl2_close(file);
        return NULL;
    }

    rl2_seek(file, 0, SEEK_SET);

    static uint8_t const wav_header[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
    header[4] = header[5] = header[6] = header[7] = 0;

    if (memcmp(header, wav_header, 8) == 0) {
        return rl2_wavRead(file);
    }

    return NULL;
}

void rl2_destroySound(rl2_Sound const sound) {
    switch (sound->type) {
        case RL2_WAV: rl2_wavDestroy(sound); break;
        case RL2_VORBIS: break;
    }
}

rl2_Voice rl2_play(rl2_Sound const sound, uint8_t const volume, bool const repeat, rl2_Finished finished_cb) {
    switch (sound->type) {
        case RL2_WAV: return rl2_wavPlay(sound, volume, repeat, finished_cb);
        case RL2_VORBIS: return NULL;
    }

    return NULL;
}

void rl2_setVolume(rl2_Voice const voice, uint8_t const volume) {
    voice->volume = volume;
}

void rl2_stop(rl2_Voice const voice) {
    switch (voice->sound->type) {
        case RL2_WAV: rl2_wavStop(voice); break;
        case RL2_VORBIS: break;
    }
}

void rl2_stopAll(void) {
    rl2_Voice voice = rl2_voiceList;

    while (voice != NULL) {
        rl2_Voice next = voice->next;
        rl2_stop(voice);
        voice = next;
    }
}

int16_t const* rl2_soundMix(size_t* const num_frames) {
    int32_t buffer[RL2_SAMPLES_PER_VIDEO_FRAME];
    memset(buffer, 0, sizeof(buffer));

    rl2_Voice voice = rl2_voiceList;

    while (voice != NULL) {
        switch (voice->sound->type) {
            case RL2_WAV: rl2_wavMix(voice, buffer, RL2_SAMPLES_PER_VIDEO_FRAME); break;
            case RL2_VORBIS: break;
        }

        voice = voice->next;
    }

    for (size_t i = 0; i < RL2_SAMPLES_PER_VIDEO_FRAME; i++) {
        int32_t const s32 = buffer[i];
        int16_t const s16 = s32 < -32768 ? -32768 : s32 > 32767 ? 32767 : s32;

        rl2_audioFrames[i] = s16;
    }

    *num_frames = RL2_SAMPLES_PER_VIDEO_FRAME;
    return rl2_audioFrames;
}
