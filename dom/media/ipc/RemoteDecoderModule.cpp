/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderModule.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "OpusDecoder.h"
#include "RemoteAudioDecoder.h"
#include "RemoteDecoderManagerChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "gfxConfig.h"
#include "mozilla/RemoteDecodeUtils.h"

namespace mozilla {

using namespace ipc;
using namespace layers;

already_AddRefed<PlatformDecoderModule> RemoteDecoderModule::Create(
    RemoteDecodeIn aLocation) {
  MOZ_ASSERT(!XRE_IsGPUProcess() && !XRE_IsRDDProcess(),
             "Should not be created in GPU or RDD process.");
  if (!XRE_IsContentProcess()) {
    // For now, the RemoteDecoderModule is only available in the content
    // process.
    return nullptr;
  }
  return MakeAndAddRef<RemoteDecoderModule>(aLocation);
}

//看看HEVC能不能创建这个类
//mLocation这个东西是外部传进来的
//单例，用的是create方法创建的
//似乎这个方法是被PDMFactory::CreateAndStartupPDM方法调用的
//也可能被MFMediaEngineeDcoderModule调用，这个东西是windows下的东西，不用考虑
//关键是这个aLocation是怎么回事
//PDMFactory::CreateAndStartupPDM()方法会传入aLocation
//感觉不应该，在创建时，aLocation应该就是RDD才对
RemoteDecoderModule::RemoteDecoderModule(RemoteDecodeIn aLocation)
    : mLocation(aLocation) {
      //似乎是构造了这个类
      //进一步调查一下
      //printf("\n RemoteDecoderModule is constructing \n");
    }

media::DecodeSupportSet RemoteDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  MOZ_CRASH("Deprecated: Use RemoteDecoderModule::Supports");
}  // namespace mozilla

media::DecodeSupportSet RemoteDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  bool supports =
      RemoteDecoderManagerChild::Supports(mLocation, aParams, aDiagnostics);
  // This should only be supported by mf media engine cdm process.
  if (aParams.mMediaEngineId &&
      mLocation != RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM) {
    supports = false;
  }
  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Sandbox %s decoder %s requested type %s",
           RemoteDecodeInToStr(mLocation), supports ? "supports" : "rejects",
           aParams.MimeType().get()));
  if (supports) {
    // TODO: Note that we do not yet distinguish between SW/HW decode support.
    //       Will be done in bug 1754239.
    //printf("\n check does HEVC/AVC will output in there \n");
    return media::DecodeSupport::SoftwareDecode;
  }
  //HEVC在这里有输出而AVC没有，先尝试一下在这里修改吧
  //printf("\n check does HEVC/AVC will output in there \n");
  return media::DecodeSupport::Unsupported;
}

RefPtr<RemoteDecoderModule::CreateDecoderPromise>
RemoteDecoderModule::AsyncCreateDecoder(const CreateDecoderParams& aParams) {
  //嗯，AVC能走到这里而HEVC不能
  //这个类是PlatformDecoderModule的一个派生类，对这个方法的调用可能是通过Platformecoderodule来执行的
  //printf("check RemoteDecoderModule AsyncCreateDecoder does used");
  if (aParams.mConfig.IsAudio()) {
    // OpusDataDecoder will check this option to provide the same info
    // that IsDefaultPlaybackDeviceMono provides.  We want to avoid calls
    // to IsDefaultPlaybackDeviceMono on RDD because initializing audio
    // backends on RDD will be blocked by the sandbox.
    if (OpusDataDecoder::IsOpus(aParams.mConfig.mMimeType) &&
        IsDefaultPlaybackDeviceMono()) {
      CreateDecoderParams params = aParams;
      params.mOptions += CreateDecoderParams::Option::DefaultPlaybackDeviceMono;
      return RemoteDecoderManagerChild::CreateAudioDecoder(params, mLocation);
    }
    return RemoteDecoderManagerChild::CreateAudioDecoder(aParams, mLocation);
  }
  return RemoteDecoderManagerChild::CreateVideoDecoder(aParams, mLocation);
}

}  // namespace mozilla
