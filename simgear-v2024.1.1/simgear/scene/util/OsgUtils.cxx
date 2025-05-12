/*
 * SPDX-FileName: OsgUtils.cxx
 * SPDX-FileComment: Useful templates for interfacing to Open Scene Graph
 * SPDX-FileCopyrightText: Copyright (C) 2008  Tim Moore <timoore@redhat.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "OsgUtils.hxx"

#include <simgear/debug/logstream.hxx>

namespace simgear {
namespace osgutils {

osgText::Text::AlignmentType mapAlignment(const std::string& val)
{
    if (val == "left-top" || val == "LeftTop" || val == "LEFT_TOP")
        return osgText::Text::LEFT_TOP;
    else if (val == "left-center" || val == "LeftCenter" || val == "LEFT_CENTER")
        return osgText::Text::LEFT_CENTER;
    else if (val == "left-bottom" || val == "LeftBottom" || val == "LEFT_BOTTOM")
        return osgText::Text::LEFT_BOTTOM;
    else if (val == "center-top" || val == "CenterTop" || val == "CENTER_TOP")
        return osgText::Text::CENTER_TOP;
    else if (val == "center-center" || val == "CenterCenter" || val == "CENTER_CENTER")
        return osgText::Text::CENTER_CENTER;
    else if (val == "center-bottom" || val == "CenterBottom" || val == "CENTER_BOTTOM")
        return osgText::Text::CENTER_BOTTOM;
    else if (val == "right-top" || val == "RightTop" || val == "RIGHT_TOP")
        return osgText::Text::RIGHT_TOP;
    else if (val == "right-center" || val == "RightCenter" || val == "RIGHT_CENTER")
        return osgText::Text::RIGHT_CENTER;
    else if (val == "right-bottom" || val == "RightBottom" || val == "RIGHT_BOTTOM")
        return osgText::Text::RIGHT_BOTTOM;
    else if (val == "left-base-line" || val == "LeftBaseLine" || val == "LEFT_BASE_LINE")
        return osgText::Text::LEFT_BASE_LINE;
    else if (val == "center-base-line" || val == "CenterBaseLine" || val == "CENTER_BASE_LINE")
        return osgText::Text::CENTER_BASE_LINE;
    else if (val == "right-base-line" || val == "RightBaseLine" || val == "RIGHT_BASE_LINE")
        return osgText::Text::RIGHT_BASE_LINE;
    else if (val == "left-bottom-base-line" || val == "LeftBottomBaseLine" || val == "LEFT_BOTTOM_BASE_LINE")
        return osgText::Text::LEFT_BOTTOM_BASE_LINE;
    else if (val == "center-bottom-base-line" || val == "CenterBottomBaseLine" || val == "CENTER_BOTTOM_BASE_LINE")
        return osgText::Text::CENTER_BOTTOM_BASE_LINE;
    else if (val == "right-bottom-base-line" || val == "RightBottomBaseLine" || val == "RIGHT_BOTTOM_BASE_LINE")
        return osgText::Text::RIGHT_BOTTOM_BASE_LINE;
    else if (val == "base-line")
        return osgText::Text::BASE_LINE;
    else {
        SG_LOG(SG_GENERAL, SG_ALERT, "ignoring unknown text-alignment '" << val << "'.");
        return osgText::Text::BASE_LINE;
    }
}

} // namespace osgutils
} // namespace simgear
