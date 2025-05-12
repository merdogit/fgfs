/*
 * SPDX-FileName: HUD_label.cxx
 * SPDX-FileComment: HUD Label
 * SPDX-FileCopyrightText: Copyright (C) 1997  Michele F. America  [micheleamerica#geocities:com]
 * SPDX-FileContributor: Copyright (C) 2006  Melchior FRANZ  [mfranz#aon:at]
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "HUD.hxx"
#include "HUD_private.hxx"

#include <Main/globals.hxx>

HUD::Label::Label(HUD *hud, const SGPropertyNode *n, float x, float y) :
    Item(hud, n, x, y),
    _input(n->getNode("input", false)),
    _box(n->getBoolValue("box", false)),
    _pointer_width(n->getFloatValue("pointer-width", 7.0)),
    _pointer_length(n->getFloatValue("pointer-length", 5.0)),
    _blink_condition(0),
    _blink_interval(n->getFloatValue("blinking/interval", -1.0f)),
    _blink_target(0.0),
    _blink_state(true)
{
    const SGPropertyNode *node = n->getNode("blinking/condition");
    if (node)
       _blink_condition = sgReadCondition(globals->get_props(), node);

    std::string halign = n->getStringValue("halign", "center");
    if (halign == "left")
        _halign = LEFT;
    else if (halign == "right")
        _halign = RIGHT;
    else
        _halign = HCENTER;

    _halign |= VCENTER;

    std::string pre = n->getStringValue("prefix", "");
    std::string post = n->getStringValue("postfix", "");
    std::string fmt = n->getStringValue("format", "");

    if (!pre.empty())
        _format = pre;

    if (!fmt.empty())
        _format += fmt;
    else
        _format += "%s";

    if (!post.empty())
        _format += post;

    _mode = check_format(_format.c_str());
    if (_mode == INVALID) {
        SG_LOG(SG_INPUT, SG_ALERT, "HUD: invalid format '" << _format.c_str()
                << "' in <label> '" << _name << '\'');
        _format = "INVALID";
        _mode = NONE;
    }

    blink();
}


void HUD::Label::draw(void)
{
    if (!((_mode == NONE || _input.isValid()) && blink()))
        return;

    if (_box) {
        float l, r, p;
        float pw = _pointer_width / 2.0;

        l = _center_x - pw;
        r = _center_x + pw;
        bool draw_parallel = fabs(_pointer_width - _w) > 2.0; // draw lines left and right of arrow?

        if (option_bottom()) {
            if (draw_parallel) {
                draw_line(_x, _y, l, _y);
                draw_line(r, _y, _x + _w, _y);
            }
            p = _y - _pointer_length;
            draw_line(l, _y, _center_x, p);
            draw_line(_center_x, p, r, _y);
        } else
            draw_line(_x, _y, _x + _w, _y);

        if (option_top()) {
            if (draw_parallel) {
                draw_line(_x, _y + _h, l, _y + _h);
                draw_line(r, _y + _h, _x + _w, _y + _h);
            }
            p = _y + _h + _pointer_length;
            draw_line(l, _y + _h, _center_x, p);
            draw_line(_center_x, p, r, _y + _h);
        } else
            draw_line(_x + _w, _y + _h, _x, _y + _h);

        l = _center_y - pw;
        r = _center_y + pw;
        draw_parallel = fabs(_pointer_width - _h) > 2.0;

        if (option_left()) {
            if (draw_parallel) {
                draw_line(_x, _y, _x, l);
                draw_line(_x, r, _x, _y + _h);
            }
            p = _x - _pointer_length;
            draw_line(_x, l, p, _center_y);
            draw_line(p, _center_y, _x, r);
        } else
            draw_line(_x, _y + _h, _x, _y);

        if (option_right()) {
            if (draw_parallel) {
                draw_line(_x + _w, _y, _x + _w, l);
                draw_line(_x + _w, r, _x + _w, _y + _h);
            }
            p = _x + _w + _pointer_length;
            draw_line(_x + _w, l, p, _center_y);
            draw_line(p, _center_y, _x + _w, r);
        } else
            draw_line(_x + _w, _y, _x + _w, _y + _h);
    }

    const int BUFSIZE = 256;
    char buf[BUFSIZE+1];
	buf[ BUFSIZE] = '\0';  // Be sure to terminate properly
    if (_mode == NONE)
        snprintf(buf, BUFSIZE, _format.c_str(), 0);
    else if (_mode == STRING)
        snprintf(buf, BUFSIZE, _format.c_str(), _input.getStringValue().c_str());
    else if (_mode == INT)
        snprintf(buf, BUFSIZE, _format.c_str(), int(_input.getFloatValue()));
    else if (_mode == LONG)
        snprintf(buf, BUFSIZE, _format.c_str(), long(_input.getFloatValue()));
    else if (_mode == FLOAT)
        snprintf(buf, BUFSIZE, _format.c_str(), float(_input.getFloatValue()));
    else if (_mode == DOUBLE) // not really supported yet
        snprintf(buf, BUFSIZE, _format.c_str(), double(_input.getFloatValue()));

    if (_halign & HCENTER)
        draw_text(_center_x, _center_y, buf, _halign, get_digits());
    else if (_halign & LEFT)
        draw_text(_x, _center_y, buf, _halign, get_digits());
    else // if (_halign & RIGHT)
        draw_text(_x + _w, _center_y, buf, _halign, get_digits());
}


bool HUD::Label::blink()
{
    if (_blink_interval < 0.0f)
        return true;

    if (_blink_condition && !_blink_condition->test())
        return true;

    if (_hud->timer() < _blink_target)
        return _blink_state;

    _blink_target = _hud->timer() + _blink_interval;
    return _blink_state = !_blink_state;
}


