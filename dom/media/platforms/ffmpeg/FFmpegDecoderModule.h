/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDecoderModule_h__
#define __FFmpegDecoderModule_h__

#include "FFmpegAudioDecoder.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegVideoDecoder.h"
#include "PlatformDecoderModule.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

template <int V>
class FFmpegDecoderModule : public PlatformDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create(
      FFmpegLibWrapper* aLib) {
    RefPtr<PlatformDecoderModule> pdm = new FFmpegDecoderModule(aLib);

    return pdm.forget();
  }

  explicit FFmpegDecoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) {}
  virtual ~FFmpegDecoderModule() = default;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override {
    if (Supports(SupportDecoderParams(aParams), nullptr) ==
        media::DecodeSupport::Unsupported) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder = new FFmpegVideoDecoder<V>(
        mLib, aParams.VideoConfig(), aParams.mKnowsCompositor,
        aParams.mImageContainer,
        aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency),
        aParams.mOptions.contains(
            CreateDecoderParams::Option::HardwareDecoderNotAllowed),
        aParams.mTrackingId);
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override {
    if (Supports(SupportDecoderParams(aParams), nullptr) ==
        media::DecodeSupport::Unsupported) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder =
        new FFmpegAudioDecoder<V>(mLib, aParams.AudioConfig());
    return decoder.forget();
  }

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (!trackInfo) {
      return media::DecodeSupport::Unsupported;
    }
    return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
  }

//在此处增加HEVC对应的逻辑
  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
    // This should only be supported by MFMediaEngineDecoderModule.
    if (aParams.mMediaEngineId) {
      return media::DecodeSupport::Unsupported;
    }

    const auto& trackInfo = aParams.mConfig;
    const nsACString& mimeType = trackInfo.mMimeType;

    //嗯，如果是VPX编码，就直接返回不支持
    // Temporary - forces use of VPXDecoder when alpha is present.
    // Bug 1263836 will handle alpha scenario once implemented. It will shift
    // the check for alpha to PDMFactory but not itself remove the need for a
    // check.
    if (VPXDecoder::IsVPX(mimeType) && trackInfo.GetAsVideoInfo()->HasAlpha()) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg decoder rejects requested type '%s'",
               mimeType.BeginReading()));
      return media::DecodeSupport::Unsupported;
    }


    //这里有一个GetCodecId方法，注意
    //AVCodecID是个枚举，并且里面有HEVC
    //注意，这个类是模板类，V是版本号,
    //GetCodecId这个方法是static，这里首先是个判断，后面的阶段可能才有new
    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(mimeType);
    AVCodecID audioCodec = FFmpegAudioDecoder<V>::GetCodecId(mimeType);
    //可以走到这里，但是枚举值好像不对
    //看一下GetCodecId方法
    //这样输出时，枚举值是0(AV_CODEC_ID_NONE)，不支持HEVC
    //printf("\n 枚举：%d，mimeType: %s\n",videoCodec,NS_ConvertUTF16toUTF8(NS_ConvertUTF8toUTF16(mimeType)).get());
    if (audioCodec == AV_CODEC_ID_NONE && videoCodec == AV_CODEC_ID_NONE) {
      //H265可以走到这里，AVC不会
      //printf("\n 枚举：%d，mimeType: %s\n",videoCodec,NS_ConvertUTF16toUTF8(NS_ConvertUTF8toUTF16(mimeType)).get());
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg decoder rejects requested type '%s'",
               mimeType.BeginReading()));
      return media::DecodeSupport::Unsupported;
    }
    

    AVCodecID codec = audioCodec != AV_CODEC_ID_NONE ? audioCodec : videoCodec;

    //看看能不能走到这里
    bool supports = !!FFmpegDataDecoder<V>::FindAVCodec(mLib, codec);
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("FFmpeg decoder %s requested type '%s'",
             supports ? "supports" : "rejects", mimeType.BeginReading()));
    if (supports) {
      // TODO: Note that we do not yet distinguish between SW/HW decode support.
      //       Will be done in bug 1754239.
      return media::DecodeSupport::SoftwareDecode;
    }
    return media::DecodeSupport::Unsupported;
  }

  //用于输出自己的身份
  virtual void  WhoIam() const override
  {
    printf("\nI am FFmpegDecoderModule\n");
  } 

 protected:
  bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
#if defined(MOZ_WIDGET_ANDROID)
    return aColorDepth == gfx::ColorDepth::COLOR_8;
#endif
    return true;
  }

 private:
  FFmpegLibWrapper* mLib;
};

}  // namespace mozilla

#endif  // __FFmpegDecoderModule_h__
