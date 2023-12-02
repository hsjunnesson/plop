##########################################################################
#  This program is free software: you can redistribute it and/or modify  #
#  it under the terms of the GNU General Public License as published by  #
#  the Free Software Foundation, either version 3 of the License, or     #
#  (at your option) any later version.                                   #
#                                                                        #
#  This program is distributed in the hope that it will be useful,       #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#  GNU General Public License for more details.                          #
#                                                                        #
#  You should have received a copy of the GNU General Public License     #
#  along with this program.  If not, see <http://www.gnu.org/licenses/>. #
#                                                                        #
##########################################################################

##########################################################################
#   NOTE: Make sure to set the WWISE_SDK environment variable            #
#         to the root of the Wwise SDK.                                  #
#                                                                        #
# Once done, this module will define:                                    #
#                                                                        #
#  Wwise_FOUND         - system has Wwise SDK                            #
#  Wwise_INCLUDE_DIR   - where to find the AK directory containing       #
#                        AkPlatforms.h, AkWwiseSDKVersion.h, etc.        #
#  Wwise_LIBRARIES     - Wwise SDK static libraries                      #
#                                                                        #
#  Author: Jordi Ortolá Ankum                                            #
##########################################################################

include (FindPackageHandleStandardArgs)

if (NOT EXISTS ${WWISE_SDK})
  message (FATAL_ERROR "'${WWISE_SDK}' does not exist")
endif()

find_path(Wwise_INCLUDE_DIR
  NAMES "AK/AkWwiseSDKVersion.h"
  PATHS "${WWISE_SDK}/include"
  NO_CMAKE_FIND_ROOT_PATH
)

if (CMAKE_BUILD_TYPE MATCHES Release)
  set(SDK_BUILD_CONFIG Release)
else()
  set(SDK_BUILD_CONFIG Debug)
endif()

set (ABI_DIR x64_vc170)

set (libNames
  AkAudioInputSource AkCompressorFX AkDelayFX AkExpanderFX AkFlangerFX
  AkGainFX AkGuitarDistortionFX AkHarmonizerFX AkMatrixReverbFX AkMemoryMgr AkMeterFX AkMusicEngine
  AkParametricEQFX AkPeakLimiterFX AkPitchShifterFX AkRecorderFX AkRoomVerbFX AkSilenceSource
  AkSineSource AkSoundEngine AkStereoDelayFX
  AkStreamMgr AkSynthOneSource AkTimeStretchFX AkToneSource AkTremoloFX AkVorbisDecoder
  CommunicationCentral
)

if (CMAKE_BUILD_TYPE MATCHES Release)
  list(FILTER libNames EXCLUDE REGEX "CommunicationCentral")
endif()

set(SDK_LIB_DIR ${WWISE_SDK}/${ABI_DIR}/${SDK_BUILD_CONFIG}/lib)

foreach(name ${libNames})
  add_library(${name} STATIC IMPORTED)
  set_property(TARGET ${name} PROPERTY IMPORTED_LOCATION "${SDK_LIB_DIR}/${name}.lib")
endforeach()

set(Wwise_LIBRARIES
  # Base libs, contain circular dependencies, order is important!
  AkMemoryMgr
  CommunicationCentral
  AkStreamMgr # <- dependency on AkMemoryMgr
  AkSoundEngine # <- dependency on AkStreamMgr
  AkMusicEngine # <- dependency on AkSoundEngine
  AkSoundEngine # <- dependency on CommunicationCentral

  # FX
  AkCompressorFX AkDelayFX AkExpanderFX AkFlangerFX
  AkGainFX AkGuitarDistortionFX AkHarmonizerFX AkMatrixReverbFX AkMeterFX
  AkParametricEQFX AkPeakLimiterFX AkPitchShifterFX AkRecorderFX AkRoomVerbFX
  AkStereoDelayFX AkTimeStretchFX AkTremoloFX 

  # Sources
  AkAudioInputSource AkSilenceSource AkSineSource
  AkSynthOneSource AkToneSource

  # Misc
  AkVorbisDecoder
)

if (CMAKE_BUILD_TYPE MATCHES Release)
  list(FILTER Wwise_LIBRARIES EXCLUDE REGEX "CommunicationCentral")
endif()

# Handle QUIETLY and REQUIRED args and set Wwise_FOUND to true if all libs found
find_package_handle_standard_args(Wwise
    REQUIRED_VARS
      SDK_LIB_DIR
      Wwise_INCLUDE_DIR
    FAIL_MESSAGE DEFAULT_MSG
)

mark_as_advanced(SDK_LIB_DIR SDK_BUILD_CONFIG WWISE_SDK
  AkMemoryMgr AkMeterFX AkRecorderFX AkPeakLimiterFX AkHarmonizerFX AkStereoDelayFX
  AkMusicEngine AkTimeStretchFX McDSPLimiterFX AkConvolutionReverbFX AkCompressorFX
  AkFlangerFX AkPitchShifterFX AkSoundSeedImpactFX McDSPFutzBoxFX AkRoomVerbFX
  AkSineSource AkSilenceSource AkSynthOne AkParametricEQFX AkVorbisDecoder
  AkToneSource AkExpanderFX AkGuitarDistortionFX AkSoundSeedWoosh AkTremoloFX
  AkMatrixReverbFX AkStreamMgr AkDelayFX AkAudioInputSource AkSoundSeedWind AkGainFX
  AuroHeadphoneFX AuroPannerMixer CrankcaseAudioREVModelPlayerFX AkSoundEngine
  CommunicationCentral
)
