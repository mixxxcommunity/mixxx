/****************************************************************************
                   encoderffmpegcore.cpp  -  FFMPEG encoder for mixxx
                             -------------------
    copyright            : (C) 2012-2013 by Tuukka Pasanen
                           (C) 2007 by Wesley Stessens
                           (C) 1994 by Xiph.org (encoder example)
                           (C) 1994 Tobias Rafreider (shoutcast and recording fixes)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
   FFMPEG encoder class..
     - Supports what FFMPEG is compiled to supported
     - Same interface for all codecs
*/

#include "recording/encoderffmpegcore.h"

#include <stdlib.h> // needed for random num gen
#include <time.h> // needed for random num gen
#include <string.h> // needed for memcpy
#include <QDebug>

#include "engine/engineabstractrecord.h"
#include "controlobjectthreadmain.h"
#include "controlobject.h"
#include "playerinfo.h"
#include "trackinfoobject.h"
#include "errordialoghandler.h"

//
// FFMPEG changed their variable/define names in 1.0
// smallest number that is AV_/AV compatible avcodec version is
// 54/59/100 which is 3554148
//

// Constructor
#ifdef AV_CODEC_ID_NONE
EncoderFfmpegCore::EncoderFfmpegCore(EngineAbstractRecord *engine, AVCodecID codec)
#else
EncoderFfmpegCore::EncoderFfmpegCore(EngineAbstractRecord *engine, CodecID codec)
#endif
{
    m_bStreamInitialized = false;
    m_pEngine = engine;
    m_strMetaDataTitle = NULL;
    m_strMetaDataArtist = NULL;
    m_strMetaDataAlbum = NULL;
    m_pMetaData = TrackPointer(NULL);
    m_SCodecID = codec;

    m_pEncodeFormatCtx = NULL;
    m_pEncoderAudioStream = NULL;
    m_pEncoderAudioCodec = NULL;
    m_pEncoderFormat = NULL;


    m_pSamples = NULL;
    m_pFltSamples = NULL;
    m_iAudioInputFrameSize = -1;

    memset( m_SBuffer, 0x00, (65355 * 2) );
    m_lBufferSize = 0;
    m_iAudioCpyLen = 0;
    m_iFltAudioCpyLen = 0;

    m_pSamplerate = new ControlObjectThread(ControlObject::getControl(ConfigKey("[Master]", "samplerate")));

    m_pSwrCtx = NULL;
    m_pOut = NULL;
    m_pOutSize = 0;

    m_SCcodecId = codec;
    m_lBitrate = 128000;
    m_lDts = 0;
    m_lPts = 0;
    m_lRecorededBytes = 0;

}

// Destructor  //call flush before any encoder gets deleted
EncoderFfmpegCore::~EncoderFfmpegCore() {

    qDebug() << "EncoderFfmpegCore::~EncoderFfmpegCore()";
    av_freep(&m_pSamples);
    av_freep(&m_pFltSamples);

    avio_open_dyn_buf(&m_pEncodeFormatCtx->pb);
    if (av_write_trailer(m_pEncodeFormatCtx) != 0) {
        qDebug() << "Multiplexer: failed to write a trailer.";
    } else {
        unsigned char *l_strBuffer = NULL;
        int l_iBufferLen = 0;
        l_iBufferLen = avio_close_dyn_buf(m_pEncodeFormatCtx->pb, (uint8_t**)(&l_strBuffer));
        m_pEngine->write(NULL, l_strBuffer, 0, l_iBufferLen);
        av_free(l_strBuffer);
    }



    if( m_pStream != NULL ) {
        avcodec_close(m_pStream->codec);
    }

    //if( m_pEncoderAudioCodec != NULL )
    //{
    //    avcodec_close(m_pEncoderAudioCodec);
    //    av_free(m_pEncoderAudioCodec);
    //}

    if( m_pEncodeFormatCtx != NULL ) {
        av_free(m_pEncodeFormatCtx);
    }

    //if( m_pCodec != NULL ){
    //     avcodec_close(m_pCodec);
    //}

    // Close buffer
    delete m_pSamplerate;
}

unsigned int EncoderFfmpegCore::reSample(AVFrame *inframe) {

    if (!m_pSwrCtx) {
        // Create converter from in type to s16 sample rate
#ifndef __FFMPEGOLDAPI__
        qDebug() << "ffmpeg: Using libavresample";

        m_pSwrCtx = avresample_alloc_context();

        av_opt_set_int(m_pSwrCtx,"in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(m_pSwrCtx,"in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        av_opt_set_int(m_pSwrCtx,"in_sample_rate", 44100, 0);
        av_opt_set_int(m_pSwrCtx,"out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(m_pSwrCtx,"out_sample_fmt", m_pEncoderAudioStream->codec->sample_fmt, 0);
        av_opt_set_int(m_pSwrCtx,"out_sample_rate", 44100, 0);

#else
        qDebug() << "ffmpeg: OLD FFMPEG API in use!";
        m_pSwrCtx = av_audio_resample_init(2,
                                           m_pEncoderAudioStream->codec->channels,
                                           m_pEncoderAudioStream->codec->sample_rate,
                                           m_pEncoderAudioStream->codec->sample_rate,
                                           m_pEncoderAudioStream->codec->sample_fmt,
                                           AV_SAMPLE_FMT_FLT,
                                           16,
                                           10,
                                           0,
                                           0.8);

#endif
        if( !m_pSwrCtx ) {
            qDebug() << "Can't init convertor!";
            return -1;
        }

#ifndef __FFMPEGOLDAPI__
        // If it not working let user know about it!
        // If we don't do this we'll gonna crash
        if ( avresample_open(m_pSwrCtx ) < 0) {
            m_pSwrCtx = NULL;
            qDebug() << "ERROR!! Conventor not created: 44100 Hz " << av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT) << " 2 channels";
            qDebug() << "To 44100 HZ format:" << av_get_sample_fmt_name(m_pEncoderAudioStream->codec->sample_fmt) << " with 2 channels";
            return -1;
        }
#endif
        qDebug() << "Created: 44100 Hz " << av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT) << " 2 channels";
        qDebug() << "To 44100 HZ format:" << av_get_sample_fmt_name(m_pEncoderAudioStream->codec->sample_fmt) << " with 2 channels";
    }

    if (m_pSwrCtx) {

#ifndef __FFMPEGOLDAPI__
        uint8_t **l_pIn = (uint8_t **)inframe->extended_data;
#endif

// #ifdef __FFMPEGOLDAPI__
//        int64_t l_lInReadBytes = av_samples_get_buffer_size(NULL, 2,
//                                 inframe->nb_samples,
//                                 m_pEncoderAudioStream->codec->sample_fmt, 1);
// #endif

#ifndef __FFMPEGOLDAPI__
        int l_iOutSamples = av_rescale_rnd(avresample_get_delay(m_pSwrCtx) +
                                           inframe->nb_samples, 44100, 44100, AV_ROUND_UP);

        int l_iOutSamplesLines = 0;
        // Alloc too much.. if not enough we are in trouble!
        av_samples_alloc(&m_pOut, &l_iOutSamplesLines, 2, l_iOutSamples, m_pEncoderAudioStream->codec->sample_fmt, 0);
#else
        int l_iOutSamples = av_rescale_rnd(inframe->nb_samples, m_pEncoderAudioStream->codec->sample_rate, m_pEncoderAudioStream->codec->sample_rate, AV_ROUND_UP);

        int l_iOutBytes =  av_samples_get_buffer_size(NULL, 2,
                           l_iOutSamples,
                           m_pEncoderAudioStream->codec->sample_fmt, 1);


        //av_samples_alloc(&m_pOut, NULL, 2, l_iOutSamples, AV_SAMPLE_FMT_S16, 0);
        m_pOut = (short *)malloc(l_iOutBytes * 2);
#endif

        int l_iLen = 0;
#ifndef __FFMPEGOLDAPI__

        // qDebug() << "SAMPLES OUT" << l_iOutSamples << "SAMPLES IN" << inframe->nb_samples;
#if LIBAVRESAMPLE_VERSION_INT <= 3
      l_iLen = avresample_convert(m_pSwrCtx, (void **)&m_pOut, 0, l_iOutSamples,
                                  (void **)l_pIn, 0, inframe->nb_samples);
#else
      l_iLen = avresample_convert(m_pSwrCtx, (uint8_t **)&m_pOut, 0, l_iOutSamples,
                                  (uint8_t **)l_pIn, 0, inframe->nb_samples);
#endif
        // qDebug() << "U" << l_iLen;

        m_pOutSize = av_samples_get_buffer_size(NULL, 2,l_iLen,m_pEncoderAudioStream->codec->sample_fmt, 1);

#else
        // qDebug() << "INSAMPLE" << inframe->nb_samples << "OUTSAMPLE" << l_iOutSamples << "OUTBYTES" << l_iOutBytes << "INBYTES" << l_lInReadBytes;
        l_iLen = audio_resample(m_pSwrCtx,
                                (short *)m_pOut, (short *)inframe->data[0],
                                inframe->nb_samples);

        // qDebug() << "U" << l_iLen;


        m_pOutSize = l_iOutBytes;
#endif
        if (l_iLen < 0) {
            qDebug() << "swr_convert() failed";
            return -1;
        }

    } else {
        m_pOutSize = 0;
    }

    return 0;
}

//call sendPackages() or write() after 'flush()' as outlined in engineshoutcast.cpp
void EncoderFfmpegCore::flush() {
}

/*
  Get new random serial number
  -> returns random number
*/
int EncoderFfmpegCore::getSerial() {
    int l_iSerial = 0;
    return l_iSerial;
}

void EncoderFfmpegCore::encodeBuffer(const CSAMPLE *samples, const int size) {
    unsigned char *l_strBuffer = NULL;
    int l_iBufferLen = 0;
    //int l_iAudioCpyLen = m_iAudioInputFrameSize *
    //                     av_get_bytes_per_sample(m_pEncoderAudioStream->codec->sample_fmt) *
    //                     m_pEncoderAudioStream->codec->channels;
    long l_iLeft = size;
    long j = 0;
    unsigned int l_iBufPos = 0;
    unsigned int l_iPos = 0;

    float *l_fNormalizedSamples = (float *)malloc( size * sizeof(float) );

    // For FFMPEG/LIBAV <= 1.0/0.8.5 needed thing to make this not to clip..
    //
    // Don't know why but input is too high..
    // Thats why we divide by 10000
    //
    for( j = 0; j < size; j ++) {
        l_fNormalizedSamples[j] = samples[j] / 10000;
    }

    //CSAMPLE *l_pSampleLocation = (CSAMPLE *)samples;
    //  m_bStreamInitialized = true;
    if( m_bStreamInitialized == false ) {
        m_bStreamInitialized = true;
        // Write a header.
        avio_open_dyn_buf(&m_pEncodeFormatCtx->pb);
        if (avformat_write_header(m_pEncodeFormatCtx, NULL) != 0) {
            qDebug() << "EncoderFfmpegCore::encodeBuffer: failed to write a header.";
            return;
        }

        l_iBufferLen = avio_close_dyn_buf(m_pEncodeFormatCtx->pb, (uint8_t**)(&l_strBuffer));
        m_pEngine->write(NULL, l_strBuffer, 0, l_iBufferLen);
        av_free(l_strBuffer);

        qDebug() << "EncoderFfmpegCore::encodeBuffer: Header!" << l_iBufferLen;
        // return;
    }
    // qDebug() << "Sample data: " << size << "Input frame_size" << m_iAudioInputFrameSize << "Audio cpy size" << l_iAudioCpyLen;
    // qDebug() << "FLT data size: " << av_samples_get_buffer_size(NULL, 2, m_iAudioInputFrameSize, AV_SAMPLE_FMT_FLT,1);

    while( l_iLeft > (m_iFltAudioCpyLen / 4)) {
        memset(m_pFltSamples, 0x00, m_iFltAudioCpyLen);

           for (j = 0; j < m_iFltAudioCpyLen / 4; j++) {
            // qDebug() << m_lBufferSize << "O" << j << ":" << (m_iFltAudioCpyLen / 2) << ";" << l_iPos;
            if( m_lBufferSize > 0 ) {
                m_pFltSamples[j] = m_SBuffer[ l_iBufPos ++ ];
                m_lBufferSize --;
                m_lRecorededBytes ++;
            } else {
                m_pFltSamples[j] = l_fNormalizedSamples[l_iPos ++];
                l_iLeft --;
                m_lRecorededBytes ++;
            }

            if( l_iLeft <= 0 ) {
                qDebug() << "ffmpegecodercore: No samples left.. for encoding!";
                break;
            }
        }

        m_lBufferSize = 0;

        if( avio_open_dyn_buf(&m_pEncodeFormatCtx->pb) < 0) {
            qDebug() << "Can't alloc Dyn buffer!";
            return;
        }

        if( ! writeAudioFrame(m_pEncodeFormatCtx, m_pEncoderAudioStream) ) {
            l_iBufferLen = avio_close_dyn_buf(m_pEncodeFormatCtx->pb, (uint8_t**)(&l_strBuffer));
            // qDebug() << l_iLeft << "JEEEEA bytz:" << l_iBufferLen;
            m_pEngine->write(NULL, l_strBuffer, 0, l_iBufferLen);
            av_free(l_strBuffer);
        }
    }

    // Keep things clean
    memset( m_SBuffer, 0x00, 65535);

    for( j = 0; j < l_iLeft; j ++ ) {
        m_SBuffer[ j ] = l_fNormalizedSamples[ l_iPos ++ ];
    }
    m_lBufferSize = l_iLeft;
    free(l_fNormalizedSamples);
}

/* Originally called from engineshoutcast.cpp to update metadata information
 * when streaming, however, this causes pops
 *
 * Currently this method is used before init() once to save artist, title and album
*/
void EncoderFfmpegCore::updateMetaData(char* artist, char* title, char* album) {
    qDebug() << "ffmpegecodercore: UpdateMetadata: !" << artist << " - " << title << " - " << album;
    m_strMetaDataTitle = title;
    m_strMetaDataArtist = artist;
    m_strMetaDataAlbum = album;
}

void EncoderFfmpegCore::initStream() {
    qDebug() << "ffmpegecodercore: Initstream!";
}

int EncoderFfmpegCore::initEncoder(int bitrate) {
    av_register_all();
    avcodec_register_all();

#ifndef avformat_alloc_output_context2
    qDebug() << "EncoderFfmpegCore::initEncoder: Old Style initialization";
    m_pEncodeFormatCtx = avformat_alloc_context();
#endif

#ifdef AV_CODEC_ID_NONE
    if( m_SCcodecId == AV_CODEC_ID_MP3 ) {
#else
    if( m_SCcodecId == CODEC_ID_MP3 ) {
#endif
      qDebug() << "EncoderFfmpegCore::initEncoder: Codec MP3";
#ifdef avformat_alloc_output_context2
        avformat_alloc_output_context2(&m_pEncodeFormatCtx, NULL, NULL, "output.mp3");
#else
        m_pEncoderFormat = av_guess_format(NULL, "output.mp3", NULL);
#endif

        m_lBitrate = bitrate * 1000;
#ifdef AV_CODEC_ID_NONE
    } else if( m_SCcodecId == AV_CODEC_ID_AAC ) {
#else
    } else if( m_SCcodecId == CODEC_ID_AAC ) {
#endif
      qDebug() << "EncoderFfmpegCore::initEncoder: Codec M4A";
#ifdef avformat_alloc_output_context2
        avformat_alloc_output_context2(&m_pEncodeFormatCtx, NULL, NULL, "output.m4a");
#else
        m_pEncoderFormat = av_guess_format(NULL, "output.m4a", NULL);
#endif
        m_lBitrate = bitrate * 1000;
    } else {
        qDebug() << "EncoderFfmpegCore::initEncoder: Codec OGG/Vorbis";
#ifdef avformat_alloc_output_context2
        avformat_alloc_output_context2(&m_pEncodeFormatCtx, NULL, NULL, "output.ogg");
        m_pEncodeFormatCtx->oformat->audio_codec=AV_CODEC_ID_VORBIS;
#else
        m_pEncoderFormat = av_guess_format(NULL, "output.ogg", NULL);
#ifdef AV_CODEC_ID_NONE
        m_pEncoderFormat->audio_codec=AV_CODEC_ID_VORBIS;
#else
        m_pEncoderFormat->audio_codec=CODEC_ID_VORBIS;
#endif
#endif
        m_lBitrate = bitrate * 1000;
    }

#ifdef avformat_alloc_output_context2
    m_pEncoderFormat = m_pEncodeFormatCtx->oformat;
#else
    m_pEncodeFormatCtx->oformat = m_pEncoderFormat;
#endif

    m_pEncoderAudioStream = addStream(m_pEncodeFormatCtx, &m_pEncoderAudioCodec, m_pEncoderFormat->audio_codec);

    openAudio(m_pEncodeFormatCtx, m_pEncoderAudioCodec, m_pEncoderAudioStream);

    // qDebug() << "jepusti";

    return 0;
}

// Private methods

int EncoderFfmpegCore::writeAudioFrame(AVFormatContext *formatctx, AVStream *stream) {
    AVCodecContext *l_SCodecCtx = NULL;;
    AVPacket l_SPacket;
    AVFrame *l_SFrame = avcodec_alloc_frame();
    int l_iGotPacket;
    int l_iRet;
#ifdef av_make_error_string
    char l_strErrorBuff[256];
#endif
    
    av_init_packet(&l_SPacket);
    l_SPacket.size = 0;
    l_SPacket.data = NULL;

    // Calculate correct DTS for FFMPEG
    m_lDts = round(((double)m_lRecorededBytes / (double)44100 / (double)2. * (double)m_pEncoderAudioStream->time_base.den));
    m_lPts = m_lDts;

    l_SCodecCtx = stream->codec;
#ifdef av_make_error_string
    memset( l_strErrorBuff, 0x00, 256 );
#endif
    
    // printf("MEGQ: %d * %d * %d = %d\n",m_iAudioInputFrameSize, av_get_bytes_per_sample(l_SCodecCtx->sample_fmt), l_SCodecCtx->channels, m_iAudioCpyLen);
    // get_audio_l_SFrame(samples, m_iAudioInputFrameSize, l_SCodecCtx->channels);
    l_SFrame->nb_samples = m_iAudioInputFrameSize;
    // Mixxx uses float (32 bit) samples..
    l_SFrame->format = AV_SAMPLE_FMT_FLT;
#ifdef AV_CODEC_ID_NONE
    l_SFrame->channel_layout = l_SCodecCtx->channel_layout;
#endif

    l_iRet = avcodec_fill_audio_frame(l_SFrame,
                                   l_SCodecCtx->channels,
                                   AV_SAMPLE_FMT_FLT,
                                   (const uint8_t *)m_pFltSamples,
                                   m_iFltAudioCpyLen,
                                   1);

    if ( l_iRet != 0 ) {
#ifdef av_make_error_string
        qDebug() << "Can't fill FFMPEG frame: error " << l_iRet << "String '" << av_make_error_string( l_strErrorBuff, 256, l_iRet ) << "'" <<  m_iFltAudioCpyLen;
#endif
        qDebug() << "Can't refill 1st FFMPEG frame!";
        return -1;
    }

    // If we have something else than AV_SAMPLE_FMT_FLT we have to convert it to something that
    // fits..
    if( l_SCodecCtx->sample_fmt != AV_SAMPLE_FMT_FLT ) {
        reSample(l_SFrame);
        // After we have turned our samples to destination
        // Format we must re-alloc l_SFrame.. it easier like this..
#ifndef __FFMPEGOLDAPI__
        avcodec_free_frame(&l_SFrame);
#else
        av_free(l_SFrame);
#endif
        l_SFrame = NULL;
        l_SFrame = avcodec_alloc_frame();
        l_SFrame->nb_samples = m_iAudioInputFrameSize;
        l_SFrame->format = l_SCodecCtx->sample_fmt;
#ifdef AV_CODEC_ID_NONE
        l_SFrame->channel_layout = m_pEncoderAudioStream->codec->channel_layout;
#endif

        l_iRet = avcodec_fill_audio_frame(l_SFrame, l_SCodecCtx->channels,
                                       l_SCodecCtx->sample_fmt,
                                       (const uint8_t *)m_pOut,
                                       m_iAudioCpyLen,
                                       1);

        if ( l_iRet != 0 ) {
#ifdef av_make_error_string
            qDebug() << "Can't refill FFMPEG frame: error " << l_iRet << "String '" << av_make_error_string( l_strErrorBuff, 256, l_iRet ) << "'" <<  m_iAudioCpyLen << " " <<  av_samples_get_buffer_size(NULL, 2, m_iAudioInputFrameSize,m_pEncoderAudioStream->codec->sample_fmt,1) << " " << m_pOutSize;
#endif
            qDebug() << "Can't refill 2nd FFMPEG frame!";
            return -1;
        }
    }

    //qDebug() << "!!" << l_iRet;
    l_iRet = avcodec_encode_audio2(l_SCodecCtx, &l_SPacket, l_SFrame, &l_iGotPacket);

    if (l_iRet < 0) {
        qDebug() << "Error encoding audio frame";
        return -1;
    }

    if (!l_iGotPacket) {
        // qDebug() << "No packet! Can't encode audio!!";
        return -1;
    }

    l_SPacket.stream_index = stream->index;
 
    // Let's calculate DTS/PTS and give it to FFMPEG..
    // THEN codecs like OGG/Voris works ok!!
    l_SPacket.dts = m_lDts;
    l_SPacket.pts = m_lDts;

    // qDebug() << "!!::" << l_SPacket.size << "index" << l_SPacket.stream_index;

    // Some times den is zero.. so 0 dived by 0 is
    // Something?
    if( m_pEncoderAudioStream->pts.den == 0 ){
      qDebug() << "Time hack!";
      m_pEncoderAudioStream->pts.den = 1;
    }

    // Write the compressed frame to the media file. */
    l_iRet = av_interleaved_write_frame(formatctx, &l_SPacket);

    if (l_iRet != 0) {
        qDebug() << "Error while writing audio frame";
        return -1;
    }

    // qDebug() << "!!PP";
    av_free_packet(&l_SPacket);
#ifndef __FFMPEGOLDAPI__
        av_destruct_packet(&l_SPacket);
        avcodec_free_frame(&l_SFrame);
#else
        av_free(l_SFrame);
#endif

    return 0;
}


void EncoderFfmpegCore::closeAudio(AVFormatContext *formatctx, AVStream *stream) {
    avcodec_close(stream->codec);

    av_free(m_pSamples);
}

void EncoderFfmpegCore::openAudio(AVFormatContext *formatctx, AVCodec *codec, AVStream *stream) {
    AVCodecContext *l_SCodecCtx;
    int l_iRet;

    l_SCodecCtx = stream->codec;

    qDebug() << "openCodec!";

    /* open it */
    l_iRet = avcodec_open2(l_SCodecCtx, codec, NULL);
    if (l_iRet < 0) {
        qDebug() << "Could not open audio codec!";
        return;
    }

    if (l_SCodecCtx->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE) {
        m_iAudioInputFrameSize = 10000;
    } else {
        m_iAudioInputFrameSize = l_SCodecCtx->frame_size;
    }

    m_iAudioCpyLen = m_iAudioInputFrameSize *
                     av_get_bytes_per_sample(stream->codec->sample_fmt) *
                     stream->codec->channels;


    m_iFltAudioCpyLen = av_samples_get_buffer_size(NULL, 2, m_iAudioInputFrameSize, AV_SAMPLE_FMT_FLT,1);

    // m_pSamples is destination samples.. m_pFltSamples is FLOAT (32 bit) samples..
    m_pSamples = (uint8_t *)av_malloc(m_iAudioCpyLen * sizeof( uint8_t ));
    //m_pFltSamples = (uint16_t *)av_malloc(m_iFltAudioCpyLen);
    m_pFltSamples = (float *)av_malloc(m_iFltAudioCpyLen * sizeof( float ));

    if (!m_pSamples) {
        qDebug() << "Could not allocate audio samples buffer";
        return;
    }



}

/* Add an output stream. */
#ifdef AV_CODEC_ID_NONE
AVStream *EncoderFfmpegCore::addStream(AVFormatContext *formatctx, AVCodec **codec, enum AVCodecID codec_id) {
#else
AVStream *EncoderFfmpegCore::addStream(AVFormatContext *formatctx, AVCodec **codec, enum CodecID codec_id) {
#endif
  AVCodecContext *l_SCodecCtx = NULL;
    AVStream *l_SStream = NULL;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
#ifdef avcodec_get_name
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
#endif
        return NULL;
    }

    l_SStream = avformat_new_stream(formatctx, *codec);
    if (!l_SStream) {
        qDebug() << "Could not allocate stream";
        return NULL;
    }
    l_SStream->id = formatctx->nb_streams-1;
    l_SCodecCtx = l_SStream->codec;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        l_SStream->id = 1;
        l_SCodecCtx->sample_fmt = m_pEncoderAudioCodec->sample_fmts[0];
 
        //l_SCodecCtx->bit_rate    = 128000;
        l_SCodecCtx->bit_rate    = m_lBitrate;
        l_SCodecCtx->sample_rate = 44100;
        l_SCodecCtx->channels    = 2;
        break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (formatctx->oformat->flags & AVFMT_GLOBALHEADER)
        l_SCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return l_SStream;
}
