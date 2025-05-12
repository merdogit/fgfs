/*
 * SPDX-FileCopyrightText: Copyright (C) 2024 Fernando García Liñán
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <osg/Camera>
#include <osg/Drawable>

namespace simgear {

/**
 * Call glMemoryBarrier() before rendering a given drawable.
 */
class DrawableMemoryBarrier : public osg::Drawable::DrawCallback {
public:
    DrawableMemoryBarrier(GLbitfield barriers) : _barriers(barriers) {}
    void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* drawable) const override
    {
        renderInfo.getState()->get<osg::GLExtensions>()->glMemoryBarrier(_barriers);
        drawable->drawImplementation(renderInfo);
    }
private:
    GLbitfield _barriers;
};

/**
 * Call glMemoryBarrier() before a camera starts rendering.
 * This callback should be set as the initial draw callback, which ensures that
 * the barrier is issued before the drawing of the camera's subgraph and pre
 * render stages.
 */
class CameraMemoryBarrier : public osg::Camera::DrawCallback {
public:
    CameraMemoryBarrier(GLbitfield barriers) : _barriers(barriers) {}
    void operator()(osg::RenderInfo& renderInfo) const override
    {
        renderInfo.getState()->get<osg::GLExtensions>()->glMemoryBarrier(_barriers);
    }
private:
    GLbitfield _barriers;
};

} // namespace simgear
