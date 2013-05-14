/****************************************************************************
                   encodervorbis.cpp  -  vorbis encoder for mixxx
                             -------------------
    copyright            : (C) 2007 by Wesley Stessens
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

#include "recording/encoderffmpegmp3.h"

// Constructor
#ifdef AV_CODEC_ID_NONE
EncoderFfmpegMp3::EncoderFfmpegMp3(EngineAbstractRecord *engine) : EncoderFfmpegCore( engine, AV_CODEC_ID_MP3 )
#else
EncoderFfmpegMp3::EncoderFfmpegMp3(EngineAbstractRecord *engine) : EncoderFfmpegCore( engine, CODEC_ID_MP3 )
#endif
{
}

