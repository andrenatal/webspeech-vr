/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioSink.h"
#include "MediaDecoderStateMachine.h"
#include "AudioStream.h"
#include "prenv.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define SINK_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define SINK_LOG(type, msg)
#endif

AudioSink::AudioSink(MediaDecoderStateMachine* aStateMachine, AudioAvailableEventManager* aEventManager,
                     int64_t aStartTime, AudioInfo aInfo, dom::AudioChannelType aChannelType)
  : mStateMachine(aStateMachine)
  , mEventManager(aEventManager)
  , mStartTime(aStartTime)
  , mWritten(0)
  , mInfo(aInfo)
  , mChannelType(aChannelType)
  , mVolume(1.0)
  , mPlaybackRate(1.0)
  , mPreservesPitch(false)
  , mStopAudioThread(false)
  , mSetVolume(false)
  , mSetPlaybackRate(false)
  , mSetPreservesPitch(false)
  , mPlaying(true)
{
  NS_ASSERTION(mStartTime != -1, "Should have audio start time by now");
}

nsresult
AudioSink::Init()
{
  nsresult rv = NS_NewNamedThread("Media Audio",
                                  getter_AddRefs(mThread),
                                  nullptr,
                                  MEDIA_THREAD_STACK_SIZE);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &AudioSink::AudioLoop);
  return mThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

int64_t
AudioSink::GetPosition()
{
  if (!mAudioStream) {
    return 0;
  }
  return mAudioStream->GetPosition();
}

void
AudioSink::PrepareToShutdown()
{
  AssertCurrentThreadInMonitor();
  mStopAudioThread = true;
  if (mAudioStream) {
    mAudioStream->Cancel();
  }
  GetReentrantMonitor().NotifyAll();
}

void
AudioSink::Shutdown()
{
  mThread->Shutdown();
  mThread = nullptr;
  MOZ_ASSERT(!mAudioStream);
}

void
AudioSink::SetVolume(double aVolume)
{
  AssertCurrentThreadInMonitor();
  mVolume = aVolume;
  mSetVolume = true;
}

void
AudioSink::SetPlaybackRate(double aPlaybackRate)
{
  AssertCurrentThreadInMonitor();
  NS_ASSERTION(mPlaybackRate != 0, "Don't set the playbackRate to 0 on AudioStream");
  mPlaybackRate = aPlaybackRate;
  mSetPlaybackRate = true;
}

void
AudioSink::SetPreservesPitch(bool aPreservesPitch)
{
  AssertCurrentThreadInMonitor();
  mPreservesPitch = aPreservesPitch;
  mSetPreservesPitch = true;
}

void
AudioSink::StartPlayback()
{
  AssertCurrentThreadInMonitor();
  mPlaying = true;
}

void
AudioSink::StopPlayback()
{
  AssertCurrentThreadInMonitor();
  mPlaying = false;
}

void
AudioSink::AudioLoop()
{
  AssertOnAudioThread();
  SINK_LOG(PR_LOG_DEBUG, ("%p AudioSink: AudioLoop started", this));

  if (NS_FAILED(InitializeAudioStream())) {
    NS_WARNING("Initializing AudioStream failed.");
    return;
  }

  while (1) {
    WaitForAudioToPlay();
    if (!IsPlaybackContinuing()) {
      break;
    }

    // See if there's a gap in the audio. If there is, push silence into the
    // audio hardware, so we can play across the gap.
    // Calculate the timestamp of the next chunk of audio in numbers of
    // samples.
    NS_ASSERTION(AudioQueue().GetSize() > 0, "Should have data to play");
    CheckedInt64 sampleTime = UsecsToFrames(AudioQueue().PeekFront()->mTime, mInfo.mRate);

    // Calculate the number of frames that have been pushed onto the audio hardware.
    CheckedInt64 playedFrames = UsecsToFrames(mStartTime, mInfo.mRate) + mWritten;

    CheckedInt64 missingFrames = sampleTime - playedFrames;
    if (!missingFrames.isValid() || !sampleTime.isValid()) {
      NS_WARNING("Int overflow adding in AudioLoop");
      break;
    }

    if (missingFrames.value() > 0) {
      // The next audio chunk begins some time after the end of the last chunk
      // we pushed to the audio hardware. We must push silence into the audio
      // hardware so that the next audio chunk begins playback at the correct
      // time.
      missingFrames = std::min<int64_t>(UINT32_MAX, missingFrames.value());
      mWritten += PlaySilence(static_cast<uint32_t>(missingFrames.value()),
                              playedFrames.value());
    } else {
      mWritten += PlayFromAudioQueue(sampleTime.value());
    }
    int64_t endTime = GetEndTime();
    if (endTime != -1) {
      mStateMachine->OnAudioEndTimeUpdate(endTime);
    }
  }
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (AudioQueue().AtEndOfStream() && !mStopAudioThread) {
    Drain();
  }
  SINK_LOG(PR_LOG_DEBUG, ("%p AudioSink: AudioLoop complete", this));
  Cleanup();
  SINK_LOG(PR_LOG_DEBUG, ("%p AudioSink: AudioLoop exit", this));
}

nsresult
AudioSink::InitializeAudioStream()
{
  // AudioStream initialization can block for extended periods in unusual
  // circumstances, so we take care to drop the decoder monitor while
  // initializing.
  nsAutoPtr<AudioStream> audioStream(new AudioStream());
  nsresult rv = audioStream->Init(mInfo.mChannels, mInfo.mRate,
                                  mChannelType, AudioStream::HighLatency);
  if (NS_SUCCEEDED(rv)) {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    mAudioStream = audioStream;
    UpdateStreamSettings();
  }

  return rv;
}

void
AudioSink::Drain()
{
  MOZ_ASSERT(!mAudioStream->IsPaused());
  AssertCurrentThreadInMonitor();
  // If the media was too short to trigger the start of the audio stream,
  // start it now.
  mAudioStream->Start();
  {
    ReentrantMonitorAutoExit exit(GetReentrantMonitor());
    mAudioStream->Drain();
  }
  // Fire one last event for any extra frames that didn't fill a framebuffer.
  mEventManager->Drain(GetEndTime());
}

void
AudioSink::Cleanup()
{
  // Must hold lock while shutting down and anulling the audio stream to prevent
  // state machine thread trying to use it while we're destroying it.
  AssertCurrentThreadInMonitor();
  mAudioStream->Shutdown();
  mAudioStream = nullptr;
  mEventManager->Clear();
  mStateMachine->OnAudioSinkComplete();
}

bool
AudioSink::ExpectMoreAudioData()
{
  return AudioQueue().GetSize() == 0 && !AudioQueue().IsFinished();
}

void
AudioSink::WaitForAudioToPlay()
{
  // Wait while we're not playing, and we're not shutting down, or we're
  // playing and we've got no audio to play.
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  while (!mStopAudioThread && (!mPlaying || ExpectMoreAudioData())) {
    if (!mPlaying && !mAudioStream->IsPaused()) {
      mAudioStream->Pause();
    }
    mon.Wait();
  }
}

bool
AudioSink::IsPlaybackContinuing()
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  // If we're shutting down, break out and exit the audio thread.
  // Also break out if audio is being captured.
  if (mStopAudioThread || AudioQueue().AtEndOfStream()) {
    return false;
  }

  if (mPlaying && mAudioStream->IsPaused()) {
    mAudioStream->Resume();
  }

  UpdateStreamSettings();
  return true;
}

uint32_t
AudioSink::PlaySilence(uint32_t aFrames, uint64_t aFrameOffset)
{
  // Maximum number of bytes we'll allocate and write at once to the audio
  // hardware when the audio stream contains missing frames and we're
  // writing silence in order to fill the gap. We limit our silence-writes
  // to 32KB in order to avoid allocating an impossibly large chunk of
  // memory if we encounter a large chunk of silence.
  const uint32_t SILENCE_BYTES_CHUNK = 32 * 1024;

  AssertOnAudioThread();
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  uint32_t maxFrames = SILENCE_BYTES_CHUNK / mInfo.mChannels / sizeof(AudioDataValue);
  uint32_t frames = std::min(aFrames, maxFrames);
  if (!PR_GetEnv("MOZ_QUIET")) {
    SINK_LOG(PR_LOG_DEBUG, ("%p AudioSink: playing %u frames of silence", this, aFrames));
  }
  WriteSilence(frames);
  // Dispatch events to the DOM for the audio just written.
  mEventManager->QueueWrittenAudioData(nullptr, frames * mInfo.mChannels,
                                       (aFrameOffset + frames) * mInfo.mChannels);
  return frames;
}

uint32_t
AudioSink::PlayFromAudioQueue(uint64_t aFrameOffset)
{
  AssertOnAudioThread();
  NS_ASSERTION(!mAudioStream->IsPaused(), "Don't play when paused");
  nsAutoPtr<AudioData> audio(AudioQueue().PopFront());
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    NS_WARN_IF_FALSE(mPlaying, "Should be playing");
    // Awaken the decode loop if it's waiting for space to free up in the
    // audio queue.
    GetReentrantMonitor().NotifyAll();
  }
  if (!PR_GetEnv("MOZ_QUIET")) {
    SINK_LOG(PR_LOG_DEBUG, ("%p AudioSink: playing %u frames of audio at time %lld",
                            this, audio->mFrames, audio->mTime));
  }
  mAudioStream->Write(audio->mAudioData, audio->mFrames);

  StartAudioStreamPlaybackIfNeeded();

  int outChan = mAudioStream->GetOutChannels();
  // Dispatch events to the DOM for the audio just written.
  mEventManager->QueueWrittenAudioData(audio->mAudioData.get(),
                                       audio->mFrames * outChan,
                                       (aFrameOffset + audio->mFrames) * outChan);
  if (audio->mOffset != -1) {
    mStateMachine->OnPlaybackOffsetUpdate(audio->mOffset);
  }
  return audio->mFrames;
}

void
AudioSink::UpdateStreamSettings()
{
  AssertCurrentThreadInMonitor();

  bool setVolume = mSetVolume;
  bool setPlaybackRate = mSetPlaybackRate;
  bool setPreservesPitch = mSetPreservesPitch;
  double volume = mVolume;
  double playbackRate = mPlaybackRate;
  bool preservesPitch = mPreservesPitch;

  mSetVolume = false;
  mSetPlaybackRate = false;
  mSetPreservesPitch = false;

  {
    ReentrantMonitorAutoExit exit(GetReentrantMonitor());
    if (setVolume) {
      mAudioStream->SetVolume(volume);
    }

    if (setPlaybackRate &&
        NS_FAILED(mAudioStream->SetPlaybackRate(playbackRate))) {
      NS_WARNING("Setting the playback rate failed in AudioSink.");
    }

    if (setPreservesPitch &&
      NS_FAILED(mAudioStream->SetPreservesPitch(preservesPitch))) {
      NS_WARNING("Setting the pitch preservation failed in AudioSink.");
    }
  }
}

void
AudioSink::StartAudioStreamPlaybackIfNeeded()
{
  // This value has been chosen empirically.
  const uint32_t MIN_WRITE_BEFORE_START_USECS = 200000;

  // We want to have enough data in the buffer to start the stream.
  if (static_cast<double>(mAudioStream->GetWritten()) / mAudioStream->GetRate() >=
      static_cast<double>(MIN_WRITE_BEFORE_START_USECS) / USECS_PER_S) {
    mAudioStream->Start();
  }
}

void
AudioSink::WriteSilence(uint32_t aFrames)
{
  uint32_t numSamples = aFrames * mInfo.mChannels;
  nsAutoTArray<AudioDataValue, 1000> buf;
  buf.SetLength(numSamples);
  memset(buf.Elements(), 0, numSamples * sizeof(AudioDataValue));
  mAudioStream->Write(buf.Elements(), aFrames);

  StartAudioStreamPlaybackIfNeeded();
}

int64_t
AudioSink::GetEndTime()
{
  CheckedInt64 playedUsecs = FramesToUsecs(mWritten, mInfo.mRate) + mStartTime;
  if (!playedUsecs.isValid()) {
    NS_WARNING("Int overflow calculating audio end time");
    return -1;
  }
  return playedUsecs.value();
}

MediaQueue<AudioData>&
AudioSink::AudioQueue()
{
  return mStateMachine->mReader->AudioQueue();
}

ReentrantMonitor&
AudioSink::GetReentrantMonitor()
{
  return mStateMachine->mDecoder->GetReentrantMonitor();
}

void
AudioSink::AssertCurrentThreadInMonitor()
{
  return mStateMachine->AssertCurrentThreadInMonitor();
}

void
AudioSink::AssertOnAudioThread()
{
  MOZ_ASSERT(IsCurrentThread(mThread));
}

} // namespace mozilla
