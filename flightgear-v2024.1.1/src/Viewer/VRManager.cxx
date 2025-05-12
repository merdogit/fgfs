// Copyright (C) 2021  James Hogan
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "VRManager.hxx"
#include "WindowBuilder.hxx"
#include "renderer.hxx"

#include <osgXR/Settings>

#include <simgear/scene/util/RenderConstants.hxx>
#include <simgear/scene/viewer/CompositorPass.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

namespace flightgear
{

// Unfortunately, this can't be scoped inside VRManager::instance().
// If its initialisation completes after main() calls atexit(fgExitCleanup),
// then its destruction should take place before fgExitCleanup() is called.
static osg::ref_ptr<VRManager> managerInstance;

VRManager::VRManager() :
    _reloadCompositorCallback(new ReloadCompositorCallback(this)),
    _propXrLayersValidation("/sim/vr/openxr/layers/validation"),
    _propXrExtensionsDepthInfo("/sim/vr/openxr/extensions/depth-info"),
    _propXrExtensionsVisibilityMask("/sim/vr/openxr/extensions/visibility-mask"),
    _propXrRuntimeName("/sim/vr/openxr/runtime/name"),
    _propXrSystemName("/sim/vr/openxr/system/name"),
    _propStateString("/sim/vr/state-string"),
    _propPresent("/sim/vr/present"),
    _propRunning("/sim/vr/running"),
    _propEnabled("/sim/vr/enabled"),
    _propDepthInfo("/sim/vr/depth-info"),
    _propVisibilityMask("/sim/vr/visibility-mask"),
    _propValidationLayer("/sim/vr/validation-layer"),
    _propMode("/sim/vr/mode"),
    _propSwapchainMode("/sim/vr/swapchain-mode"),
    _propMirrorEnabled("/sim/vr/mirror-enabled"),
    _propMirrorMode("/sim/vr/mirror-mode"),
    _listenerEnabled(this, &osgXR::Manager::setEnabled),
    _listenerDepthInfo(this, &VRManager::setDepthInfo),
    _listenerVisibilityMask(this, &VRManager::setVisibilityMask),
    _listenerValidationLayer(this, &VRManager::setValidationLayer),
    _listenerMode(this, &VRManager::setVRMode),
    _listenerSwapchainMode(this, &VRManager::setSwapchainMode),
    _listenerMirrorMode(this, &VRManager::setMirrorMode)
{
    uint32_t fgVersion = (FLIGHTGEAR_MAJOR_VERSION << 16 |
                          FLIGHTGEAR_MINOR_VERSION << 8 |
                          FLIGHTGEAR_PATCH_VERSION);
    _settings->setApp("FlightGear", fgVersion);
    _settings->preferEnvBlendMode(osgXR::Settings::BLEND_MODE_OPAQUE);

    // Inform osgXR what node masks to use
    setVisibilityMaskNodeMasks(simgear::NodeMask::LEFT_BIT,
                               simgear::NodeMask::RIGHT_BIT);

    // Hook into viewer, but don't enable VR just yet
    osgViewer::View *view = globals->get_renderer()->getView();
    if (view) {
        setViewer(globals->get_renderer()->getViewerBase());
        view->apply(this);
    }

    syncReadOnlyProperties();

    _propEnabled.node(true)->addChangeListener(&_listenerEnabled, true);
    _propDepthInfo.node(true)->addChangeListener(&_listenerDepthInfo, true);
    _propVisibilityMask.node(true)->addChangeListener(&_listenerVisibilityMask, true);
    _propValidationLayer.node(true)->addChangeListener(&_listenerValidationLayer, true);
    _propMode.node(true)->addChangeListener(&_listenerMode, true);
    _propSwapchainMode.node(true)->addChangeListener(&_listenerSwapchainMode, true);
    _propMirrorMode.node(true)->addChangeListener(&_listenerMirrorMode, true);

    // No need for a change listener, but it should still be resolvable
    _propMirrorEnabled.node(true);
}

VRManager *VRManager::instance()
{
    static bool initialised = false;
    if (!initialised) {
        managerInstance = new VRManager;
        initialised = true;
    }
    return managerInstance;
}

void VRManager::syncProperties()
{
    // If the state has changed, properties may need synchronising
    if (checkAndResetStateChanged()) {
        syncReadOnlyProperties();
        syncSettingProperties();
    }
}

void VRManager::syncReadOnlyProperties()
{
    _propXrLayersValidation = hasValidationLayer();
    _propXrExtensionsDepthInfo = hasDepthInfoExtension();
    _propXrExtensionsVisibilityMask = hasVisibilityMaskExtension();
    _propXrRuntimeName = getRuntimeName();
    _propXrSystemName = getSystemName();

    _propStateString = getStateString();
    _propPresent = getPresent();
    _propRunning = isRunning();
}

void VRManager::syncSettingProperties()
{
    bool enabled = getEnabled();
    if (_propEnabled != enabled)
        _propEnabled = enabled;
}

bool VRManager::getUseMirror() const
{
    return _propMirrorEnabled && isRunning();
}

void VRManager::setValidationLayer(bool validationLayer)
{
    _settings->setValidationLayer(validationLayer);
    syncSettings();
}

void VRManager::setDepthInfo(bool depthInfo)
{
    _settings->setDepthInfo(depthInfo);
    syncSettings();
}

void VRManager::setVisibilityMask(bool visibilityMask)
{
    _settings->setVisibilityMask(visibilityMask);
    syncSettings();
}

void VRManager::setVRMode(const std::string& mode)
{
    osgXR::Settings::VRMode vrMode = osgXR::Settings::VRMODE_AUTOMATIC;

    if (mode == "AUTOMATIC") {
        vrMode = osgXR::Settings::VRMODE_AUTOMATIC;
    } else if (mode == "SLAVE_CAMERAS") {
        vrMode = osgXR::Settings::VRMODE_SLAVE_CAMERAS;
    } else if (mode == "SCENE_VIEW") {
        vrMode = osgXR::Settings::VRMODE_SCENE_VIEW;
    }

    _settings->setVRMode(vrMode);
    syncSettings();
}

void VRManager::setSwapchainMode(const std::string& mode)
{
    osgXR::Settings::SwapchainMode swapchainMode = osgXR::Settings::SWAPCHAIN_AUTOMATIC;

    if (mode == "AUTOMATIC") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_AUTOMATIC;
    } else if (mode == "MULTIPLE") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_MULTIPLE;
    } else if (mode == "SINGLE") {
        swapchainMode = osgXR::Settings::SWAPCHAIN_SINGLE;
    }

    _settings->setSwapchainMode(swapchainMode);
    syncSettings();
}

void VRManager::setMirrorMode(const std::string& mode)
{
    osgXR::MirrorSettings::MirrorMode mirrorMode = osgXR::MirrorSettings::MIRROR_AUTOMATIC;
    int viewIndex = -1;

    if (mode == "AUTOMATIC") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_AUTOMATIC;
    } else if (mode == "NONE") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_NONE;
    } else if (mode == "LEFT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_SINGLE;
        viewIndex = 0;
    } else if (mode == "RIGHT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_SINGLE;
        viewIndex = 1;
    } else if (mode == "LEFT_RIGHT") {
        mirrorMode = osgXR::MirrorSettings::MIRROR_LEFT_RIGHT;
    }

    _settings->getMirrorSettings().setMirror(mirrorMode, viewIndex);
}

void VRManager::update()
{
    osgXR::Manager::update();
    syncProperties();
}

void VRManager::doCreateView(osgXR::View *xrView)
{
    // Restarted in osgXR::Manager::update()
    _viewer->stopThreading();

    // Construct a property tree for the camera
    SGPropertyNode_ptr camNode = new SGPropertyNode;
    WindowBuilder *windowBuilder = WindowBuilder::getWindowBuilder();
    setValue(camNode->getNode("window/name", true),
             windowBuilder->getDefaultWindowName());

    // Build a camera
    CameraGroup *cgroup = CameraGroup::getDefault();
    CameraInfo *info = cgroup->buildCamera(camNode);

    // Notify osgXR about the new compositor's scene slave cameras
    if (info) {
        _camInfos[xrView] = info;
        _xrViews[info] = xrView;
        info->reloadCompositorCallback = _reloadCompositorCallback;

        postReloadCompositor(cgroup, info);
    }
}

void VRManager::doDestroyView(osgXR::View *xrView)
{
    // Restarted in osgXR::Manager::update()
    _viewer->stopThreading();

    CameraGroup *cgroup = CameraGroup::getDefault();
    auto it = _camInfos.find(xrView);
    if (it != _camInfos.end()) {
        osg::ref_ptr<CameraInfo> info = (*it).second;
        _camInfos.erase(it);

        auto it2 = _xrViews.find(info.get());
        if (it2 != _xrViews.end())
            _xrViews.erase(it2);

        cgroup->removeCamera(info.get());
    }
}

void VRManager::onRunning()
{
    // Reload compositors to trigger switch to mirror of VR
    CameraGroup *cgroup = CameraGroup::getDefault();
    reloadCompositors(cgroup);
}

void VRManager::onStopped()
{
    // As long as we're not in the process of destroying FlightGear, reload
    // compositors to trigger switch away from mirror of VR
    if (!isDestroying())
    {
        CameraGroup *cgroup = CameraGroup::getDefault();
        reloadCompositors(cgroup);
    }
}

void VRManager::preReloadCompositor(CameraGroup *cgroup, CameraInfo *info)
{
    osgXR::View *xrView = _xrViews[info];

    auto& passes = info->compositor->getPassList();
    for (auto& pass: passes)
        if (pass->type == "scene")
            xrView->removeSlave(pass->camera);
}

void VRManager::postReloadCompositor(CameraGroup *cgroup, CameraInfo *info)
{
    osgXR::View *xrView = _xrViews[info];

    auto& passes = info->compositor->getPassList();
    for (auto& pass: passes)
        if (pass->type == "scene")
            xrView->addSlave(pass->camera);
}

}
