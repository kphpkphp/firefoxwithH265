/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "AgnosticDecoderModule.h"
#include "AudioTrimmer.h"
#include "BlankDecoderModule.h"
#include "DecoderDoctorDiagnostics.h"
#include "EMEDecoderModule.h"
#include "GMPDecoderModule.h"
#include "H264.h"
#include "MP4Decoder.h"
#include "MediaChangeMonitor.h"
#include "MediaInfo.h"
#include "OpusDecoder.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderModule.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart
#include "nsPrintfCString.h"

#include "mozilla/ipc/UtilityAudioDecoderParent.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#  ifdef MOZ_WMF_MEDIA_ENGINE
#    include "MFMediaEngineDecoderModule.h"
#  endif
#  ifdef MOZ_WMF_CDM
#    include "mozilla/CDMProxy.h"
#  endif
#endif
#ifdef MOZ_FFVPX
#  include "FFVPXRuntimeLinker.h"
#endif
#ifdef MOZ_FFMPEG
#  include "FFmpegRuntimeLinker.h"
#endif
#ifdef MOZ_APPLEMEDIA
#  include "AppleDecoderModule.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidDecoderModule.h"
#endif
#ifdef MOZ_OMX
#  include "OmxDecoderModule.h"
#endif

#include <functional>

using DecodeSupport = mozilla::media::DecodeSupport;
using DecodeSupportSet = mozilla::media::DecodeSupportSet;
using MediaCodec = mozilla::media::MediaCodec;
using MediaCodecsSupport = mozilla::media::MediaCodecsSupport;
using MediaCodecsSupported = mozilla::media::MediaCodecsSupported;
using MCSInfo = mozilla::media::MCSInfo;

namespace mozilla {

#define PDM_INIT_LOG(msg, ...) \
  MOZ_LOG(sPDMLog, LogLevel::Debug, ("PDMInitializer, " msg, ##__VA_ARGS__))

extern already_AddRefed<PlatformDecoderModule> CreateNullDecoderModule();

class PDMInitializer final {
 public:
  // This function should only be executed ONCE per process.
  static void InitPDMs();

  // Return true if we've finished PDMs initialization.
  static bool HasInitializedPDMs();

 private:
  static void InitGpuPDMs() {
#ifdef XP_WIN
    WMFDecoderModule::Init();
#endif
  }

  static void InitRddPDMs() {
#ifdef XP_WIN
    WMFDecoderModule::Init();
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
    FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_rdd_ffmpeg_enabled()) {
      FFmpegRuntimeLinker::Init();
    }
#endif
  }

  static void InitUtilityPDMs() {
    const ipc::SandboxingKind kind = GetCurrentSandboxingKind();
#ifdef XP_WIN
    if (kind == ipc::SandboxingKind::UTILITY_AUDIO_DECODING_WMF) {
      WMFDecoderModule::Init();
    }
#  ifdef MOZ_WMF_MEDIA_ENGINE
    if (StaticPrefs::media_wmf_media_engine_enabled() &&
        kind == ipc::SandboxingKind::MF_MEDIA_ENGINE_CDM) {
      MFMediaEngineDecoderModule::Init();
    }
#  endif
#endif
#ifdef MOZ_APPLEMEDIA
    if (kind == ipc::SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA) {
      AppleDecoderModule::Init();
    }
#endif
#ifdef MOZ_FFVPX
    if (kind == ipc::SandboxingKind::GENERIC_UTILITY) {
      FFVPXRuntimeLinker::Init();
    }
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_utility_ffmpeg_enabled() &&
        kind == ipc::SandboxingKind::GENERIC_UTILITY) {
      FFmpegRuntimeLinker::Init();
    }
#endif
  }

  static void InitContentPDMs() {
#if !defined(MOZ_WIDGET_ANDROID)  // Still required for video?
    if (StaticPrefs::media_allow_audio_non_utility()) {
#endif  // !defined(MOZ_WIDGET_ANDROID)
#ifdef XP_WIN
#  ifdef MOZ_WMF
      if (!StaticPrefs::media_rdd_process_enabled() ||
          !StaticPrefs::media_rdd_wmf_enabled() ||
          !StaticPrefs::media_utility_process_enabled() ||
          !StaticPrefs::media_utility_wmf_enabled()) {
        WMFDecoderModule::Init();
      }
#  endif
#endif
#ifdef MOZ_APPLEMEDIA
      AppleDecoderModule::Init();
#endif
#ifdef MOZ_OMX
      OmxDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
      FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
      FFmpegRuntimeLinker::Init();
#endif
#if !defined(MOZ_WIDGET_ANDROID)  // Still required for video?
    }
#endif  // !defined(MOZ_WIDGET_ANDROID)

    RemoteDecoderManagerChild::Init();
  }

  static void InitDefaultPDMs() {
#ifdef XP_WIN
    WMFDecoderModule::Init();
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_OMX
    OmxDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
    FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
    FFmpegRuntimeLinker::Init();
#endif
  }

  static bool sHasInitializedPDMs;
  static StaticMutex sMonitor MOZ_UNANNOTATED;
};

bool PDMInitializer::sHasInitializedPDMs = false;
StaticMutex PDMInitializer::sMonitor;

/* static */
void PDMInitializer::InitPDMs() {
  StaticMutexAutoLock mon(sMonitor);
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sHasInitializedPDMs);
  if (XRE_IsGPUProcess()) {
    PDM_INIT_LOG("Init PDMs in GPU process");
    InitGpuPDMs();
  } else if (XRE_IsRDDProcess()) {
    PDM_INIT_LOG("Init PDMs in RDD process");
    InitRddPDMs();
  } else if (XRE_IsUtilityProcess()) {
    PDM_INIT_LOG("Init PDMs in Utility process");
    InitUtilityPDMs();
  } else if (XRE_IsContentProcess()) {
    PDM_INIT_LOG("Init PDMs in Content process");
    InitContentPDMs();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                          "PDMFactory is only usable in the "
                          "Parent/GPU/RDD/Utility/Content process");
    PDM_INIT_LOG("Init PDMs in Chrome process");
    InitDefaultPDMs();
  }
  sHasInitializedPDMs = true;
}

/* static */
bool PDMInitializer::HasInitializedPDMs() {
  StaticMutexAutoLock mon(sMonitor);
  return sHasInitializedPDMs;
}

class SupportChecker {
 public:
  enum class Reason : uint8_t {
    kSupported,
    kVideoFormatNotSupported,
    kAudioFormatNotSupported,
    kUnknown,
  };

  struct CheckResult {
    explicit CheckResult(Reason aReason,
                         MediaResult aResult = MediaResult(NS_OK))
        : mReason(aReason), mMediaResult(std::move(aResult)) {}
    CheckResult(const CheckResult& aOther) = default;
    CheckResult(CheckResult&& aOther) = default;
    CheckResult& operator=(const CheckResult& aOther) = default;
    CheckResult& operator=(CheckResult&& aOther) = default;

    Reason mReason;
    MediaResult mMediaResult;
  };

  template <class Func>
  void AddToCheckList(Func&& aChecker) {
    mCheckerList.AppendElement(std::forward<Func>(aChecker));
  }

  void AddMediaFormatChecker(const TrackInfo& aTrackConfig) {
    if (aTrackConfig.IsVideo()) {
      auto mimeType = aTrackConfig.GetAsVideoInfo()->mMimeType;
      RefPtr<MediaByteBuffer> extraData =
          aTrackConfig.GetAsVideoInfo()->mExtraData;
      AddToCheckList([mimeType, extraData]() {
        if (MP4Decoder::IsH264(mimeType)) {
          SPSData spsdata;
          // WMF H.264 Video Decoder and Apple ATDecoder
          // do not support YUV444 format.
          // For consistency, all decoders should be checked.
          if (H264::DecodeSPSFromExtraData(extraData, spsdata) &&
              (spsdata.profile_idc == 244 /* Hi444PP */ ||
               spsdata.chroma_format_idc == PDMFactory::kYUV444)) {
            return CheckResult(
                SupportChecker::Reason::kVideoFormatNotSupported,
                MediaResult(
                    NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Decoder may not have the capability "
                                  "to handle the requested video format "
                                  "with YUV444 chroma subsampling.")));
          }
        }
        return CheckResult(SupportChecker::Reason::kSupported);
      });
    }
  }

  SupportChecker::CheckResult Check() {
    for (auto& checker : mCheckerList) {
      auto result = checker();
      if (result.mReason != SupportChecker::Reason::kSupported) {
        return result;
      }
    }
    return CheckResult(SupportChecker::Reason::kSupported);
  }

  void Clear() { mCheckerList.Clear(); }

 private:
  nsTArray<std::function<CheckResult()>> mCheckerList;
};  // SupportChecker

PDMFactory::PDMFactory() {
  EnsureInit();
  CreatePDMs();
  CreateNullPDM();
}

PDMFactory::~PDMFactory() = default;

/* static */
void PDMFactory::EnsureInit() {
  if (PDMInitializer::HasInitializedPDMs()) {
    return;
  }
  auto initalization = []() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    if (!PDMInitializer::HasInitializedPDMs()) {
      // Ensure that all system variables are initialized.
      gfx::gfxVars::Initialize();
      // Prime the preferences system from the main thread.
      Unused << BrowserTabsRemoteAutostart();
      PDMInitializer::InitPDMs();
    }
  };
  // If on the main thread, then initialize PDMs. Otherwise, do a sync-dispatch
  // to main thread.
  if (NS_IsMainThread()) {
    initalization();
    return;
  }
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadSerialEventTarget();
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "PDMFactory::EnsureInit", std::move(initalization));
  SyncRunnable::DispatchToThread(mainTarget, runnable);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise> PDMFactory::CreateDecoder(
    const CreateDecoderParams& aParams) {
  if (aParams.mUseNullDecoder.mUse) {
    MOZ_ASSERT(mNullPDM);
    return CreateDecoderWithPDM(mNullPDM, aParams);
  }
  bool isEncrypted = mEMEPDM && aParams.mConfig.mCrypto.IsEncrypted();

  if (isEncrypted) {
    return CreateDecoderWithPDM(mEMEPDM, aParams);
  }

  return CheckAndMaybeCreateDecoder(CreateDecoderParamsForAsync(aParams), 0);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PDMFactory::CheckAndMaybeCreateDecoder(CreateDecoderParamsForAsync&& aParams,
                                       uint32_t aIndex,
                                       Maybe<MediaResult> aEarlierError) {
  uint32_t i = aIndex;
  auto params = SupportDecoderParams(aParams);
  for (; i < mCurrentPDMs.Length(); i++) {
    if (mCurrentPDMs[i]->Supports(params, nullptr /* diagnostic */) ==
        media::DecodeSupport::Unsupported) {
      continue;
    }
    RefPtr<PlatformDecoderModule::CreateDecoderPromise> p =
        CreateDecoderWithPDM(mCurrentPDMs[i], aParams)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [](RefPtr<MediaDataDecoder>&& aDecoder) {
                  return PlatformDecoderModule::CreateDecoderPromise::
                      CreateAndResolve(std::move(aDecoder), __func__);
                },
                [self = RefPtr{this}, i, params = std::move(aParams)](
                    const MediaResult& aError) mutable {
                  // Try the next PDM.
                  return self->CheckAndMaybeCreateDecoder(std::move(params),
                                                          i + 1, Some(aError));
                });
    return p;
  }
  if (aEarlierError) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        std::move(*aEarlierError), __func__);
  }
  return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  nsPrintfCString("Error no decoder found for %s",
                                  aParams.mConfig->mMimeType.get())
                      .get()),
      __func__);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PDMFactory::CreateDecoderWithPDM(PlatformDecoderModule* aPDM,
                                 const CreateDecoderParams& aParams) {
  MOZ_ASSERT(aPDM);
  MediaResult result = NS_OK;

  SupportChecker supportChecker;
  const TrackInfo& config = aParams.mConfig;
  supportChecker.AddMediaFormatChecker(config);

  auto checkResult = supportChecker.Check();
  if (checkResult.mReason != SupportChecker::Reason::kSupported) {
    if (checkResult.mReason ==
        SupportChecker::Reason::kVideoFormatNotSupported) {
      result = checkResult.mMediaResult;
    } else if (checkResult.mReason ==
               SupportChecker::Reason::kAudioFormatNotSupported) {
      result = checkResult.mMediaResult;
    }
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        result, __func__);
  }

  if (config.IsAudio()) {
    RefPtr<PlatformDecoderModule::CreateDecoderPromise> p;
    p = aPDM->AsyncCreateDecoder(aParams)->Then(
        GetCurrentSerialEventTarget(), __func__,
        [params = CreateDecoderParamsForAsync(aParams)](
            RefPtr<MediaDataDecoder>&& aDecoder) {
          RefPtr<MediaDataDecoder> decoder = std::move(aDecoder);
          if (!params.mNoWrapper.mDontUseWrapper) {
            decoder =
                new AudioTrimmer(decoder.forget(), CreateDecoderParams(params));
          }
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndResolve(
              decoder, __func__);
        },
        [](const MediaResult& aError) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              aError, __func__);
        });
    return p;
  }

  if (!config.IsVideo()) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL(
                "Decoder configuration error, expected audio or video.")),
        __func__);
  }

  if ((MP4Decoder::IsH264(config.mMimeType) ||
#ifdef MOZ_AV1
       AOMDecoder::IsAV1(config.mMimeType) ||
#endif
       VPXDecoder::IsVPX(config.mMimeType)) &&
      !aParams.mUseNullDecoder.mUse && !aParams.mNoWrapper.mDontUseWrapper) {
    return MediaChangeMonitor::Create(this, aParams);
  }
  return aPDM->AsyncCreateDecoder(aParams);
}

DecodeSupportSet PDMFactory::SupportsMimeType(
    const nsACString& aMimeType) const {
      
      //直接输出是不行的，aMimeType这个东西是个基类，不支持get()方法
      //nsPrintfCString a = new nsPrintfCString (aMimeType);
      //这样是可以成功输出aMimeType的，虽然不是很优雅
      //printf("\n这里是PDMFactory.cpp里的输出：%s\n",NS_ConvertUTF16toUTF8(NS_ConvertUTF8toUTF16(aMimeType)).get());
      //CreateTrackInfoWithMIMEType()这个方法可能是定义在VideoUtils.cpp里的
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  //printf("\n%s\n",aMimeType);
  if (!trackInfo) {
    //这里感觉不太对，好像H264和H265都有可能在这里输出，而且H264可能在这里输出两次
    //如果H264播放过一次，那么就不会输出了
    //H264只会在第一次播放视频时输出一次，之后就不会再输出了
    //H265在第一次播放的时候也会多输出一次，看来和视频无关，第一次播放视频的时候就是会多输出一次，但原因没有查明
    //难道有什么一开始的轨道什么的？
    //printf("\n%s\n",aMimeType.get());
    //printf("\n######################################################\n");
    //这里的输出是在报错之后的，如果重复播放，H265也不会在这里输出
    //不明白为什么
    //感觉可能有个初始化过程
    //可能和多线程有关
    return DecodeSupport::Unsupported;
  }
  //HEVC可以生成trackInfo，因此HEVC会到Supports方法中

  //printf("\n#####################  the first   ##############################\n");

  return Supports(SupportDecoderParams(*trackInfo), nullptr);
}


DecodeSupportSet PDMFactory::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
      //总是会走到这里
      //算了先不管，专注HEVC
      //printf("##########################################################");
      //HEVC可以被送到这里
      //printf("\n%s\n",aParams.mConfig.mMimeType.get());
  if (mEMEPDM) {
    return mEMEPDM->Supports(aParams, aDiagnostics);
  }
  //此处的逻辑是：依次遍历PDM，如果支持，就返回这个PDM，如果没找到支持的，返回Unsupported
  RefPtr<PlatformDecoderModule> current =
      GetDecoderModule(aParams, aDiagnostics);

  if (!current) {
    //H265并不会走到这里，在这里的时候mMimeType值是video/theora
    //printf("\nthere is PDMFactory::Supports %s\n",aParams.mConfig.mMimeType.get());
    //printf("sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss");
    return DecodeSupport::Unsupported;
  }
 // printf("\n#####################  the second  ##############################\n");
  // We have a PDM - check for + return SW/HW support info

  //printf("the PDMFactory::Supports ");
  //printf("\nthere is PDMFactory::Supports, the  codec is %s , the module is \n",aParams.mConfig.mMimeType.get());
      //让Module输出自己的身份
      //hevc会走到这里，输出的对应模型是GMPDecoderModule
      //尝试改成FFmpegDecoderModule，当前来看Ubuntu上H264解码用的是ffmpeg，
  //GMP这个模型是继承来的Supports方法
  //current->WhoIam();
  //这里返回unsupported，之后返给SupportsMimeType，SupportsMimeType返给MediaFormatReader::OnDemuxerInitDone()，判断不支持
  //GMP这个先会进去，之后再判断不支持
  //我的ubuntu没有安装FFmpeg，当前怀疑这个FFmpeg是firefox自带的，它会自己编译一个库放起来，只给自己用
  return current->Supports(aParams, aDiagnostics);
}

//这里，这里调用了createandstartuppdm()诸方法
//但是下面这些XRE_...方法又是怎么回事？
//PDMFactory到底是负责启动一个新进程，还是根据现在所在的进程启动？
//看看PDMFactory的构造，可能是在MediaFormatReader那里确定在哪个进程上构造PDMFacory的？
//CreatePDMs()这个方法是被PDMFactory()构造函数调用的
void PDMFactory::CreatePDMs() {
  //AVC/HEVC都会走到这里
  //printf("\n check if AVC/HEVC will come to here ,there is PDMFactory::CreatePDMs \n");
  if (StaticPrefs::media_use_blank_decoder()) {
    CreateAndStartupPDM<BlankDecoderModule>();
    // The Blank PDM SupportsMimeType reports true for all codecs; the creation
    // of its decoder is infallible. As such it will be used for all media, we
    // can stop creating more PDM from this point.
    return;
  }

  if (XRE_IsGPUProcess()) {
    CreateGpuPDMs();
  } else if (XRE_IsRDDProcess()) {
    CreateRddPDMs();
  } else if (XRE_IsUtilityProcess()) {
    CreateUtilityPDMs();
  } else if (XRE_IsContentProcess()) {
    CreateContentPDMs();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(
        XRE_IsParentProcess(),
        "PDMFactory is only usable in the Parent/GPU/RDD/Content process");
    CreateDefaultPDMs();
  }
}

void PDMFactory::CreateGpuPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
}

#if defined(MOZ_FFMPEG)
static DecoderDoctorDiagnostics::Flags GetFailureFlagBasedOnFFmpegStatus(
    const FFmpegRuntimeLinker::LinkStatus& aStatus) {
  switch (aStatus) {
    case FFmpegRuntimeLinker::LinkStatus_INVALID_FFMPEG_CANDIDATE:
    case FFmpegRuntimeLinker::LinkStatus_UNUSABLE_LIBAV57:
    case FFmpegRuntimeLinker::LinkStatus_INVALID_LIBAV_CANDIDATE:
    case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_FFMPEG:
    case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_LIBAV:
    case FFmpegRuntimeLinker::LinkStatus_INVALID_CANDIDATE:
      return DecoderDoctorDiagnostics::Flags::LibAVCodecUnsupported;
    default:
      MOZ_DIAGNOSTIC_ASSERT(
          aStatus == FFmpegRuntimeLinker::LinkStatus_NOT_FOUND,
          "Only call this method when linker fails.");
      return DecoderDoctorDiagnostics::Flags::FFmpegNotFound;
  }
}
#endif

void PDMFactory::CreateRddPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() &&
      StaticPrefs::media_rdd_wmf_enabled()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (StaticPrefs::media_rdd_applemedia_enabled()) {
    CreateAndStartupPDM<AppleDecoderModule>();
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::media_ffvpx_enabled() &&
      StaticPrefs::media_rdd_ffvpx_enabled()) {
    CreateAndStartupPDM<FFVPXRuntimeLinker>();
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      StaticPrefs::media_rdd_ffmpeg_enabled() &&
      !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
  }
#endif
  CreateAndStartupPDM<AgnosticDecoderModule>();
}

void PDMFactory::CreateUtilityPDMs() {
  const ipc::SandboxingKind aKind = GetCurrentSandboxingKind();
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() &&
      StaticPrefs::media_utility_wmf_enabled() &&
      aKind == ipc::SandboxingKind::UTILITY_AUDIO_DECODING_WMF) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (StaticPrefs::media_utility_applemedia_enabled() &&
      aKind == ipc::SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA) {
    CreateAndStartupPDM<AppleDecoderModule>();
  }
#endif
  if (aKind == ipc::SandboxingKind::GENERIC_UTILITY) {
#ifdef MOZ_FFVPX
    if (StaticPrefs::media_ffvpx_enabled() &&
        StaticPrefs::media_utility_ffvpx_enabled()) {
      CreateAndStartupPDM<FFVPXRuntimeLinker>();
    }
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_ffmpeg_enabled() &&
        StaticPrefs::media_utility_ffmpeg_enabled() &&
        !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
      mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
          FFmpegRuntimeLinker::LinkStatusCode());
    }
#endif
#ifdef MOZ_WIDGET_ANDROID
    if (StaticPrefs::media_utility_android_media_codec_enabled()) {
      StartupPDM(AndroidDecoderModule::Create(),
                 StaticPrefs::media_android_media_codec_preferred());
    }
#endif
    CreateAndStartupPDM<AgnosticDecoderModule>();
  }
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (aKind == ipc::SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    if (StaticPrefs::media_wmf_media_engine_enabled()) {
      CreateAndStartupPDM<MFMediaEngineDecoderModule>();
    }
  }
#endif
}

void PDMFactory::CreateContentPDMs() {
  if (StaticPrefs::media_gpu_process_decoder()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::GpuProcess);
  }

  if (StaticPrefs::media_rdd_process_enabled()) {
    //输出很多，不确定到底HEVC有没有到这里
    //printf("\n check if AVC/HEVC can come to here ,here is PDMFactory::CreateContentPDMs \n");
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::RddProcess);
  }

  if (StaticPrefs::media_utility_process_enabled()) {
#ifdef MOZ_APPLEMEDIA
    CreateAndStartupPDM<RemoteDecoderModule>(
        RemoteDecodeIn::UtilityProcess_AppleMedia);
#endif
#ifdef XP_WIN
    CreateAndStartupPDM<RemoteDecoderModule>(
        RemoteDecodeIn::UtilityProcess_WMF);
#endif
    // WMF and AppleMedia should be created before Generic because the order
    // affects what decoder module would be chose first.
    CreateAndStartupPDM<RemoteDecoderModule>(
        RemoteDecodeIn::UtilityProcess_Generic);
  }
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (StaticPrefs::media_wmf_media_engine_enabled()) {
    CreateAndStartupPDM<RemoteDecoderModule>(
        RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM);
  }
#endif

#if !defined(MOZ_WIDGET_ANDROID)  // Still required for video?
  if (StaticPrefs::media_allow_audio_non_utility()) {
#endif  // !defined(MOZ_WIDGET_ANDROID)
#ifdef XP_WIN
    if (StaticPrefs::media_wmf_enabled()) {
#  ifdef MOZ_WMF
      if (!StaticPrefs::media_rdd_process_enabled() ||
          !StaticPrefs::media_rdd_wmf_enabled()) {
        if (!CreateAndStartupPDM<WMFDecoderModule>()) {
          mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
        }
      }
#  endif
    } else if (StaticPrefs::media_decoder_doctor_wmf_disabled_is_failure()) {
      mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
    }
#endif

#ifdef MOZ_APPLEMEDIA
    CreateAndStartupPDM<AppleDecoderModule>();
#endif
#ifdef MOZ_OMX
    if (StaticPrefs::media_omx_enabled()) {
      CreateAndStartupPDM<OmxDecoderModule>();
    }
#endif
#ifdef MOZ_FFVPX
    if (StaticPrefs::media_ffvpx_enabled()) {
      CreateAndStartupPDM<FFVPXRuntimeLinker>();
    }
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_ffmpeg_enabled() &&
        !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
      mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
          FFmpegRuntimeLinker::LinkStatusCode());
    }
#endif

    CreateAndStartupPDM<AgnosticDecoderModule>();
#if !defined(MOZ_WIDGET_ANDROID)  // Still required for video?
  }
#endif  // !defined(MOZ_WIDGET_ANDROID)

  // Android still needs this, the actual decoder is remoted on java side
#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::media_android_media_codec_enabled()) {
    StartupPDM(AndroidDecoderModule::Create(),
               StaticPrefs::media_android_media_codec_preferred());
  }
#endif

  if (StaticPrefs::media_gmp_decoder_enabled() &&
      !StartupPDM(GMPDecoderModule::Create(),
                  StaticPrefs::media_gmp_decoder_preferred())) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::GMPPDMFailedToStartup;
  }
}

void PDMFactory::CreateDefaultPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled()) {
    if (!CreateAndStartupPDM<WMFDecoderModule>()) {
      mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
    }
  } else if (StaticPrefs::media_decoder_doctor_wmf_disabled_is_failure()) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
  }
#endif

#ifdef MOZ_APPLEMEDIA
  CreateAndStartupPDM<AppleDecoderModule>();
#endif
#ifdef MOZ_OMX
  if (StaticPrefs::media_omx_enabled()) {
    CreateAndStartupPDM<OmxDecoderModule>();
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::media_ffvpx_enabled()) {
    CreateAndStartupPDM<FFVPXRuntimeLinker>();
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::media_android_media_codec_enabled()) {
    StartupPDM(AndroidDecoderModule::Create(),
               StaticPrefs::media_android_media_codec_preferred());
  }
#endif

  CreateAndStartupPDM<AgnosticDecoderModule>();

  if (StaticPrefs::media_gmp_decoder_enabled() &&
      !StartupPDM(GMPDecoderModule::Create(),
                  StaticPrefs::media_gmp_decoder_preferred())) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::GMPPDMFailedToStartup;
  }
}

void PDMFactory::CreateNullPDM() {
  mNullPDM = CreateNullDecoderModule();
  MOZ_ASSERT(mNullPDM && NS_SUCCEEDED(mNullPDM->Startup()));
}

bool PDMFactory::StartupPDM(already_AddRefed<PlatformDecoderModule> aPDM,
                            bool aInsertAtBeginning) {
  RefPtr<PlatformDecoderModule> pdm = aPDM;
  if (pdm && NS_SUCCEEDED(pdm->Startup())) {
    if (aInsertAtBeginning) {
      mCurrentPDMs.InsertElementAt(0, pdm);
    } else {
      mCurrentPDMs.AppendElement(pdm);
    }
    return true;
  }
  return false;
}

already_AddRefed<PlatformDecoderModule> PDMFactory::GetDecoderModule(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  if (aDiagnostics) {
    // If libraries failed to load, the following loop over mCurrentPDMs
    // will not even try to use them. So we record failures now.
    aDiagnostics->SetFailureFlags(mFailureFlags);
  }
  //HEVC可以进到这里来
  //printf("\n there is GetDecoderModule,the codec is %s \n ",aParams.mConfig.mMimeType.get());

  RefPtr<PlatformDecoderModule> pdm;
  //这个pdm判断有问题吧？
  //H265时，会返回基类的一个实例
  //现在不确定是哪个实例，不是FFmpeg
  for (const auto& current : mCurrentPDMs) {
    //让Module输出自己的身份
    //current->WhoIam();
    //奇怪，HEVC的时候，这里输出的模型总是PlatformDecoderModuleBase，不知道哪个类没有重写继承
    //无法确定，这里一直是输出PlatformDecoderModuleBase
    //不管了，继续向下
    //开始修改
    if (current->Supports(aParams, aDiagnostics) !=
        media::DecodeSupport::Unsupported) {
          //printf("\n there is GetDecoderModule,the codec is %s \n ",aParams.mConfig.mMimeType.get());
          //current->WhoIam();
      pdm = current;

      //pdm->WhoIam();
      break;
    }
  }
  return pdm.forget();
}

void PDMFactory::SetCDMProxy(CDMProxy* aProxy) {
  MOZ_ASSERT(aProxy);

#ifdef MOZ_WIDGET_ANDROID
  if (IsWidevineKeySystem(aProxy->KeySystem())) {
    mEMEPDM = AndroidDecoderModule::Create(aProxy);
    return;
  }
#endif
#ifdef MOZ_WMF_CDM
  if (IsPlayReadyKeySystemAndSupported(aProxy->KeySystem())) {
    mEMEPDM = RemoteDecoderModule::Create(
        RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM);
    return;
  }
#endif
  auto m = MakeRefPtr<PDMFactory>();
  mEMEPDM = MakeRefPtr<EMEDecoderModule>(aProxy, m);
}

/* static */
media::MediaCodecsSupported PDMFactory::Supported(bool aForceRefresh) {
  MOZ_ASSERT(NS_IsMainThread());

  static auto calculate = []() {
    auto pdm = MakeRefPtr<PDMFactory>();
    MediaCodecsSupported supported;
    // H264 and AAC depends on external framework that must be dynamically
    // loaded.
    // We currently only ship a single PDM per platform able to decode AAC or
    // H264. As such we can assert that being able to create a H264 or AAC
    // decoder indicates that with WMF on Windows or FFmpeg on Unixes is
    // available.
    // This logic will have to be revisited if a PDM supporting either codec
    // will be added in addition to the WMF and FFmpeg PDM (such as OpenH264)
    for (const auto& cd : MCSInfo::GetAllCodecDefinitions()) {
      supported += MCSInfo::GetDecodeMediaCodecsSupported(
          cd.codec, pdm->SupportsMimeType(nsCString(cd.mimeTypeString)));
    }
    return supported;
  };

  static MediaCodecsSupported supported = calculate();
  if (aForceRefresh) {
    supported = calculate();
  }

  return supported;
}

//这个方法会被RemoteDecoderManagerChild::Supports()调用
/* static */
DecodeSupportSet PDMFactory::SupportsMimeType(
    const nsACString& aMimeType, const MediaCodecsSupported& aSupported,
    RemoteDecodeIn aLocation) {
      //AVC/HEVC会走到这里
      //printf("\n test if HEVC will come to here , here is PDMFactory::SupportsMimeType \n");
  const TrackSupportSet supports =
      RemoteDecoderManagerChild::GetTrackSupport(aLocation);

  if (supports.contains(TrackSupport::Video)) {
    if (MP4Decoder::IsH264(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::H264, aSupported);
    }
    if (VPXDecoder::IsVP9(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::VP9, aSupported);
    }
    if (VPXDecoder::IsVP8(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::VP8, aSupported);
    }
#ifdef MOZ_AV1
    if (AOMDecoder::IsAV1(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::AV1, aSupported);
    }
#endif
    if (TheoraDecoder::IsTheora(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::Theora, aSupported);
    }
    //加上HEVC
    //OK，有效
    //这个地方的原理大概是，检索是否支持这种类型
    //支持类型的列表在/dom/media/platforms/MediaCodecsSupport
    if (MP4Decoder::IsH265(aMimeType)) {
      //这个地方要看看
      return MCSInfo::GetDecodeSupportSet(MediaCodec::H265, aSupported);
    }
  }

  if (supports.contains(TrackSupport::Audio)) {
    if (MP4Decoder::IsAAC(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::AAC, aSupported);
    }
    if (aMimeType.EqualsLiteral("audio/mpeg")) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::MP3, aSupported);
    }
    if (OpusDataDecoder::IsOpus(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::Opus, aSupported);
    }
    if (VorbisDataDecoder::IsVorbis(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::Vorbis, aSupported);
    }
    if (aMimeType.EqualsLiteral("audio/flac")) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::FLAC, aSupported);
    }
    if (WaveDataDecoder::IsWave(aMimeType)) {
      return MCSInfo::GetDecodeSupportSet(MediaCodec::Wave, aSupported);
    }
  }
  return DecodeSupport::Unsupported;
}

/* static */
bool PDMFactory::AllDecodersAreRemote() {
  return StaticPrefs::media_rdd_process_enabled() &&
#if defined(MOZ_FFVPX)
         StaticPrefs::media_rdd_ffvpx_enabled() &&
#endif
         StaticPrefs::media_rdd_opus_enabled() &&
         StaticPrefs::media_rdd_theora_enabled() &&
         StaticPrefs::media_rdd_vorbis_enabled() &&
         StaticPrefs::media_rdd_vpx_enabled() &&
#if defined(MOZ_WMF)
         StaticPrefs::media_rdd_wmf_enabled() &&
#endif
         StaticPrefs::media_rdd_wav_enabled();
}

#undef PDM_INIT_LOG
}  // namespace mozilla
