/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2016-2026 SDR Telecom

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TVLTX_BMD_CAPTURE_H
#define TVLTX_BMD_CAPTURE_H

#include <pthread.h>
#include "DeckLinkAPI.h"

extern "C" {
#include "../encoder_opts.h"
#include "../sync.h"
}

/* One Blackmagic DeckLink capture input (one instance per <inputcard>). */
class DeckLinkInput : public IDeckLinkInputCallback
{
public:
    explicit DeckLinkInput(input_config *input);

    /* capture until `running` goes false */
    void run();

    /* IUnknown */
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    /* IDeckLinkInputCallback */
    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events,
                                                              IDeckLinkDisplayMode *mode,
                                                              BMDDetectedVideoInputFormatFlags flags);
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame *video,
                                                             IDeckLinkAudioInputPacket *audio);

private:
    virtual ~DeckLinkInput();

    bool open_device();
    void close_device();
    bool start_capture();
    void no_device_loop();
    void watchdog();
    void deliver_video(IDeckLinkVideoInputFrame *video, int64_t t);
    void deliver_audio(IDeckLinkAudioInputPacket *audio, int64_t t);

    input_config *input;
    int32_t ref_count;

    IDeckLink *device;
    IDeckLinkInput *dl_input;
    IDeckLinkConfiguration *config;
    IDeckLinkProfileAttributes *attributes;

    BMDDisplayMode display_mode;   /* the configured mode */
    BMDPixelFormat pixel_format;   /* 8-bit YUV, or 8-bit BGRA for RGB sources */
    BMDVideoConnection connection;
    bool format_detection;
    int channels;

    sync_mode mode;
    sync_video sv;
    sync_audio sa;
    int64_t epoch_ns;

    uint8_t *frame;                /* UYVY frame handed to the encoder */
    uint8_t *bars;
    int16_t *pcm;
    size_t pcm_frames;             /* capacity, in sample frames */

    pthread_mutex_t lock;          /* callback thread vs. watchdog */
    int64_t last_video_ns;
    int64_t last_audio_ns;
    bool no_signal;
    bool mode_mismatch;
};

#endif
