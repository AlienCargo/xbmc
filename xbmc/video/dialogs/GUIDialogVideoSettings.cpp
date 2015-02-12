/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#include "GUIDialogVideoSettings.h"
#include "GUIPassword.h"
#include "addons/Skin.h"
#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#include "cores/VideoRenderers/RenderFlags.h"
#endif
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "profiles/ProfilesManager.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/log.h"
#include "video/VideoDatabase.h"
#ifdef HAS_DS_PLAYER
#include "cores/DSPlayer/Filters/RendererSettings.h"
#include "cores/DSPlayer/dsgraph.h"
#include "dialogs/GUIDialogSelect.h"
#include "DSUtil/DSUtil.h"
#include "utils/CharsetConverter.h"
#include "guilib/LocalizeStrings.h"
#include "application.h"
#endif


#define SETTING_VIDEO_CROP                "video.crop"
#define SETTING_VIDEO_VIEW_MODE           "video.viewmode"
#define SETTING_VIDEO_ZOOM                "video.zoom"
#define SETTING_VIDEO_PIXEL_RATIO         "video.pixelratio"
#define SETTING_VIDEO_BRIGHTNESS          "video.brightness"
#define SETTING_VIDEO_CONTRAST            "video.contrast"
#define SETTING_VIDEO_GAMMA               "video.gamma"
#define SETTING_VIDEO_NONLIN_STRETCH      "video.nonlinearstretch"
#define SETTING_VIDEO_POSTPROCESS         "video.postprocess"
#define SETTING_VIDEO_VERTICAL_SHIFT      "video.verticalshift"

#define SETTING_VIDEO_VDPAU_NOISE         "vdpau.noise"
#define SETTING_VIDEO_VDPAU_SHARPNESS     "vdpau.sharpness"

#define SETTING_VIDEO_DEINTERLACEMODE     "video.deinterlacemode"
#define SETTING_VIDEO_INTERLACEMETHOD     "video.interlacemethod"
#define SETTING_VIDEO_SCALINGMETHOD       "video.scalingmethod"

#ifdef HAS_DS_PLAYER
#define VIDEO_SETTINGS_DS_STATS           "video.dsstats"
#define VIDEO_SETTINGS_DS_FILTERS         "video.dsfilters"
#endif

#define SETTING_VIDEO_STEREOSCOPICMODE    "video.stereoscopicmode"
#define SETTING_VIDEO_STEREOSCOPICINVERT  "video.stereoscopicinvert"

#define SETTING_VIDEO_MAKE_DEFAULT        "video.save"
#define SETTING_VIDEO_CALIBRATION         "video.calibration"

using namespace std;

CGUIDialogVideoSettings::CGUIDialogVideoSettings()
    : CGUIDialogSettingsManualBase(WINDOW_DIALOG_VIDEO_OSD_SETTINGS, "VideoOSDSettings.xml"),
      m_viewModeChanged(false)
{

#ifdef HAS_DS_PLAYER
  m_scalingMethod = 0;
  m_dsStats = 0;	 
#endif

}

CGUIDialogVideoSettings::~CGUIDialogVideoSettings()
{ }

void CGUIDialogVideoSettings::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);

  CVideoSettings &videoSettings = CMediaSettings::Get().GetCurrentVideoSettings();

  const std::string &settingId = setting->GetId();
  if (settingId == SETTING_VIDEO_DEINTERLACEMODE)
    videoSettings.m_DeinterlaceMode = static_cast<EDEINTERLACEMODE>(static_cast<const CSettingInt*>(setting)->GetValue());
  else if (settingId == SETTING_VIDEO_INTERLACEMETHOD)
    videoSettings.m_InterlaceMethod = static_cast<EINTERLACEMETHOD>(static_cast<const CSettingInt*>(setting)->GetValue());
  else if (settingId == VIDEO_SETTINGS_DS_STATS)
  { 
    m_dsStats = static_cast<DS_STATS>(static_cast<const CSettingInt*>(setting)->GetValue());
    g_dsSettings.pRendererSettings->displayStats = (DS_STATS)m_dsStats;
  }
  else if (settingId == SETTING_VIDEO_SCALINGMETHOD)
#ifdef HAS_DS_PLAYER
  { 
    if (g_application.GetCurrentPlayer() == PCID_DSPLAYER)
  { 
    m_scalingMethod = static_cast<EDSSCALINGMETHOD>(static_cast<const CSettingInt*>(setting)->GetValue());
    videoSettings.SetDSPlayerScalingMethod((EDSSCALINGMETHOD)m_scalingMethod);
  }
  else 
#endif
    videoSettings.m_ScalingMethod = static_cast<ESCALINGMETHOD>(static_cast<const CSettingInt*>(setting)->GetValue());
#ifdef HAS_DS_PLAYER
  }
#endif

#ifdef HAS_VIDEO_PLAYBACK
  else if (settingId == SETTING_VIDEO_CROP)
    videoSettings.m_Crop = static_cast<const CSettingBool*>(setting)->GetValue();
  else if (settingId == SETTING_VIDEO_VIEW_MODE)
  {
    videoSettings.m_ViewMode = static_cast<const CSettingInt*>(setting)->GetValue();

    g_renderManager.SetViewMode(videoSettings.m_ViewMode);

    m_viewModeChanged = true;
    m_settingsManager->SetNumber(SETTING_VIDEO_ZOOM, videoSettings.m_CustomZoomAmount);
    m_settingsManager->SetNumber(SETTING_VIDEO_PIXEL_RATIO, videoSettings.m_CustomPixelRatio);
    m_settingsManager->SetNumber(SETTING_VIDEO_VERTICAL_SHIFT, videoSettings.m_CustomVerticalShift);
    m_settingsManager->SetBool(SETTING_VIDEO_NONLIN_STRETCH, videoSettings.m_CustomNonLinStretch);
    m_viewModeChanged = false;
  }
  else if (settingId == SETTING_VIDEO_ZOOM ||
           settingId == SETTING_VIDEO_VERTICAL_SHIFT ||
           settingId == SETTING_VIDEO_PIXEL_RATIO ||
           settingId == SETTING_VIDEO_NONLIN_STRETCH)
  {
    if (settingId == SETTING_VIDEO_ZOOM)
      videoSettings.m_CustomZoomAmount = static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue());
    else if (settingId == SETTING_VIDEO_VERTICAL_SHIFT)
      videoSettings.m_CustomVerticalShift = static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue());
    else if (settingId == SETTING_VIDEO_PIXEL_RATIO)
      videoSettings.m_CustomPixelRatio = static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue());
    else if (settingId == SETTING_VIDEO_NONLIN_STRETCH)
      videoSettings.m_CustomNonLinStretch = static_cast<const CSettingBool*>(setting)->GetValue();
    else if (settingId == SETTING_VIDEO_NONLIN_STRETCH)
      videoSettings.m_CustomNonLinStretch = static_cast<const CSettingBool*>(setting)->GetValue();

    if (!m_viewModeChanged)
    {
      // try changing the view mode to custom. If it already is set to custom
      // manually call the render manager
      if (m_settingsManager->GetInt(SETTING_VIDEO_VIEW_MODE) != ViewModeCustom)
        m_settingsManager->SetInt(SETTING_VIDEO_VIEW_MODE, ViewModeCustom);
      else
        g_renderManager.SetViewMode(videoSettings.m_ViewMode);
    }
  }
  else if (settingId == SETTING_VIDEO_POSTPROCESS)
    videoSettings.m_PostProcess = static_cast<const CSettingBool*>(setting)->GetValue();
  else if (settingId == SETTING_VIDEO_BRIGHTNESS)
    videoSettings.m_Brightness = static_cast<float>(static_cast<const CSettingInt*>(setting)->GetValue());
  else if (settingId == SETTING_VIDEO_CONTRAST)
    videoSettings.m_Contrast = static_cast<float>(static_cast<const CSettingInt*>(setting)->GetValue());
  else if (settingId == SETTING_VIDEO_GAMMA)
    videoSettings.m_Gamma = static_cast<float>(static_cast<const CSettingInt*>(setting)->GetValue());
  else if (settingId == SETTING_VIDEO_VDPAU_NOISE)
    videoSettings.m_NoiseReduction = static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue());
  else if (settingId == SETTING_VIDEO_VDPAU_SHARPNESS)
    videoSettings.m_Sharpness = static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue());
#endif
  else if (settingId == SETTING_VIDEO_STEREOSCOPICMODE)
    videoSettings.m_StereoMode = static_cast<const CSettingInt*>(setting)->GetValue();
  else if (settingId == SETTING_VIDEO_STEREOSCOPICINVERT)
    videoSettings.m_StereoInvert = static_cast<const CSettingBool*>(setting)->GetValue();
}

void CGUIDialogVideoSettings::OnSettingAction(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);

  const std::string &settingId = setting->GetId();
  if (settingId == SETTING_VIDEO_CALIBRATION)
  {
    // launch calibration window
    if (CProfilesManager::Get().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE  &&
        g_passwordManager.CheckSettingLevelLock(CSettings::Get().GetSetting("videoscreen.guicalibration")->GetLevel()))
      return;
    g_windowManager.ActivateWindow(WINDOW_SCREEN_CALIBRATION);
  }
  // TODO
  else if (settingId == SETTING_VIDEO_MAKE_DEFAULT)
    Save();

#ifdef HAS_DS_PLAYER
  if (g_application.GetCurrentPlayer() == PCID_DSPLAYER)
  {
    CGUIDialogSelect *pDlg = (CGUIDialogSelect *)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
      if (!pDlg)
        return;
    
    CStdString filterName;

    BeginEnumFilters(g_dsGraph->pFilterGraph, pEF, pBF)
    {
      if ((pBF == CGraphFilters::Get()->AudioRenderer.pBF && CGraphFilters::Get()->AudioRenderer.guid != CLSID_ReClock) || pBF == CGraphFilters::Get()->VideoRenderer.pBF)
        continue;

      Com::SmartQIPtr<ISpecifyPropertyPages> pProp = pBF;
      CAUUID pPages;
      if (pProp)
      {
        pProp->GetPages(&pPages);
        if (pPages.cElems > 0)
        {
          g_charsetConverter.wToUTF8(GetFilterName(pBF), filterName);
          pDlg->Add(filterName);
        }
        CoTaskMemFree(pPages.pElems);
      }
    }
    EndEnumFilters
    pDlg->SetHeading(55062);
    pDlg->DoModal();

    IBaseFilter *pBF = NULL;
    CStdStringW strNameW;

    g_charsetConverter.utf8ToW(pDlg->GetSelectedLabelText(), strNameW);
    if (SUCCEEDED(g_dsGraph->pFilterGraph->FindFilterByName(strNameW, &pBF)))
    {
      //Showing the property page for this filter
      m_pDSPropertyPage = new CDSPropertyPage(pBF);
      m_pDSPropertyPage->Initialize();
    }
  }
#endif
}

void CGUIDialogVideoSettings::Save()
{
  if (CProfilesManager::Get().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE &&
      !g_passwordManager.CheckSettingLevelLock(::SettingLevelExpert))
    return;

  // prompt user if they are sure
  if (CGUIDialogYesNo::ShowAndGetInput(12376, 750, 0, 12377))
  { // reset the settings
    CVideoDatabase db;
    if (!db.Open())
      return;
    db.EraseVideoSettings();
    db.Close();

    CMediaSettings::Get().GetDefaultVideoSettings() = CMediaSettings::Get().GetCurrentVideoSettings();
    CMediaSettings::Get().GetDefaultVideoSettings().m_SubtitleStream = -1;
    CMediaSettings::Get().GetDefaultVideoSettings().m_AudioStream = -1;
    CSettings::Get().Save();
  }
}

void CGUIDialogVideoSettings::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  CSettingCategory *category = AddCategory("audiosubtitlesettings", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }

  // get all necessary setting groups
  CSettingGroup *groupVideo = AddGroup(category);
  if (groupVideo == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }
  CSettingGroup *groupVideoPlayback = AddGroup(category);
  if (groupVideoPlayback == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }
  CSettingGroup *groupStereoscopic = AddGroup(category);
  if (groupStereoscopic == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }
  CSettingGroup *groupSaveAsDefault = AddGroup(category);
  if (groupSaveAsDefault == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }

  CSettingGroup *groupDSFilter = AddGroup(category);
  if (groupDSFilter == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogVideoSettings: unable to setup settings");
    return;
  }

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  CVideoSettings &videoSettings = CMediaSettings::Get().GetCurrentVideoSettings();

  StaticIntegerSettingOptions entries;
  if (g_renderManager.Supports(VS_DEINTERLACEMODE_OFF))
    entries.push_back(make_pair(16039, VS_DEINTERLACEMODE_OFF));
  if (g_renderManager.Supports(VS_DEINTERLACEMODE_AUTO))
    entries.push_back(make_pair(16040, VS_DEINTERLACEMODE_AUTO));
  if (g_renderManager.Supports(VS_DEINTERLACEMODE_FORCE))
    entries.push_back(make_pair(16041, VS_DEINTERLACEMODE_FORCE));
  if (!entries.empty())
    AddSpinner(groupVideo, SETTING_VIDEO_DEINTERLACEMODE, 16037, 0, static_cast<int>(videoSettings.m_DeinterlaceMode), entries);

  entries.clear();
  entries.push_back(make_pair(16019, VS_INTERLACEMETHOD_AUTO));
  entries.push_back(make_pair(20131, VS_INTERLACEMETHOD_RENDER_BLEND));
  entries.push_back(make_pair(20130, VS_INTERLACEMETHOD_RENDER_WEAVE_INVERTED));
  entries.push_back(make_pair(20129, VS_INTERLACEMETHOD_RENDER_WEAVE));
  entries.push_back(make_pair(16022, VS_INTERLACEMETHOD_RENDER_BOB_INVERTED));
  entries.push_back(make_pair(16021, VS_INTERLACEMETHOD_RENDER_BOB));
  entries.push_back(make_pair(16020, VS_INTERLACEMETHOD_DEINTERLACE));
  entries.push_back(make_pair(16036, VS_INTERLACEMETHOD_DEINTERLACE_HALF));
  entries.push_back(make_pair(16324, VS_INTERLACEMETHOD_SW_BLEND));
  entries.push_back(make_pair(16314, VS_INTERLACEMETHOD_INVERSE_TELECINE));
  entries.push_back(make_pair(16311, VS_INTERLACEMETHOD_VDPAU_TEMPORAL_SPATIAL));
  entries.push_back(make_pair(16310, VS_INTERLACEMETHOD_VDPAU_TEMPORAL));
  entries.push_back(make_pair(16325, VS_INTERLACEMETHOD_VDPAU_BOB));
  entries.push_back(make_pair(16318, VS_INTERLACEMETHOD_VDPAU_TEMPORAL_SPATIAL_HALF));
  entries.push_back(make_pair(16317, VS_INTERLACEMETHOD_VDPAU_TEMPORAL_HALF));
  entries.push_back(make_pair(16314, VS_INTERLACEMETHOD_VDPAU_INVERSE_TELECINE));
  entries.push_back(make_pair(16320, VS_INTERLACEMETHOD_DXVA_BOB));
  entries.push_back(make_pair(16321, VS_INTERLACEMETHOD_DXVA_BEST));
  entries.push_back(make_pair(16325, VS_INTERLACEMETHOD_AUTO_ION));
  entries.push_back(make_pair(16327, VS_INTERLACEMETHOD_VAAPI_BOB));
  entries.push_back(make_pair(16328, VS_INTERLACEMETHOD_VAAPI_MADI));
  entries.push_back(make_pair(16329, VS_INTERLACEMETHOD_VAAPI_MACI));
  entries.push_back(make_pair(16330, VS_INTERLACEMETHOD_MMAL_ADVANCED));
  entries.push_back(make_pair(16331, VS_INTERLACEMETHOD_MMAL_ADVANCED_HALF));
  entries.push_back(make_pair(16332, VS_INTERLACEMETHOD_MMAL_BOB));
  entries.push_back(make_pair(16333, VS_INTERLACEMETHOD_MMAL_BOB_HALF));

  /* remove unsupported methods */
  for (StaticIntegerSettingOptions::iterator it = entries.begin(); it != entries.end();)
  {
    if (g_renderManager.Supports((EINTERLACEMETHOD)it->second))
      it++;
    else
      it = entries.erase(it);
  }

  if (!entries.empty())
  {
    CSettingInt *settingInterlaceMethod = AddSpinner(groupVideo, SETTING_VIDEO_INTERLACEMETHOD, 16038, 0, static_cast<int>(videoSettings.m_InterlaceMethod), entries);

    CSettingDependency dependencyDeinterlaceModeOff(SettingDependencyTypeEnable, m_settingsManager);
    dependencyDeinterlaceModeOff.And()
      ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_VIDEO_DEINTERLACEMODE, "0", SettingDependencyOperatorEquals, true, m_settingsManager)));
    SettingDependencies depsDeinterlaceModeOff;
    depsDeinterlaceModeOff.push_back(dependencyDeinterlaceModeOff);
    settingInterlaceMethod->SetDependencies(depsDeinterlaceModeOff);
  }

#ifdef HAS_DS_PLAYER
  if (g_application.GetCurrentPlayer() == PCID_DVDPLAYER)
  {
#endif
    entries.clear();
    entries.push_back(make_pair(16301, VS_SCALINGMETHOD_NEAREST));
    entries.push_back(make_pair(16302, VS_SCALINGMETHOD_LINEAR));
    entries.push_back(make_pair(16303, VS_SCALINGMETHOD_CUBIC));
    entries.push_back(make_pair(16304, VS_SCALINGMETHOD_LANCZOS2));
    entries.push_back(make_pair(16323, VS_SCALINGMETHOD_SPLINE36_FAST));
    entries.push_back(make_pair(16315, VS_SCALINGMETHOD_LANCZOS3_FAST));
    entries.push_back(make_pair(16322, VS_SCALINGMETHOD_SPLINE36));
    entries.push_back(make_pair(16305, VS_SCALINGMETHOD_LANCZOS3));
    entries.push_back(make_pair(16306, VS_SCALINGMETHOD_SINC8));
    //  entries.push_back(make_pair(?????, VS_SCALINGMETHOD_NEDI));
    entries.push_back(make_pair(16307, VS_SCALINGMETHOD_BICUBIC_SOFTWARE));
    entries.push_back(make_pair(16308, VS_SCALINGMETHOD_LANCZOS_SOFTWARE));
    entries.push_back(make_pair(16309, VS_SCALINGMETHOD_SINC_SOFTWARE));
    entries.push_back(make_pair(13120, VS_SCALINGMETHOD_VDPAU_HARDWARE));
    entries.push_back(make_pair(16319, VS_SCALINGMETHOD_DXVA_HARDWARE));
    entries.push_back(make_pair(16316, VS_SCALINGMETHOD_AUTO));

    /* remove unsupported methods */
    for (StaticIntegerSettingOptions::iterator it = entries.begin(); it != entries.end();)
    {
      if (g_renderManager.Supports((ESCALINGMETHOD)it->second))
        it++;
      else
        it = entries.erase(it);
    }

    AddSpinner(groupVideo, SETTING_VIDEO_SCALINGMETHOD, 16300, 0, static_cast<int>(videoSettings.m_ScalingMethod), entries);

#ifdef HAS_DS_PLAYER
  }
  else if (g_application.GetCurrentPlayer() == PCID_DSPLAYER)
  {
    entries.clear();
    entries.push_back(make_pair(55005, DS_SCALINGMETHOD_NEAREST_NEIGHBOR));
    entries.push_back(make_pair(55006, DS_SCALINGMETHOD_BILINEAR ));
    entries.push_back(make_pair(55007, DS_SCALINGMETHOD_BILINEAR_2));
    entries.push_back(make_pair(55008, DS_SCALINGMETHOD_BILINEAR_2_60));
    entries.push_back(make_pair(55009, DS_SCALINGMETHOD_BILINEAR_2_75));
    entries.push_back(make_pair(55010, DS_SCALINGMETHOD_BILINEAR_2_100));

    m_scalingMethod = videoSettings.GetDSPlayerScalingMethod();
    AddSpinner(groupVideo, SETTING_VIDEO_SCALINGMETHOD, 16300, 0, static_cast<int>(m_scalingMethod), entries);

    entries.clear();
    entries.push_back(make_pair(55011, DS_STATS_NONE));
    entries.push_back(make_pair(55012, DS_STATS_1));
    entries.push_back(make_pair(55013, DS_STATS_2));
    entries.push_back(make_pair(55014, DS_STATS_3));
    AddSpinner(groupVideo, VIDEO_SETTINGS_DS_STATS, 55015, 0, static_cast<int>(m_dsStats), entries);
  }

  if (g_application.GetCurrentPlayer() == PCID_DSPLAYER)
  {
    AddButton(groupVideo, VIDEO_SETTINGS_DS_FILTERS, 55062, 0);
  }
#endif

#ifdef HAS_VIDEO_PLAYBACK
  if (g_renderManager.Supports(RENDERFEATURE_CROP))
    AddToggle(groupVideo, SETTING_VIDEO_CROP, 644, 0, videoSettings.m_Crop);

  if (g_renderManager.Supports(RENDERFEATURE_STRETCH) || g_renderManager.Supports(RENDERFEATURE_PIXEL_RATIO))
  {
    entries.clear();
    for (int i = 0; i < 7; ++i)
      entries.push_back(make_pair(630 + i, i));
    AddSpinner(groupVideo, SETTING_VIDEO_VIEW_MODE, 629, 0, videoSettings.m_ViewMode, entries);
  }
  if (g_renderManager.Supports(RENDERFEATURE_ZOOM))
    AddSlider(groupVideo, SETTING_VIDEO_ZOOM, 216, 0, videoSettings.m_CustomZoomAmount, "%2.2f", 0.5f, 0.01f, 2.0f, 216, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_VERTICAL_SHIFT))
    AddSlider(groupVideo, SETTING_VIDEO_VERTICAL_SHIFT, 225, 0, videoSettings.m_CustomVerticalShift, "%2.2f", -2.0f, 0.01f, 2.0f, 225, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_PIXEL_RATIO))
    AddSlider(groupVideo, SETTING_VIDEO_PIXEL_RATIO, 217, 0, videoSettings.m_CustomPixelRatio, "%2.2f", 0.5f, 0.01f, 2.0f, 217, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_POSTPROCESS))
    AddToggle(groupVideo, SETTING_VIDEO_POSTPROCESS, 16400, 0, videoSettings.m_PostProcess);
  if (g_renderManager.Supports(RENDERFEATURE_BRIGHTNESS))
    AddPercentageSlider(groupVideoPlayback, SETTING_VIDEO_BRIGHTNESS, 464, 0, static_cast<int>(videoSettings.m_Brightness), 14047, 1, 464, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_CONTRAST))
    AddPercentageSlider(groupVideoPlayback, SETTING_VIDEO_CONTRAST, 465, 0, static_cast<int>(videoSettings.m_Contrast), 14047, 1, 465, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_GAMMA))
    AddPercentageSlider(groupVideoPlayback, SETTING_VIDEO_GAMMA, 466, 0, static_cast<int>(videoSettings.m_Gamma), 14047, 1, 466, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_NOISE))
    AddSlider(groupVideoPlayback, SETTING_VIDEO_VDPAU_NOISE, 16312, 0, videoSettings.m_NoiseReduction, "%2.2f", 0.0f, 0.01f, 1.0f, 16312, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_SHARPNESS))
    AddSlider(groupVideoPlayback, SETTING_VIDEO_VDPAU_SHARPNESS, 16313, 0, videoSettings.m_Sharpness, "%2.2f", -1.0f, 0.02f, 1.0f, 16313, usePopup);
  if (g_renderManager.Supports(RENDERFEATURE_NONLINSTRETCH))
    AddToggle(groupVideoPlayback, SETTING_VIDEO_NONLIN_STRETCH, 659, 0, videoSettings.m_CustomNonLinStretch);
#endif

  // stereoscopic settings
  entries.clear();
  entries.push_back(make_pair(16316, RENDER_STEREO_MODE_OFF));
  entries.push_back(make_pair(36503, RENDER_STEREO_MODE_SPLIT_HORIZONTAL));
  entries.push_back(make_pair(36504, RENDER_STEREO_MODE_SPLIT_VERTICAL));
  AddSpinner(groupStereoscopic, SETTING_VIDEO_STEREOSCOPICMODE  , 36535, 0, videoSettings.m_StereoMode, entries);
  AddToggle(groupStereoscopic, SETTING_VIDEO_STEREOSCOPICINVERT, 36536, 0, videoSettings.m_StereoInvert);

  // general settings
  AddButton(groupSaveAsDefault, SETTING_VIDEO_MAKE_DEFAULT, 12376, 0);
  AddButton(groupSaveAsDefault, SETTING_VIDEO_CALIBRATION, 214, 0);
}
