// Canvas gui/dialog manager
//
// Copyright (C) 2012  Thomas Geymayer <tomgey@gmail.com>
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

#ifndef CANVAS_GUI_MGR_HXX_
#define CANVAS_GUI_MGR_HXX_

#include <simgear/canvas/canvas_fwd.hxx>
#include <simgear/canvas/elements/CanvasGroup.hxx>
#include <simgear/props/PropertyBasedMgr.hxx>
#include <simgear/props/propertyObject.hxx>

#include <osg/ref_ptr>
#include <osg/Geode>
#include <osg/MatrixTransform>

namespace osgViewer { class View; }
namespace osg { class Camera; }

namespace osgGA
{
  class GUIEventAdapter;
}

class GUIEventHandler;
class GUIMgr : public SGSubsystem
{
public:
    GUIMgr();

    // Subsystem API.
    void init() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "CanvasGUI"; }

    simgear::canvas::WindowPtr createWindow(const std::string& name = "");

    /**
     * Get simgear::canvas::Group containing all windows
     */
    simgear::canvas::GroupPtr getDesktop();

    /**
     * Set the input (keyboard) focus to the given window.
     */
    void setInputFocus(const simgear::canvas::WindowPtr& window);

    /**
     * Grabs the pointer so that all events are passed to this @a window until
     * the pointer is ungrabbed with ungrabPointer().
     */
    bool grabPointer(const simgear::canvas::WindowPtr& window);

    /**
     * Releases the grab acquired for this @a window with grabPointer().
     */
    void ungrabPointer(const simgear::canvas::WindowPtr& window);

    /**
     * specify the osgViewer::View and Camera
     */
    void setGUIViewAndCamera(osgViewer::View* view, osg::Camera* cam);
protected:
    simgear::canvas::GroupPtr           _desktop;
    osg::ref_ptr<GUIEventHandler>       _event_handler;
    osg::ref_ptr<osgViewer::View>       _viewerView;
    osg::ref_ptr<osg::Camera>           _camera;

    simgear::canvas::Placements
    addWindowPlacement( SGPropertyNode* placement,
                        simgear::canvas::CanvasPtr canvas );
};

#endif /* CANVAS_GUI_MGR_HXX_ */
