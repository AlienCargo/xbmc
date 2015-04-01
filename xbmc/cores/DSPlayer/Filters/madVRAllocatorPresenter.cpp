/*
 * (C) 2006-2014 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "madVRAllocatorPresenter.h"
#include "windowing/WindowingFactory.h"
#include <moreuuids.h>
#include "RendererSettings.h"
#include "Application.h"
#include "ApplicationMessenger.h"
#include "cores/VideoRenderers/RenderManager.h"
#include "guilib/GUIWindowManager.h"
#include "settings/Settings.h"
#include "utils/CharsetConverter.h"
#include "cores/DSPlayer/Filters/MadvrSettings.h"
#include "settings/MediaSettings.h"
#include "settings/DisplaySettings.h"
#include "PixelShaderList.h"

#define ShaderStage_PreScale 0
#define ShaderStage_PostScale 1

extern bool g_bExternalSubtitleTime;

//
// CmadVRAllocatorPresenter
//

CmadVRAllocatorPresenter::CmadVRAllocatorPresenter(HWND hWnd, HRESULT& hr, CStdString& _Error)
  : ISubPicAllocatorPresenterImpl(hWnd, hr)
  , m_ScreenSize(0, 0)
  , m_bIsFullscreen(false)
{
  //Init Variable
  g_renderManager.PreInit(RENDERER_DSHOW);
  m_isDeviceSet = false;
  m_firstBoot = true;

  if (FAILED(hr)) {
    _Error += L"ISubPicAllocatorPresenterImpl failed\n";
    return;
  }

  hr = S_OK;
}

CmadVRAllocatorPresenter::~CmadVRAllocatorPresenter()
{
  if (m_pSRCB) {
    // nasty, but we have to let it know about our death somehow
    ((CSubRenderCallback*)(ISubRenderCallback2*)m_pSRCB)->SetDXRAP(nullptr);
  }

  //Restore Kodi Gui
  m_isDeviceSet = false;
  g_Windowing.GetKodi3DDevice()->SetPixelShader(NULL);
  g_Windowing.ResetForMadvr();

  // the order is important here
  m_pSubPicQueue = nullptr;
  m_pAllocator = nullptr;
  m_pDXR = nullptr;
}

STDMETHODIMP CmadVRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  if (riid != IID_IUnknown && m_pDXR) {
    if (SUCCEEDED(m_pDXR->QueryInterface(riid, ppv))) {
      return S_OK;
    }
  }

  return __super::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CmadVRAllocatorPresenter::SetSubDevice(IDirect3DDevice9* pD3DDev)
{
  if (!pD3DDev)
  {
    // release all resources
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;
    return S_OK;
  }

  Com::SmartSize size;

  if (m_pAllocator) {
    m_pAllocator->ChangeDevice(pD3DDev);
  }
  else
  {
    m_pAllocator = DNew CDX9SubPicAllocator(pD3DDev, size, true);
    if (!m_pAllocator) {
      return E_FAIL;
    }
  }

  HRESULT hr = S_OK;

  if (!m_pSubPicQueue) {
    CAutoLock cAutoLock(this);
    m_pSubPicQueue = g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead > 0
      ? (ISubPicQueue*)DNew CSubPicQueue(g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead, g_dsSettings.pRendererSettings->subtitlesSettings.disableAnimations, m_pAllocator, &hr)
      : (ISubPicQueue*)DNew CSubPicQueueNoThread(m_pAllocator, &hr);
  }
  else {
    m_pSubPicQueue->Invalidate();
  }

  if (SUCCEEDED(hr) && (m_SubPicProvider)) {
    m_pSubPicQueue->SetSubPicProvider(m_SubPicProvider);
  }

  return hr;
}

void CmadVRAllocatorPresenter::SetResolution(float fps)
{
  if (CSettings::Get().GetInt("videoplayer.adjustrefreshrate") != ADJUST_REFRESHRATE_OFF && g_graphicsContext.IsFullScreenRoot())
  {
    RESOLUTION bestRes = g_renderManager.m_pRenderer->ChooseBestMadvrResolution(fps);
    g_graphicsContext.SetVideoResolution(bestRes);
  }
}

// IOsdRenderCallback
STDMETHODIMP CmadVRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
  CLog::Log(LOGDEBUG, "%s madVR device it's ready", __FUNCTION__);

  if (!pD3DDev)
    return S_OK;

  m_pD3DDeviceMadVR = pD3DDev;

  if (!m_firstBoot)
    return S_OK;

  g_graphicsContext.SetFullScreenVideo(true);
  m_firstBoot = false;
  m_isDeviceSet = true;
  g_Windowing.ResetForMadvr();

  return S_OK;
}

STDMETHODIMP CmadVRAllocatorPresenter::ClearBackground(LPCSTR name, REFERENCE_TIME frameStart, RECT *fullOutputRect, RECT *activeVideoRect)
{
  //CLog::Log(LOGDEBUG, "ClearBackground");
  return S_OK;
}

STDMETHODIMP CmadVRAllocatorPresenter::RenderOsd(LPCSTR name, REFERENCE_TIME frameStart, RECT *fullOutputRect, RECT *activeVideoRect)
{
  // restore pixelshader for render kodi gui
  m_pD3DDeviceMadVR->SetPixelShader(NULL);

  // render kodi gui
  g_application.RenderMadvr();

  //restore stagestate for xysubfilter
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  m_pD3DDeviceMadVR->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

  // set false for pixelshader
  m_pD3DDeviceMadVR->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  return S_OK;
}

HRESULT CmadVRAllocatorPresenter::Render(
  REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf,
  int left, int top, int right, int bottom, int width, int height)
{
  Com::SmartRect wndRect(0, 0, width, height);
  Com::SmartRect videoRect(left, top, right, bottom);

  __super::SetPosition(wndRect, videoRect);
  if (!g_bExternalSubtitleTime) {
    SetTime(rtStart);
  }
  if (atpf > 0 && m_pSubPicQueue) {
    m_fps = 10000000.0 / atpf;
    m_pSubPicQueue->SetFPS(m_fps);
  }

  if (!g_renderManager.IsConfigured())
  {

    SetResolution(m_fps);
    
    Com::SmartQIPtr<IMadVRSeekbarControl> pMadVrSeek = m_pDXR;
    pMadVrSeek->DisableSeekbar(true);

    if (CSettings::Get().GetBool("dsplayer.madvrexclusivemode"))
    {
      Com::SmartQIPtr<IMadVRExclusiveModeControl> pMadVrEx = m_pDXR;
      pMadVrEx->DisableExclusiveMode(false);

      Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
      pMadvrSettings->SettingsSetBoolean(L"enableExclusive", true);
      pMadvrSettings->SettingsSetBoolean(L"exclusiveDelay", true);
    }

    m_NativeVideoSize = GetVideoSize(false);
    m_AspectRatio = GetVideoSize(true);

    g_renderManager.Configure(m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy, m_fps, g_dsSettings.pRendererSettings->bAllowFullscreen ? CONF_FLAGS_FULLSCREEN : 0, RENDER_FMT_NONE, 0, 0);
    CLog::Log(LOGDEBUG, "%s Render manager configured (FPS: %f) %i %i %i %i", __FUNCTION__, m_fps, m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy);
  }

  AlphaBltSubPic(Com::SmartSize(width, height));

  return S_OK;
}

// ISubPicAllocatorPresenter
STDMETHODIMP CmadVRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{

  CheckPointer(ppRenderer, E_POINTER);

  if (m_pDXR) {
    return E_UNEXPECTED;
  }

  HRESULT hr = m_pDXR.CoCreateInstance(CLSID_madVR, GetOwner());
  if (!m_pDXR) {
    return E_FAIL;
  }

  // ISubRenderCallback
  Com::SmartQIPtr<ISubRender> pSR = m_pDXR;
  if (!pSR) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  m_pSRCB = DNew CSubRenderCallback(this);
  if (FAILED(pSR->SetCallback(m_pSRCB))) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  // IOsdRenderCallback
  Com::SmartQIPtr<IMadVROsdServices> pOR = m_pDXR;
  if (!pOR) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  m_pORCB = DNew COsdRenderCallback(this);
  if (FAILED(pOR->OsdSetRenderCallback("Kodi.Gui", m_pORCB))) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  Com::SmartQIPtr<IMadVRExclusiveModeControl> pMadVrEx = m_pDXR;
  pMadVrEx->DisableExclusiveMode(true);

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(L"delayPlaybackStart2", CSettings::Get().GetBool("dsplayer.delaymadvrplayback"));
  
  CGraphFilters::Get()->SetMadVrCallback(this);

  (*ppRenderer = (IUnknown*)(INonDelegatingUnknown*)(this))->AddRef();

  MONITORINFO mi;
  mi.cbSize = sizeof(MONITORINFO);
  if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
    m_ScreenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
  }

  return S_OK;
}

void CmadVRAllocatorPresenter::SetMadvrPoisition(CRect wndRect, CRect videoRect)
{
  Com::SmartRect wndR(wndRect.x1, wndRect.y1, wndRect.x2, wndRect.y2);
  Com::SmartRect videoR(videoRect.x1, videoRect.y1, videoRect.x2, videoRect.y2);
  SetPosition(wndR, videoR);
  //CLog::Log(0, "wndR x1: %g   y1: %g   x2: %g   y2: %g - videoR x1: %g   y1: %g   x2: %g   y2: %g", wndRect.x1, wndRect.y1, wndRect.x2, wndRect.y2, videoRect.x1, videoRect.y1, videoRect.x2, videoRect.y2);
}

STDMETHODIMP_(void) CmadVRAllocatorPresenter::SetPosition(RECT w, RECT v)
{
  if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    pBV->SetDefaultSourcePosition();
    pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
  }

  if (Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR) {
    pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
  }
}

STDMETHODIMP_(SIZE) CmadVRAllocatorPresenter::GetVideoSize(bool fCorrectAR)
{
  SIZE size = { 0, 0 };

  if (!fCorrectAR) {
    if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
      pBV->GetVideoSize(&size.cx, &size.cy);
    }
  }
  else {
    if (Com::SmartQIPtr<IBasicVideo2> pBV2 = m_pDXR) {
      pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
    }
  }

  return size;
}

STDMETHODIMP CmadVRAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
  HRESULT hr = E_NOTIMPL;
  if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
  }
  return hr;
}

STDMETHODIMP_(bool) CmadVRAllocatorPresenter::Paint(bool fAll)
{
  return false;
}

void CmadVRAllocatorPresenter::SetPS()
{
  PixelShaderVector& psVec = g_dsSettings.pixelShaderList->GetActivatedPixelShaders();

  for (PixelShaderVector::iterator it = psVec.begin();
    it != psVec.end(); it++)
  {
    CExternalPixelShader *Shader = *it;
    Shader->Load();
    SetPixelShader(Shader->GetSourceData(), nullptr);
    Shader->DeleteSourceData();
  }
};

STDMETHODIMP CmadVRAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
  HRESULT hr = E_NOTIMPL;
  if (Com::SmartQIPtr<IMadVRExternalPixelShaders> pEPS = m_pDXR) {
    if (!pSrcData && !pTarget) {
      hr = pEPS->ClearPixelShaders(false);
    }
    else {
      hr = pEPS->AddPixelShader(pSrcData, pTarget, ShaderStage_PostScale, nullptr);
    }
  }
  return hr;
}

//IPaintCallbackMadvr

void CmadVRAllocatorPresenter::OsdRedrawFrame()
{
  Com::SmartQIPtr<IMadVROsdServices> pOR = m_pDXR;
  pOR->OsdRedrawFrame();
}

void CmadVRAllocatorPresenter::CloseMadvr()
{
  if (Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR) {
    pVW->put_Visible(OAFALSE);
    pVW->put_Owner((OAHWND)NULL);
  }
}

void CmadVRAllocatorPresenter::SettingSetScaling(CStdStringW path, int scaling)
{
  std::vector<std::wstring> vecMadvrScaling =
  {
    L"Nearest Neighbor",
    L"Bilinear",
    L"Dvxa",
    L"Mitchell-Netravali",
    L"Catmull-Rom",
    L"Bicubic50", L"Bicubic60", L"Bicubic75", L"Bicubic100",
    L"SoftCubic50", L"SoftCubic60", L"SoftCubic70", L"SoftCubic80", L"SoftCubic100",
    L"Lanczos3", L"Lanczos4", L"Lanczos8",
    L"Spline36", L"Spline64",
    L"Jinc3", L"Jinc4", L"Jinc8",
    L"Nnedi16", L"Nnedi32", L"Nnedi64", L"Nnedi128", L"Nnedi256"
  };

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetString(path, vecMadvrScaling[scaling].c_str());
}

void CmadVRAllocatorPresenter::SettingSetDoubling(CStdStringW path, int iValue)
{
  CStdStringW strBool, strInt;
  strBool = path + "Enable";
  strInt = path + "Quality";

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(strBool, (iValue>-1));
  if (iValue>-1)
    pMadvrSettings->SettingsSetInteger(strInt, iValue);
}

void CmadVRAllocatorPresenter::SettingSetDoublingCondition(CStdStringW path, int condition)
{
  std::vector<std::wstring> vecMadvrCondition = { L"2.0x", L"1.5x", L"1.2x", L"always" };
  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetString(path, vecMadvrCondition[condition].c_str());
}

void CmadVRAllocatorPresenter::SettingSetQuadrupleCondition(CStdStringW path, int condition)
{
  std::vector<std::wstring> vecMadvrCondition = { L"4.0x", L"3.0x", L"2.4x", L"always" };
  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetString(path, vecMadvrCondition[condition].c_str());
}

void CmadVRAllocatorPresenter::SettingSetDeintActive(CStdStringW path, int iValue)
{
  CStdStringW strAuto = "autoActivateDeinterlacing";
  CStdStringW strIfDoubt = "ifInDoubtDeinterlace";

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(strAuto, (iValue > -1));
  pMadvrSettings->SettingsSetBoolean(strIfDoubt, (iValue == MADVR_DEINT_IFDOUBT_ACTIVE));
}

void CmadVRAllocatorPresenter::SettingSetDeintForce(CStdStringW path, int iValue)
{
  std::vector<std::wstring> vecMadvrCondition = { L"auto", L"film", L"video" };
  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;

  pMadvrSettings->SettingsSetString(path, vecMadvrCondition[iValue].c_str());
}

void CmadVRAllocatorPresenter::SettingSetSmoothmotion(CStdStringW path, int iValue)
{
  CStdStringW stEnabled = "smoothMotionEnabled";
  CStdStringW strMode = "smoothMotionMode";
  std::vector<std::wstring> vecMadvrMode = { L"avoidJudder", L"almostAlways", L"always" };

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(stEnabled, (iValue > -1));
  if (iValue > -1)
    pMadvrSettings->SettingsSetString(strMode, vecMadvrMode[iValue].c_str());
}

void CmadVRAllocatorPresenter::SettingSetDithering(CStdStringW path, int iValue)
{
  CStdStringW stDisable = "dontDither";
  CStdStringW strMode = "ditheringAlgo";
  std::vector<std::wstring> vecMadvrMode = { L"random", L"ordered", L"errorDifMedNoise", L"errorDifLowNoise" };

  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(stDisable, (iValue == -1));
  if (iValue > -1)
    pMadvrSettings->SettingsSetString(strMode, vecMadvrMode[iValue].c_str());
}

void CmadVRAllocatorPresenter::SettingSetBool(CStdStringW path, BOOL bValue)
{
  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetBoolean(path, bValue);
}

void CmadVRAllocatorPresenter::SettingSetInt(CStdStringW path, int iValue)
{
  Com::SmartQIPtr<IMadVRSettings> pMadvrSettings = m_pDXR;
  pMadvrSettings->SettingsSetInteger(path, iValue);
}

CStdString CmadVRAllocatorPresenter::GetDXVADecoderDescription()
{
  CStdString strDXVA, strDecoding;
  bool bDecoding, bDeinterlace, bScaling;

  Com::SmartQIPtr<IMadVRInfo> pMadvrInfo = m_pDXR;
  pMadvrInfo->GetBool("dxvaDecodingActive", &bDecoding);
  pMadvrInfo->GetBool("dxvaDeinterlacingActive", &bDeinterlace);
  pMadvrInfo->GetBool("dxvaScalingActive", &bScaling);

  bDecoding ? strDecoding = "DXVA Decoding" : strDecoding = "Not using DXVA";

  strDXVA = strDecoding;
  if (bDeinterlace)
    strDXVA += ", DXVA Deinterlacing";
  if (bScaling)
    strDXVA += ", DXVA Scaling";

  return strDXVA;
};

LPDIRECT3DDEVICE9 CmadVRAllocatorPresenter::GetDevice()
{
  if (m_isDeviceSet)
  {
    //CLog::Log(0, "device madvr");
    return m_pD3DDeviceMadVR;
  }
  else
  {
    //CLog::Log(0, "device kodi");
    return g_Windowing.GetKodi3DDevice();
  }
}

