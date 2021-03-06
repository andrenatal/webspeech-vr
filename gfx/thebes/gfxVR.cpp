/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "gfxVR.h"
#include "ovr_capi_dynamic.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h" // for gfxWindowsPlatform::GetDPIScale
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

namespace {

#ifdef OVR_CAPI_LIMITED_MOZILLA
static pfn_ovr_Initialize ovr_Initialize = nullptr;
static pfn_ovr_Shutdown ovr_Shutdown = nullptr;
static pfn_ovrHmd_Detect ovrHmd_Detect = nullptr;
static pfn_ovrHmd_Create ovrHmd_Create = nullptr;
static pfn_ovrHmd_Destroy ovrHmd_Destroy = nullptr;
static pfn_ovrHmd_CreateDebug ovrHmd_CreateDebug = nullptr;
static pfn_ovrHmd_GetLastError ovrHmd_GetLastError = nullptr;
static pfn_ovrHmd_StartSensor ovrHmd_StartSensor = nullptr;
static pfn_ovrHmd_StopSensor ovrHmd_StopSensor = nullptr;
static pfn_ovrHmd_ResetSensor ovrHmd_ResetSensor = nullptr;
static pfn_ovrHmd_GetSensorState ovrHmd_GetSensorState = nullptr;
static pfn_ovrHmd_GetSensorDesc ovrHmd_GetSensorDesc = nullptr;
static pfn_ovrHmd_GetDesc ovrHmd_GetDesc = nullptr;
static pfn_ovrHmd_GetFovTextureSize ovrHmd_GetFovTextureSize = nullptr;
static pfn_ovrHmd_GetRenderDesc ovrHmd_GetRenderDesc = nullptr;
static pfn_ovrHmd_CreateDistortionMesh ovrHmd_CreateDistortionMesh = nullptr;
static pfn_ovrHmd_DestroyDistortionMesh ovrHmd_DestroyDistortionMesh = nullptr;
static pfn_ovrHmd_GetRenderScaleAndOffset ovrHmd_GetRenderScaleAndOffset = nullptr;
static pfn_ovrHmd_GetFrameTiming ovrHmd_GetFrameTiming = nullptr;
static pfn_ovrHmd_BeginFrameTiming ovrHmd_BeginFrameTiming = nullptr;
static pfn_ovrHmd_EndFrameTiming ovrHmd_EndFrameTiming = nullptr;
static pfn_ovrHmd_ResetFrameTiming ovrHmd_ResetFrameTiming = nullptr;
static pfn_ovrHmd_GetEyePose ovrHmd_GetEyePose = nullptr;
static pfn_ovrHmd_GetEyeTimewarpMatrices ovrHmd_GetEyeTimewarpMatrices = nullptr;
static pfn_ovrMatrix4f_Projection ovrMatrix4f_Projection = nullptr;
static pfn_ovrMatrix4f_OrthoSubProjection ovrMatrix4f_OrthoSubProjection = nullptr;
static pfn_ovr_GetTimeInSeconds ovr_GetTimeInSeconds = nullptr;

#if defined(XP_WIN)
# ifdef HAVE_64BIT_OS
#  define OVR_LIB_NAME "libovr64.dll"
# else
#  define OVR_LIB_NAME "libovr.dll"
# endif
#elif defined(XP_MACOSX)
# define OVR_LIB_NAME "libovr.dylib"
#else
# define OVR_LIB_NAME 0
#endif

static bool
InitializeOculusCAPI()
{
  static PRLibrary *ovrlib = nullptr;

  if (!ovrlib) {
    const char *libName = OVR_LIB_NAME;

    if (PR_GetEnv("OVR_LIB_NAME")) {
      libName = PR_GetEnv("OVR_LIB_NAME");
    }

    if (!libName) {
      printf_stderr("Don't know how to find Oculus VR library; missing OVR_LIB_NAME\n");
      return false;
    }

    ovrlib = PR_LoadLibrary(libName);

    if (!ovrlib) {
      // Not found? Try harder. Needed mainly on OSX/etc. where
      // the binary location is not in the search path.
      const char *xulName = "libxul.so";
#if defined(XP_MACOSX)
      xulName = "XUL";
#endif

      char *xulpath = PR_GetLibraryFilePathname(xulName, (PRFuncPtr) &InitializeOculusCAPI);
      if (xulpath) {
        char *xuldir = strrchr(xulpath, '/');
        if (xuldir) {
          *xuldir = 0;
          xuldir = xulpath;

          char *ovrpath = PR_GetLibraryName(xuldir, libName);
          ovrlib = PR_LoadLibrary(ovrpath);
          PR_Free(ovrpath);
        }
        PR_Free(xulpath);
      }
    }

    if (!ovrlib) {
      printf_stderr("Failed to load Oculus VR library, tried '%s'\n", libName);
      return false;
    }
  }

  // was it already initialized?
  if (ovr_Initialize)
    return true;

#define REQUIRE_FUNCTION(_x) do { \
    *(void **)&_x = (void *) PR_FindSymbol(ovrlib, #_x);                \
    if (!_x) { printf_stderr(#_x " symbol missing\n"); goto fail; }       \
  } while (0)

  REQUIRE_FUNCTION(ovr_Initialize);
  REQUIRE_FUNCTION(ovr_Shutdown);
  REQUIRE_FUNCTION(ovrHmd_Detect);
  REQUIRE_FUNCTION(ovrHmd_Create);
  REQUIRE_FUNCTION(ovrHmd_Destroy);
  REQUIRE_FUNCTION(ovrHmd_CreateDebug);
  REQUIRE_FUNCTION(ovrHmd_GetLastError);
  REQUIRE_FUNCTION(ovrHmd_StartSensor);
  REQUIRE_FUNCTION(ovrHmd_StopSensor);
  REQUIRE_FUNCTION(ovrHmd_ResetSensor);
  REQUIRE_FUNCTION(ovrHmd_GetSensorState);
  REQUIRE_FUNCTION(ovrHmd_GetSensorDesc);
  REQUIRE_FUNCTION(ovrHmd_GetDesc);
  REQUIRE_FUNCTION(ovrHmd_GetFovTextureSize);
  REQUIRE_FUNCTION(ovrHmd_GetRenderDesc);
  REQUIRE_FUNCTION(ovrHmd_CreateDistortionMesh);
  REQUIRE_FUNCTION(ovrHmd_DestroyDistortionMesh);
  REQUIRE_FUNCTION(ovrHmd_GetRenderScaleAndOffset);
  REQUIRE_FUNCTION(ovrHmd_GetFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_BeginFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_EndFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_ResetFrameTiming);
  REQUIRE_FUNCTION(ovrHmd_GetEyePose);
  REQUIRE_FUNCTION(ovrHmd_GetEyeTimewarpMatrices);
  REQUIRE_FUNCTION(ovrMatrix4f_Projection);
  REQUIRE_FUNCTION(ovrMatrix4f_OrthoSubProjection);
  REQUIRE_FUNCTION(ovr_GetTimeInSeconds);

#undef REQUIRE_FUNCTION

  return true;

 fail:
  ovr_Initialize = nullptr;
  return false;
}

#else
// we're statically linked; it's available
static bool InitializeOculusCAPI()
{
  return true;
}
#endif

} // anonymous namespace

using namespace mozilla::gfx;

static bool sOculusInitialized = false;

class HMDInfoOculus : public VRHMDInfo {
  friend class VRHMDManagerOculus;
public:
  HMDInfoOculus(ovrHmd aHMD);
  virtual ~HMDInfoOculus() { Destroy(); }

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight) MOZ_OVERRIDE;

  bool StartSensorTracking() MOZ_OVERRIDE;
  VRHMDSensorState GetSensorState(double timeOffset) MOZ_OVERRIDE;
  void StopSensorTracking() MOZ_OVERRIDE;

  void FillDistortionConstants(uint32_t whichEye,
                               const IntSize& textureSize, const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) MOZ_OVERRIDE;

protected:
  ovrHmd mHMD;
  ovrFovPort mFOVPort[2];
  uint32_t mStartCount;

  void Destroy();
};

static ovrFovPort
ToFovPort(const VRFieldOfView& aFOV)
{
  ovrFovPort fovPort;
  fovPort.LeftTan = tan(aFOV.leftDegrees * M_PI / 180.0);
  fovPort.RightTan = tan(aFOV.rightDegrees * M_PI / 180.0);
  fovPort.UpTan = tan(aFOV.upDegrees * M_PI / 180.0);
  fovPort.DownTan = tan(aFOV.downDegrees * M_PI / 180.0);
  return fovPort;
}

static VRFieldOfView
FromFovPort(const ovrFovPort& aFOV)
{
  VRFieldOfView fovInfo;
  fovInfo.leftDegrees = atan(aFOV.LeftTan) * 180.0 / M_PI;
  fovInfo.rightDegrees = atan(aFOV.RightTan) * 180.0 / M_PI;
  fovInfo.upDegrees = atan(aFOV.UpTan) * 180.0 / M_PI;
  fovInfo.downDegrees = atan(aFOV.DownTan) * 180.0 / M_PI;
  return fovInfo;
}

HMDInfoOculus::HMDInfoOculus(ovrHmd aHMD)
  : VRHMDInfo(VRHMDType::Oculus)
  , mHMD(aHMD)
  , mStartCount(0)
{
  ovrHmdDesc desc;
  ovrHmd_GetDesc(aHMD, &desc);

  mSupportedSensorBits = 0;
  if (desc.SensorCaps & ovrSensorCap_Orientation)
    mSupportedSensorBits |= State_Orientation;
  if (desc.SensorCaps & ovrSensorCap_Position)
    mSupportedSensorBits |= State_Position;

  mRecommendedEyeFOV[Eye_Left] = FromFovPort(desc.DefaultEyeFov[ovrEye_Left]);
  mRecommendedEyeFOV[Eye_Right] = FromFovPort(desc.DefaultEyeFov[ovrEye_Right]);

  mMaximumEyeFOV[Eye_Left] = FromFovPort(desc.MaxEyeFov[ovrEye_Left]);
  mMaximumEyeFOV[Eye_Right] = FromFovPort(desc.MaxEyeFov[ovrEye_Right]);

  SetFOV(mRecommendedEyeFOV[Eye_Left], mRecommendedEyeFOV[Eye_Right]);

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    screenmgr->ScreenForRect(desc.WindowsPos.x, desc.WindowsPos.y,
                             desc.Resolution.w, desc.Resolution.h,
                             getter_AddRefs(mScreen));
  }
}

void
HMDInfoOculus::Destroy()
{
  if (mHMD) {
    ovrHmd_Destroy(mHMD);
    mHMD = nullptr;
  }
}

bool
HMDInfoOculus::SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight)
{
  float pixelsPerDisplayPixel = 1.0;
  ovrSizei texSize[2];

  uint32_t caps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette; // XXX TODO add TimeWarp

  // get eye parameters and create the mesh
  for (uint32_t eye = 0; eye < NumEyes; eye++) {
    mEyeFOV[eye] = eye == 0 ? aFOVLeft : aFOVRight;
    mFOVPort[eye] = ToFovPort(mEyeFOV[eye]);

    ovrEyeRenderDesc renderDesc = ovrHmd_GetRenderDesc(mHMD, (ovrEyeType) eye, mFOVPort[eye]);
    mEyeTranslation[eye] = Point3D(renderDesc.ViewAdjust.x, renderDesc.ViewAdjust.y, renderDesc.ViewAdjust.z);

    // We set near to epsilon and far to 1.0, and then we scale in the
    // compositor's viewport transform.
    // XXX The actual znear/zfar probably need to come from CSS, or we
    // can even accept them as part of the SetFOV call on the VR HMD
    // device, but I think we can do that scaling later.
    ovrMatrix4f projMatrix = ovrMatrix4f_Projection(mFOVPort[eye], 0.0001f, 1.0f, false);
    // XXX this is gross, we really need better methods on Matrix4x4
    memcpy(&mEyeProjectionMatrix[eye], projMatrix.M, sizeof(ovrMatrix4f));
    mEyeProjectionMatrix[eye].Transpose();

    // XXX need to do better here
    texSize[eye] = ovrHmd_GetFovTextureSize(mHMD, (ovrEyeType) eye, mFOVPort[eye], pixelsPerDisplayPixel);

    ovrDistortionMesh mesh;
    bool ok = ovrHmd_CreateDistortionMesh(mHMD, (ovrEyeType) eye, mFOVPort[eye], caps, &mesh);
    if (!ok)
      return false;

    mDistortionMesh[eye].mVertices.SetLength(mesh.VertexCount);
    mDistortionMesh[eye].mIndices.SetLength(mesh.IndexCount);

    ovrDistortionVertex *srcv = mesh.pVertexData;
    VRDistortionVertex *destv = mDistortionMesh[eye].mVertices.Elements();
    memset(destv, 0, mesh.VertexCount * sizeof(VRDistortionVertex));
    for (uint32_t i = 0; i < mesh.VertexCount; ++i) {
      destv[i].pos[0] = srcv[i].Pos.x;
      destv[i].pos[1] = srcv[i].Pos.y;

      destv[i].texR[0] = srcv[i].TexR.x;
      destv[i].texR[1] = srcv[i].TexR.y;
      destv[i].texG[0] = srcv[i].TexG.x;
      destv[i].texG[1] = srcv[i].TexG.y;
      destv[i].texB[0] = srcv[i].TexB.x;
      destv[i].texB[1] = srcv[i].TexB.y;

      destv[i].genericAttribs[0] = srcv[i].VignetteFactor;
      destv[i].genericAttribs[1] = srcv[i].TimeWarpFactor;
    }

    memcpy(mDistortionMesh[eye].mIndices.Elements(), mesh.pIndexData, mesh.IndexCount * sizeof(uint16_t));
    ovrHmd_DestroyDistortionMesh(&mesh);
  }

  // take the max of both for eye resolution
  mEyeResolution.width = std::max(texSize[Eye_Left].w, texSize[Eye_Right].w);
  mEyeResolution.height = std::max(texSize[Eye_Left].h, texSize[Eye_Right].h);

  mConfiguration.hmdType = mType;
  mConfiguration.value = 0;
  mConfiguration.fov[0] = aFOVLeft;
  mConfiguration.fov[1] = aFOVRight;

  return true;
  //* need to call this during rendering each frame I think? */
  //ovrHmd_GetRenderScaleAndOffset(fovPort, texSize, renderViewport, uvScaleOffsetOut);
}

void
HMDInfoOculus::FillDistortionConstants(uint32_t whichEye,
                                       const IntSize& textureSize,
                                       const IntRect& eyeViewport,
                                       const Size& destViewport,
                                       const Rect& destRect,
                                       VRDistortionConstants& values)
{
  ovrSizei texSize = { textureSize.width, textureSize.height };
  ovrRecti eyePort = { { eyeViewport.x, eyeViewport.y }, { eyeViewport.width, eyeViewport.height } };
  ovrVector2f scaleOut[2];

  ovrHmd_GetRenderScaleAndOffset(mFOVPort[whichEye], texSize, eyePort, scaleOut);

  values.eyeToSourceScaleAndOffset[0] = scaleOut[0].x;
  values.eyeToSourceScaleAndOffset[1] = scaleOut[0].y;
  values.eyeToSourceScaleAndOffset[2] = scaleOut[1].x;
  values.eyeToSourceScaleAndOffset[3] = scaleOut[1].y;

  // These values are in clip space [-1..1] range, but we're providing
  // scaling in the 0..2 space for sanity.

  // this is the destRect in clip space
  float x0 = destRect.x / destViewport.width * 2.0 - 1.0;
  float x1 = (destRect.x + destRect.width) / destViewport.width * 2.0 - 1.0;

  float y0 = destRect.y / destViewport.height * 2.0 - 1.0;
  float y1 = (destRect.y + destRect.height) / destViewport.height * 2.0 - 1.0;

  // offset
  values.destinationScaleAndOffset[0] = (x0+x1) / 2.0;
  values.destinationScaleAndOffset[1] = (y0+y1) / 2.0;
  // scale
  values.destinationScaleAndOffset[2] = destRect.width / destViewport.width;
  values.destinationScaleAndOffset[3] = destRect.height / destViewport.height;
}

bool
HMDInfoOculus::StartSensorTracking()
{
  if (mStartCount == 0) {
    bool ok = ovrHmd_StartSensor(mHMD, ovrSensorCap_Orientation | ovrSensorCap_Position, 0);
    if (!ok)
      return false;
  }

  mStartCount++;
  return true;
}

void
HMDInfoOculus::StopSensorTracking()
{
  if (--mStartCount == 0) {
    ovrHmd_StopSensor(mHMD);
  }
}

VRHMDSensorState
HMDInfoOculus::GetSensorState(double timeOffset)
{
  VRHMDSensorState result;
  result.Clear();

  // XXX this is the wrong time base for timeOffset; we need to figure out how to synchronize
  // the Oculus time base and the browser one.
  ovrSensorState state = ovrHmd_GetSensorState(mHMD, ovr_GetTimeInSeconds() + timeOffset);
  ovrPoseStatef& pose(state.Predicted);

  result.timestamp = pose.TimeInSeconds;

  if (state.StatusFlags & ovrStatus_OrientationTracked) {
    result.flags |= State_Orientation;

    result.orientation[0] = pose.Pose.Orientation.x;
    result.orientation[1] = pose.Pose.Orientation.y;
    result.orientation[2] = pose.Pose.Orientation.z;
    result.orientation[3] = pose.Pose.Orientation.w;
    
    result.angularVelocity[0] = pose.AngularVelocity.x;
    result.angularVelocity[1] = pose.AngularVelocity.y;
    result.angularVelocity[2] = pose.AngularVelocity.z;

    result.angularAcceleration[0] = pose.AngularAcceleration.x;
    result.angularAcceleration[1] = pose.AngularAcceleration.y;
    result.angularAcceleration[2] = pose.AngularAcceleration.z;
  }

  if (state.StatusFlags & ovrStatus_PositionTracked) {
    result.flags |= State_Position;

    result.position[0] = pose.Pose.Position.x;
    result.position[1] = pose.Pose.Position.y;
    result.position[2] = pose.Pose.Position.z;
    
    result.linearVelocity[0] = pose.LinearVelocity.x;
    result.linearVelocity[1] = pose.LinearVelocity.y;
    result.linearVelocity[2] = pose.LinearVelocity.z;

    result.linearAcceleration[0] = pose.LinearAcceleration.x;
    result.linearAcceleration[1] = pose.LinearAcceleration.y;
    result.linearAcceleration[2] = pose.LinearAcceleration.z;
  }

  return result;
}

static nsTArray<RefPtr<HMDInfoOculus> > sOculusHMDs;

bool
VRHMDManagerOculus::Init()
{
  if (sOculusInitialized)
    return true;

  if (!InitializeOculusCAPI())
    return false;

  ovr_Initialize();
  int count = ovrHmd_Detect();

  for (int i = 0; i < count; ++i) {
    ovrHmd hmd = ovrHmd_Create(i);
    RefPtr<HMDInfoOculus> oc = new HMDInfoOculus(hmd);
    sOculusHMDs.AppendElement(oc);
  }

  if (count == 0) {
    ovrHmd hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
    RefPtr<HMDInfoOculus> oc = new HMDInfoOculus(hmd);
    sOculusHMDs.AppendElement(oc);
  }

  sOculusInitialized = true;
  return true;
}

void
VRHMDManagerOculus::Destroy()
{
  if (!sOculusInitialized)
    return;

  for (size_t i = 0; i < sOculusHMDs.Length(); ++i) {
    sOculusHMDs[i]->Destroy();
  }
  sOculusHMDs.Clear();

  ovr_Shutdown();
  sOculusInitialized = false;
}

void
VRHMDManagerOculus::GetOculusHMDs(nsTArray<RefPtr<VRHMDInfo> >& aHMDResult)
{
  Init();
  for (size_t i = 0; i < sOculusHMDs.Length(); ++i) {
    aHMDResult.AppendElement(sOculusHMDs[i]);
  }
}
