// fg_os_osgviewer.cxx -- common functions for fg_os interface
// implemented as an osgViewer
//
// Copyright (C) 2007  Tim Moore timoore@redhat.com
// Copyright (C) 2007 Mathias Froehlich
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

#ifdef __linux__
#include <sched.h>
#endif

#include <config.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/scene/util/OsgIoCapture.hxx>

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/Group>
#include <osg/Matrixd>
#include <osg/Notify>
#include <osg/Version>
#include <osg/View>
#include <osg/Viewport>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include "CameraGroup.hxx"
#include "FGEventHandler.hxx"
#include "VRManager.hxx"
#include "WindowBuilder.hxx"
#include "WindowSystemAdapter.hxx"
#include "renderer.hxx"
#include <Main/fg_os.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/sentryIntegration.hxx>
#include <Main/util.hxx>
#include <Scenery/scenery.hxx>

#if defined(SG_MAC)
#include <GUI/CocoaHelpers.h>
#endif

#if defined(SG_WINDOWS)
#include <process.h> // _getpid()
#endif

// Static linking of OSG needs special macros
#ifdef OSG_LIBRARY_STATIC
#include <osgDB/Registry>
USE_GRAPHICSWINDOW();
// Image formats
USE_OSGPLUGIN(bmp);
USE_OSGPLUGIN(dds);
USE_OSGPLUGIN(hdr);
USE_OSGPLUGIN(pic);
USE_OSGPLUGIN(pnm);
USE_OSGPLUGIN(rgb);
USE_OSGPLUGIN(tga);
#ifdef OSG_JPEG_ENABLED
USE_OSGPLUGIN(jpeg);
#endif
#ifdef OSG_PNG_ENABLED
USE_OSGPLUGIN(png);
#endif
#ifdef OSG_TIFF_ENABLED
USE_OSGPLUGIN(tiff);
#endif
// Model formats
USE_OSGPLUGIN(3ds);
USE_OSGPLUGIN(ac);
USE_OSGPLUGIN(ive);
USE_OSGPLUGIN(osg);
USE_OSGPLUGIN(txf);
#endif

// fg_os implementation using OpenSceneGraph's osgViewer::Viewer class
// to create the graphics window and run the event/update/render loop.

//
// fg_os implementation
//

using namespace std;
using namespace flightgear;
using namespace osg;

osg::ref_ptr<osgViewer::Viewer> viewer;

static void setStereoMode(const char* mode)
{
    DisplaySettings::StereoMode stereoMode = DisplaySettings::QUAD_BUFFER;
    bool stereoOn = true;

    if (strcmp(mode, "QUAD_BUFFER") == 0) {
        stereoMode = DisplaySettings::QUAD_BUFFER;
    } else if (strcmp(mode, "ANAGLYPHIC") == 0) {
        stereoMode = DisplaySettings::ANAGLYPHIC;
    } else if (strcmp(mode, "HORIZONTAL_SPLIT") == 0) {
        stereoMode = DisplaySettings::HORIZONTAL_SPLIT;
    } else if (strcmp(mode, "VERTICAL_SPLIT") == 0) {
        stereoMode = DisplaySettings::VERTICAL_SPLIT;
    } else if (strcmp(mode, "LEFT_EYE") == 0) {
        stereoMode = DisplaySettings::LEFT_EYE;
    } else if (strcmp(mode, "RIGHT_EYE") == 0) {
        stereoMode = DisplaySettings::RIGHT_EYE;
    } else if (strcmp(mode, "HORIZONTAL_INTERLACE") == 0) {
        stereoMode = DisplaySettings::HORIZONTAL_INTERLACE;
    } else if (strcmp(mode, "VERTICAL_INTERLACE") == 0) {
        stereoMode = DisplaySettings::VERTICAL_INTERLACE;
    } else if (strcmp(mode, "CHECKERBOARD") == 0) {
        stereoMode = DisplaySettings::CHECKERBOARD;
    } else {
        stereoOn = false;
    }
    DisplaySettings::instance()->setStereo(stereoOn);
    DisplaySettings::instance()->setStereoMode(stereoMode);
}

static const char* getStereoMode()
{
    DisplaySettings::StereoMode stereoMode = DisplaySettings::instance()->getStereoMode();
    bool stereoOn = DisplaySettings::instance()->getStereo();
    if (!stereoOn) return "OFF";
    if (stereoMode == DisplaySettings::QUAD_BUFFER) {
        return "QUAD_BUFFER";
    } else if (stereoMode == DisplaySettings::ANAGLYPHIC) {
        return "ANAGLYPHIC";
    } else if (stereoMode == DisplaySettings::HORIZONTAL_SPLIT) {
        return "HORIZONTAL_SPLIT";
    } else if (stereoMode == DisplaySettings::VERTICAL_SPLIT) {
        return "VERTICAL_SPLIT";
    } else if (stereoMode == DisplaySettings::LEFT_EYE) {
        return "LEFT_EYE";
    } else if (stereoMode == DisplaySettings::RIGHT_EYE) {
        return "RIGHT_EYE";
    } else if (stereoMode == DisplaySettings::HORIZONTAL_INTERLACE) {
        return "HORIZONTAL_INTERLACE";
    } else if (stereoMode == DisplaySettings::VERTICAL_INTERLACE) {
        return "VERTICAL_INTERLACE";
    } else if (stereoMode == DisplaySettings::CHECKERBOARD) {
        return "CHECKERBOARD";
    }
    return "OFF";
}

class NotifyLevelListener : public SGPropertyChangeListener
{
public:
    void valueChanged(SGPropertyNode* node)
    {
        osg::NotifySeverity severity = osg::getNotifyLevel();
        string val = simgear::strutils::lowercase(node->getStringValue());

        if (val == "fatal") {
            severity = osg::FATAL;
        } else if (val == "warn") {
            severity = osg::WARN;
        } else if (val == "notice") {
            severity = osg::NOTICE;
        } else if (val == "info") {
            severity = osg::INFO;
        } else if ((val == "debug") || (val == "debug-info")) {
            severity = osg::DEBUG_INFO;
        }

        osg::setNotifyLevel(severity);
    }
};

void updateOSGNotifyLevel()
{
}

void fgOSOpenWindow(bool stencil)
{
    osg::setNotifyHandler(new SGNotifyHandler);

    auto composite_viewer = dynamic_cast<osgViewer::CompositeViewer*>(
        globals->get_renderer()->getViewerBase());
    if (0) {
    } else if (composite_viewer) {
        /* We are using CompositeViewer. */
        SG_LOG(SG_VIEW, SG_DEBUG, "Using CompositeViewer");
        osgViewer::ViewerBase* viewer = globals->get_renderer()->getViewerBase();
        SG_LOG(SG_VIEW, SG_DEBUG, "Creating osgViewer::View");
        osgViewer::View* view = new osgViewer::View;
        view->setFrameStamp(composite_viewer->getFrameStamp());
        globals->get_renderer()->setView(view);
        assert(globals->get_renderer()->getView() == view);
        view->setDatabasePager(FGScenery::getPagerSingleton());

        // https://www.mail-archive.com/osg-users@lists.openscenegraph.org/msg29820.html
        view->getDatabasePager()->setUnrefImageDataAfterApplyPolicy(true, false);
        // osg::GraphicsContext::createNewContextID();

        //viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

        std::string mode;
        mode = fgGetString("/sim/rendering/multithreading-mode", "SingleThreaded");
        SG_LOG(SG_VIEW, SG_INFO, "mode=" << mode);
        if (mode == "AutomaticSelection")
            viewer->setThreadingModel(osgViewer::Viewer::AutomaticSelection);
        else if (mode == "CullDrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::CullDrawThreadPerContext);
        else if (mode == "DrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::DrawThreadPerContext);
        else if (mode == "CullThreadPerCameraDrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::CullThreadPerCameraDrawThreadPerContext);
        else
            viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

        WindowBuilder::initWindowBuilder(stencil);
        CameraGroup::buildDefaultGroup(view);

        FGEventHandler* manipulator = globals->get_renderer()->getEventHandler();
        WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();
        if (wsa->windows.size() != 1) {
            manipulator->setResizable(false);
        }
        view->getCamera()->setProjectionResizePolicy(osg::Camera::FIXED);
        view->addEventHandler(manipulator);
        // Let FG handle the escape key with a confirmation
        viewer->setKeyEventSetsDone(0);
        // The viewer won't start without some root.
        view->setSceneData(new osg::Group);
        globals->get_renderer()->setView(view);
    } else {
        /* Not using CompositeViewer. */
        SG_LOG(SG_VIEW, SG_DEBUG, "Not CompositeViewer.");
        SG_LOG(SG_VIEW, SG_DEBUG, "Creating osgViewer::Viewer");
        viewer = new osgViewer::Viewer;
        viewer->setDatabasePager(FGScenery::getPagerSingleton());

        std::string mode;
        mode = fgGetString("/sim/rendering/multithreading-mode", "SingleThreaded");
        flightgear::addSentryTag("osg-thread-mode", mode);

        if (mode == "AutomaticSelection")
            viewer->setThreadingModel(osgViewer::Viewer::AutomaticSelection);
        else if (mode == "CullDrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::CullDrawThreadPerContext);
        else if (mode == "DrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::DrawThreadPerContext);
        else if (mode == "CullThreadPerCameraDrawThreadPerContext")
            viewer->setThreadingModel(osgViewer::Viewer::CullThreadPerCameraDrawThreadPerContext);
        else
            viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
        WindowBuilder::initWindowBuilder(stencil);
        CameraGroup::buildDefaultGroup(viewer.get());

        FGEventHandler* manipulator = globals->get_renderer()->getEventHandler();
        WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();
        if (wsa->windows.size() != 1) {
            manipulator->setResizable(false);
        }
        viewer->getCamera()->setProjectionResizePolicy(osg::Camera::FIXED);
        viewer->addEventHandler(manipulator);
        // Let FG handle the escape key with a confirmation
        viewer->setKeyEventSetsDone(0);
        // The viewer won't start without some root.
        viewer->setSceneData(new osg::Group);
        globals->get_renderer()->setView(viewer.get());
    }
}
SGPropertyNode *simHost = 0, *simFrameCount, *simTotalHostTime, *simFrameResetCount, *frameWait;

// Getter/Setter to work around lack of unsigned int properties.  Note that we have a minimum of 1 DB thread as otherwise
// nothing will be loaded. We also force the number of HTTP threads to 0, as we don't use them.
inline int getNumDatabaseThreads() { return DisplaySettings::instance()->getNumOfDatabaseThreadsHint(); }
inline void setNumDatabaseThreads(int threads) {  DisplaySettings::instance()->setNumOfDatabaseThreadsHint(max(threads, 1)); DisplaySettings::instance()->setNumOfHttpDatabaseThreadsHint(0);  }

void fgOSResetProperties()
{
    SGPropertyNode* osgLevel = fgGetNode("/sim/rendering/osg-notify-level", true);
    simTotalHostTime = fgGetNode("/sim/rendering/sim-host-total-ms", true);
    simHost = fgGetNode("/sim/rendering/sim-host-avg-ms", true);
    simFrameCount = fgGetNode("/sim/rendering/sim-frame-count", true);
    simFrameResetCount = fgGetNode("/sim/rendering/sim-frame-count-reset", true);
    frameWait = fgGetNode("/sim/time/frame-wait-ms", true);
    simFrameResetCount->setBoolValue(false);
    NotifyLevelListener* l = new NotifyLevelListener;
    globals->addListenerToCleanup(l);
    osgLevel->addChangeListener(l, true);

    osg::Camera* guiCamera = getGUICamera(CameraGroup::getDefault());
    if (guiCamera) {
        Viewport* guiViewport = guiCamera->getViewport();
        fgSetInt("/sim/startup/xsize", guiViewport->width());
        fgSetInt("/sim/startup/ysize", guiViewport->height());
    }

    DisplaySettings* displaySettings = DisplaySettings::instance();
    fgTie("/sim/rendering/osg-displaysettings/split-stereo-autoadjust-aspect-ratio", displaySettings, &DisplaySettings::getSplitStereoAutoAdjustAspectRatio, &DisplaySettings::setSplitStereoAutoAdjustAspectRatio);
    fgTie("/sim/rendering/osg-displaysettings/eye-separation", displaySettings, &DisplaySettings::getEyeSeparation, &DisplaySettings::setEyeSeparation);
    fgTie("/sim/rendering/osg-displaysettings/screen-distance", displaySettings, &DisplaySettings::getScreenDistance, &DisplaySettings::setScreenDistance);
    fgTie("/sim/rendering/osg-displaysettings/screen-width", displaySettings, &DisplaySettings::getScreenWidth, &DisplaySettings::setScreenWidth);
    fgTie("/sim/rendering/osg-displaysettings/screen-height", displaySettings, &DisplaySettings::getScreenHeight, &DisplaySettings::setScreenHeight);
    fgTie("/sim/rendering/osg-displaysettings/stereo-mode", getStereoMode, setStereoMode);
    fgTie("/sim/rendering/osg-displaysettings/double-buffer", displaySettings, &DisplaySettings::getDoubleBuffer, &DisplaySettings::setDoubleBuffer);
    fgTie("/sim/rendering/osg-displaysettings/depth-buffer", displaySettings, &DisplaySettings::getDepthBuffer, &DisplaySettings::setDepthBuffer);
    fgTie("/sim/rendering/osg-displaysettings/rgb", displaySettings, &DisplaySettings::getRGB, &DisplaySettings::setRGB);

    fgTie("/sim/rendering/database-pager/threads", &getNumDatabaseThreads, &setNumDatabaseThreads);

#ifdef ENABLE_OSGXR
    fgSetBool("/sim/vr/built", true);
#else
    fgSetBool("/sim/vr/built", false);
#endif
}


static int status = 0;

void fgOSExit(int code)
{
    FGRenderer* renderer = globals->get_renderer();
    renderer->getViewerBase()->setDone(true);
    renderer->getView()->getDatabasePager()->cancel();
    status = code;

    // otherwise we crash if OSG does logging during static destruction, eg
    // GraphicsWindowX11, since OSG statics may have been created before the
    // sglog static, despite our best efforts in boostrap.cxx
    osg::setNotifyHandler(new osg::StandardNotifyHandler);
}
SGTimeStamp _lastUpdate;

static void ShowAffinities()
{
#ifdef __linux__
    char command[1024];
    snprintf(command, sizeof(command), "for i in `ls /proc/%i/task/`; do taskset -p $i; done 1>&2", getpid());
    SG_LOG(SG_VIEW, SG_ALERT, "Running: " << command);
    system(command);
#endif
}

#ifdef __linux__
static std::ostream& operator<<(std::ostream& out, const cpu_set_t& mask)
{
    out << "0x";
    unsigned char* mask2 = (unsigned char*)&mask;
    for (unsigned i = 0; i < sizeof(mask); ++i) {
        char buffer[8];
        snprintf(buffer, sizeof(buffer), "%02x", (unsigned)mask2[i]);
        out << buffer;
    }
    return out;
}
#endif

/* Listen to /sim/affinity-control and, on Linux only, responds to
value='clear' and 'revert':

    'clear'
        Stores current affinities for all thread then resets all affinities so
        that all threads can run on any cpu core.
    'revert'
        Restores thread affinities stored from previous 'clear'.
*/
struct AffinityControl : SGPropertyChangeListener {
    AffinityControl()
    {
        m_node = globals->get_props()->getNode("/sim/affinity-control", true /*create*/);
        m_node->addChangeListener(this);
    }
    void valueChanged(SGPropertyNode* node) override
    {
#ifdef __linux__
        std::string s = m_node->getStringValue();
        if (s == m_state) {
            SG_LOG(SG_VIEW, SG_ALERT, "Ignoring m_node=" << s << " because same as m_state.");
        } else if (s == "clear") {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "/proc/%i/task", getpid());
            SGPath path(buffer);
            simgear::Dir dir(path);
            m_thread_masks.clear();
            simgear::PathList pids = dir.children(
                simgear::Dir::TYPE_DIR | simgear::Dir::NO_DOT_OR_DOTDOT);
            for (SGPath path : pids) {
                std::string leaf = path.file();
                int pid = atoi(leaf.c_str());
                cpu_set_t mask;
                int e = sched_getaffinity(pid, sizeof(mask), &mask);
                SG_LOG(SG_VIEW, SG_ALERT, "Called sched_getaffinity()"
                                              << " pid=" << pid << " => e=" << e << " mask=" << mask);
                if (!e) {
                    m_thread_masks[pid] = mask;
                    memset(&mask, 255, sizeof(mask));
                    e = sched_setaffinity(pid, sizeof(mask), &mask);
                    SG_LOG(SG_VIEW, SG_ALERT, "Called sched_setaffinity()"
                                                  << " pid=" << pid << " => e=" << e << " mask=" << mask);
                    //assert(!e);
                }
            }
            m_state = s;
        } else if (s == "revert") {
            for (auto it : m_thread_masks) {
                pid_t pid = it.first;
                cpu_set_t mask = it.second;
                int e = sched_setaffinity(pid, sizeof(mask), &mask);
                SG_LOG(SG_VIEW, SG_ALERT, "Called sched_setaffinity()"
                                              << " pid=" << pid << " => e=" << e << " mask=" << mask);
                //assert(!e);
            }
            m_thread_masks.clear();
            m_state = s;
        } else {
            SG_LOG(SG_VIEW, SG_ALERT, "Unrecognised m_node=" << s);
        }
#endif
    }
    SGPropertyNode_ptr m_node;
    std::string m_state;
#ifdef __linux__
    std::map<int, cpu_set_t> m_thread_masks;
#endif
};


int fgOSMainLoop()
{
    AffinityControl affinity_control;
    osgViewer::ViewerBase* viewer_base = globals->get_renderer()->getViewerBase();
    viewer_base->setReleaseContextAtEndOfFrameHint(false);
    if (!viewer_base->isRealized()) {
        viewer_base->realize();
        std::string affinity = fgGetString("/sim/thread-cpu-affinity");
        SG_LOG(SG_VIEW, SG_ALERT, "affinity=" << affinity);
        if (affinity != "") {
            ShowAffinities();
            if (affinity == "osg") {
                SG_LOG(SG_VIEW, SG_ALERT, "Resetting affinity of current thread getpid()=" << getpid());
                OpenThreads::Affinity affinity;
                OpenThreads::SetProcessorAffinityOfCurrentThread(affinity);
                ShowAffinities();
            }
        }
    }

    while (!viewer_base->done()) {
        fgIdleHandler idleFunc = globals->get_renderer()->getEventHandler()->getIdleHandler();
        if (idleFunc) {
            _lastUpdate.stamp();
            (*idleFunc)();
            if (fgGetBool("/sim/position-finalized", false)) {
                if (simHost && simFrameCount && simTotalHostTime && simFrameResetCount) {
                    int curFrameCount = simFrameCount->getIntValue();
                    double totalSimTime = simTotalHostTime->getDoubleValue();
                    if (simFrameResetCount->getBoolValue()) {
                        curFrameCount = 0;
                        totalSimTime = 0;
                        simFrameResetCount->setBoolValue(false);
                    }
                    double lastSimFrame_ms = _lastUpdate.elapsedMSec();
                    double idle_wait = 0;
                    if (frameWait)
                        idle_wait = frameWait->getDoubleValue();
                    if (lastSimFrame_ms > 0) {
                        totalSimTime += lastSimFrame_ms - idle_wait;
                        simTotalHostTime->setDoubleValue(totalSimTime);
                        curFrameCount++;
                        simFrameCount->setIntValue(curFrameCount);
                        simHost->setDoubleValue(totalSimTime / curFrameCount);
                    }
                }
            }
        }
        globals->get_renderer()->update();
#ifdef ENABLE_OSGXR
        VRManager::instance()->update();
#endif
        viewer_base->frame(globals->get_sim_time_sec());
    }

    flightgear::addSentryBreadcrumb("main loop exited", "info");
    return status;
}

int fgGetKeyModifiers()
{
    FGRenderer* r = globals->get_renderer();
    if (!r || !r->getEventHandler()) { // happens during shutdown
        return 0;
    }

    return r->getEventHandler()->getCurrentModifiers();
}

void fgWarpMouse(int x, int y)
{
    warpGUIPointer(CameraGroup::getDefault(), x, y);
}

void fgOSInit(int* argc, char** argv)
{
    // stock OSG windows are not Hi-DPI aware
    fgSetDouble("/sim/rendering/gui-pixel-ratio", 1.0);

#if defined(SG_MAC)
    cocoaRegisterTerminateHandler();
#endif

    globals->get_renderer()->init();
    WindowSystemAdapter::setWSA(new WindowSystemAdapter);
}

void fgOSCloseWindow()
{
    // reset the cursor before we close the window

    fgSetMouseCursor(FGMouseCursor::CURSOR_ARROW);

    if (globals && globals->get_renderer()) {
        osgViewer::ViewerBase* viewer_base = globals->get_renderer()->getViewerBase();
        if (viewer_base) {
            // https://code.google.com/p/flightgear-bugs/issues/detail?id=1291
            // https://sourceforge.net/p/flightgear/codetickets/1830/
            // explicitly stop threading before we delete the renderer or
            // viewMgr (which ultimately holds refs to the CameraGroup, and
            // GraphicsContext)
            viewer_base->stopThreading();
        }
    }
#ifdef ENABLE_OSGXR
    VRManager::instance()->destroyAndWait();
#endif
    FGScenery::resetPagerSingleton();
    flightgear::addSentryBreadcrumb("fgOSCloseWindow, clearing camera group", "info");
    flightgear::CameraGroup::setDefault(NULL);
    WindowSystemAdapter::setWSA(NULL);
    viewer = NULL;
}

void fgOSFullScreen()
{
    osgViewer::ViewerBase* viewer_base = globals->get_renderer()->getViewerBase();
    std::vector<osgViewer::GraphicsWindow*> windows;
    viewer_base->getWindows(windows);

    if (windows.empty())
        return; // Huh?!?

    /* Toggling window fullscreen is only supported for the main GUI window.
     * The other windows should use fixed setup from the camera.xml file anyway. */
    osgViewer::GraphicsWindow* window = windows[0];

    osg::GraphicsContext::WindowingSystemInterface* wsi = osg::GraphicsContext::getWindowingSystemInterface();
    if (wsi == NULL) {
        SG_LOG(SG_VIEW, SG_ALERT, "ERROR: No WindowSystemInterface available. Cannot toggle window fullscreen.");
        return;
    }

    static int previous_x = 0;
    static int previous_y = 0;
    static int previous_width = 800;
    static int previous_height = 600;

    unsigned int screenWidth;
    unsigned int screenHeight;
    wsi->getScreenResolution(*(window->getTraits()), screenWidth, screenHeight);

    int x;
    int y;
    int width;
    int height;
    window->getWindowRectangle(x, y, width, height);

    /* Note: the simple "is window size == screen size" check to detect full screen state doesn't work with
     * X screen servers in Xinerama mode, since the reported screen width (or height) exceeds the maximum width
     * (or height) usable by a single window (Xserver automatically shrinks/moves the full screen window to fit a
     * single display) - so we detect full screen mode using "WindowDecoration" state instead.
     * "false" - even when a single window is display in fullscreen */
    //bool isFullScreen = x == 0 && y == 0 && width == (int)screenWidth && height == (int)screenHeight;
    bool isFullScreen = !window->getWindowDecoration();

    SG_LOG(SG_VIEW, SG_DEBUG, "Toggling fullscreen. Previous window rectangle (" << x << ", " << y << ") x (" << width << ", " << height << "), fullscreen: " << isFullScreen << ", number of screens: " << wsi->getNumScreens());
    if (isFullScreen) {
        // limit x,y coordinates and window size to screen area
        if (previous_x + previous_width > (int)screenWidth)
            previous_x = 0;
        if (previous_y + previous_height > (int)screenHeight)
            previous_y = 0;

        // disable fullscreen mode, restore previous window size/coordinates
        x = previous_x;
        y = previous_y;
        width = previous_width;
        height = previous_height;
    } else {
        // remember previous setting
        previous_x = x;
        previous_y = y;
        previous_width = width;
        previous_height = height;

        // enable fullscreen mode, set new width/height
        x = 0;
        y = 0;
        width = screenWidth;
        height = screenHeight;
    }

    // set xsize/ysize properties to adapt GUI planes
    fgSetInt("/sim/startup/xsize", width);
    fgSetInt("/sim/startup/ysize", height);
    fgSetBool("/sim/startup/fullscreen", !isFullScreen);

    // reconfigure window
    window->setWindowDecoration(isFullScreen);
    window->setWindowRectangle(x, y, width, height);
    window->grabFocusIfPointerInWindow();
}

void fgSetMouseCursor(FGMouseCursor::Cursor cursor)
{
    FGMouseCursor::instance()->setCursor(cursor);
}

FGMouseCursor::Cursor fgGetMouseCursor()
{
    return FGMouseCursor::instance()->getCursor();
}
