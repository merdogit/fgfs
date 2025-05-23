//  panel_io.cxx - I/O for 2D panel.
//
//  Written by David Megginson, started January 2000.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2 of the
//  License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  $Id: panel_io.cxx,v 1.3 2016/08/25 23:41:34 allaert Exp $

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include <string.h>             // for strcmp()

#include <simgear/compiler.h>
#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/condition.hxx>
#include <simgear/props/props_io.hxx>

#include "panel_io.hxx"
#include "ApplicationProperties.hxx"
#include "FGGroupLayer.hxx"
#include "FGLayeredInstrument.hxx"
#include "FGSwitchLayer.hxx"
#include "FGTextLayer.hxx"
#include "FGTexturedLayer.hxx"


////////////////////////////////////////////////////////////////////////
// Read and construct a panel.
//
// The panel is specified as a regular property list, and each of the
// instruments is its own, separate property list (and thus, a separate
// XML document).  The functions in this section read in the files
// as property lists, then extract properties to set up the panel
// itself.
//
// A panel contains zero or more instruments.
//
// An instrument contains one or more layers and zero or more actions.
//
// A layer contains zero or more transformations.
//
// Some special types of layers also contain other objects, such as
// chunks of text or other layers.
//
// There are currently four types of layers:
//
// 1. Textured Layer (type="texture"), the default
// 2. Text Layer (type="text")
// 3. Switch Layer (type="switch")
// 4. Built-in Layer (type="built-in", must also specify class)
//
// The only built-in layer so far is the ribbon for the magnetic compass
// (class="compass-ribbon").
//
// There are three types of actions:
//
// 1. Adjust (type="adjust"), the default
// 2. Swap (type="swap")
// 3. Toggle (type="toggle")
//
// There are three types of transformations:
//
// 1. X shift (type="x-shift"), the default
// 2. Y shift (type="y-shift")
// 3. Rotation (type="rotation")
//
// Each of these may be associated with a property, so that a needle
// will rotate with the airspeed, for example, or may have a fixed
// floating-point value.
////////////////////////////////////////////////////////////////////////


/**
 * Read a cropped texture from the instrument's property list.
 *
 * The x1 and y1 properties give the starting position of the texture
 * (between 0.0 and 1.0), and the the x2 and y2 properties give the
 * ending position.  For example, to use the bottom-left quarter of a
 * texture, x1=0.0, y1=0.0, x2=0.5, y2=0.5.
 */
static FGCroppedTexture_ptr
readTexture (const SGPropertyNode *node) {
  SG_LOG(SG_COCKPIT, SG_DEBUG, "Read texture " << node->getName ());

  return new FGCroppedTexture (node->getStringValue ("path"),
                               node->getFloatValue ("x1"),
                               node->getFloatValue ("y1"),
                               node->getFloatValue ("x2", 1.0),
                               node->getFloatValue ("y2", 1.0));
}

/**
 * Test for a condition in the current node.
 */


////////////////////////////////////////////////////////////////////////
// Read a condition and use it if necessary.
////////////////////////////////////////////////////////////////////////

static void
readConditions (SGConditional *component, const SGPropertyNode *node) {
  const SGPropertyNode * conditionNode (node->getChild ("condition"));
  if (conditionNode != 0) {
    // The top level is implicitly AND
    component->setCondition (sgReadCondition (ApplicationProperties::Properties,
                                              conditionNode));
  }
}

/**
 * Read a transformation from the instrument's property list.
 *
 * The panel module uses the transformations to slide or spin needles,
 * knobs, and other indicators, and to place layers in the correct
 * positions.  Every layer starts centered exactly on the x,y co-ordinate,
 * and many layers need to be moved or rotated simply to display the
 * instrument correctly.
 *
 * There are three types of transformations:
 *
 * "x-shift" - move the layer horizontally.
 *
 * "y-shift" - move the layer vertically.
 *
 * "rotation" - rotate the layer.
 *
 * Each transformation may have a fixed offset, and may also have
 * a floating-point property value to add to the offset.  The
 * floating-point property may be clamped to a minimum and/or
 * maximum range and scaled (after clamping).
 *
 * Note that because of the way OpenGL works, transformations will
 * appear to be applied backwards.
 */
static FGPanelTransformation *
readTransformation (const SGPropertyNode *node, const float w_scale, const float h_scale) {
  FGPanelTransformation *t (new FGPanelTransformation);

  const string name (node->getName ());
  string type (node->getStringValue ("type"));
  const string propName (node->getStringValue ("property", ""));
  const SGPropertyNode *target (0);

  if (type.empty ()) {
    SG_LOG(SG_COCKPIT, SG_INFO,
           "No type supplied for transformation " << name
           << " assuming \"rotation\"");
    type = "rotation";
  }

  if (!propName.empty ()) {
    target = ApplicationProperties::Properties->getNode (propName.c_str (), true);
  }
  t->node = target;
  t->min = node->getFloatValue ("min", -9999999);
  t->max = node->getFloatValue ("max", 99999999);
  t->has_mod = node->hasChild ("modulator");
  if (t->has_mod) {
      t->mod = node->getFloatValue ("modulator");
  }
  t->factor = node->getFloatValue ("scale", 1.0);
  t->offset = node->getFloatValue ("offset", 0.0);

  // Check for an interpolation table
  const SGPropertyNode *trans_table (node->getNode ("interpolation"));
  if (trans_table != 0) {
    SG_LOG(SG_COCKPIT, SG_INFO, "Found interpolation table with "
           << trans_table->nChildren() << " children");
    t->table = new SGInterpTable();
    for (int i = 0; i < trans_table->nChildren(); i++) {
      const SGPropertyNode * node = trans_table->getChild(i);
      if (!strcmp(node->getName(), "entry")) {
        double ind = node->getDoubleValue("ind", 0.0);
        double dep = node->getDoubleValue("dep", 0.0);
        SG_LOG(SG_COCKPIT, SG_INFO, "Adding interpolation entry "
               << ind << "==>" << dep);
        t->table->addEntry(ind, dep);
      } else {
        SG_LOG(SG_COCKPIT, SG_INFO, "Skipping " << node->getName()
               << " in interpolation");
      }
    }
  } else {
    t->table = 0;
  }

                                // Move the layer horizontally.
  if (type == "x-shift") {
    t->type = FGPanelTransformation::XSHIFT;
//     t->min *= w_scale; //removed by Martin Dressler
//     t->max *= w_scale; //removed by Martin Dressler
    t->offset *= w_scale;
    t->factor *= w_scale; //Added by Martin Dressler
  }

                                // Move the layer vertically.
  else if (type == "y-shift") {
    t->type = FGPanelTransformation::YSHIFT;
    //t->min *= h_scale; //removed
    //t->max *= h_scale; //removed
    t->offset *= h_scale;
    t->factor *= h_scale; //Added
  }

  // Rotate the layer.  The rotation
  // is in degrees, and does not need
  // to scale with the instrument size.
  else if (type == "rotation") {
    t->type = FGPanelTransformation::ROTATION;
  }

  else {
    SG_LOG(SG_COCKPIT, SG_ALERT, "Unrecognized transformation type " << type);
    delete t;
    return 0;
  }

  readConditions(t, node);
  SG_LOG(SG_COCKPIT, SG_DEBUG, "Read transformation " << name);
  return t;
}


/**
 * Read a chunk of text from the instrument's property list.
 *
 * A text layer consists of one or more chunks of text.  All chunks
 * share the same font size and color (and eventually, font), but
 * each can come from a different source.  There are three types of
 * text chunks:
 *
 * "literal" - a literal text string (the default)
 *
 * "text-value" - the current value of a string property
 *
 * "number-value" - the current value of a floating-point property.
 *
 * All three may also include a printf-style format string.
 */
FGTextLayer::Chunk *
readTextChunk (const SGPropertyNode *node) {
  FGTextLayer::Chunk *chunk;
  const string name (node->getStringValue ("name"));
  string type (node->getStringValue ("type"));
  const string format (node->getStringValue ("format"));

  // Default to literal text.
  if (type.empty ()) {
    SG_LOG(SG_COCKPIT, SG_INFO, "No type provided for text chunk " << name
           << " assuming \"literal\"");
    type = "literal";
  }

  // A literal text string.
  if (type == "literal") {
    const string text (node->getStringValue ("text"));
    chunk = new FGTextLayer::Chunk (text, format);
  } else if (type == "text-value") {
    // The value of a string property.
    const SGPropertyNode *target
      (ApplicationProperties::Properties->getNode (node->getStringValue ("property"), true));
    chunk = new FGTextLayer::Chunk (FGTextLayer::TEXT_VALUE, target, format);
  } else if (type == "number-value") {
    // The value of a float property.
    const string propName (node->getStringValue ("property"));
    const float scale (node->getFloatValue ("scale", 1.0));
    const float offset (node->getFloatValue ("offset", 0.0));
    const bool truncation (node->getBoolValue ("truncate", false));
    const SGPropertyNode *target (ApplicationProperties::Properties->getNode (propName.c_str (), true));
    chunk = new FGTextLayer::Chunk (FGTextLayer::DOUBLE_VALUE, target,
                                    format, scale, offset, truncation);
  } else {
    // Unknown type.
    SG_LOG(SG_COCKPIT, SG_ALERT, "Unrecognized type " << type
           << " for text chunk " << name);
    return 0;
  }

  readConditions (chunk, node);
  return chunk;
}

/**
 * Read a single layer from an instrument's property list.
 *
 * Each instrument consists of one or more layers stacked on top
 * of each other; the lower layers show through only where the upper
 * layers contain an alpha component.  Each layer can be moved
 * horizontally and vertically and rotated using transformations.
 *
 * This module currently recognizes four kinds of layers:
 *
 * "texture" - a layer containing a texture (the default)
 *
 * "text" - a layer containing text
 *
 * "switch" - a layer that switches between two other layers
 *   based on the current value of a boolean property.
 *
 * "built-in" - a hard-coded layer supported by C++ code in FlightGear.
 *
 * Currently, the only built-in layer class is "compass-ribbon".
 */
static FGInstrumentLayer *
readLayer (const SGPropertyNode *node, const float w_scale, const float h_scale) {
  FGInstrumentLayer *layer (NULL);
  const string name (node->getStringValue ("name"));
  string type (node->getStringValue ("type"));
  int w (node->getIntValue ("w", -1));
  int h (node->getIntValue ("h", -1));
  const bool emissive (node->getBoolValue ("emissive", false));
  if (w != -1) {
    w = int (w * w_scale);
  }
  if (h != -1) {
    h = int (h * h_scale);
  }
  if (type.empty ()) {
    SG_LOG(SG_COCKPIT, SG_INFO,
           "No type supplied for layer " << name
           << " assuming \"texture\"");
    type = "texture";
  }

  // A textured instrument layer.
  if (type == "texture") {
    const FGCroppedTexture_ptr texture (readTexture (node->getNode ("texture")));
    layer = new FGTexturedLayer (texture, w, h);
    if (emissive) {
      FGTexturedLayer *tl = (FGTexturedLayer*) layer;
      tl->setEmissive (true);
    }
  } else if (type == "group") {
    // A group of sublayers.
    layer = new FGGroupLayer ();
    for (int i = 0; i < node->nChildren(); i++) {
      const SGPropertyNode *child = node->getChild (i);
      if (!strcmp (child->getName (), "layer")) {
        ((FGGroupLayer *) layer)->addLayer (readLayer (child, w_scale, h_scale));
      }
    }
  } else if (type == "text") {
    // A textual instrument layer.
    FGTextLayer *tlayer (new FGTextLayer (w, h)); // FIXME

    // Set the text color.
    const float red (node->getFloatValue ("color/red", 0.0));
    const float green (node->getFloatValue ("color/green", 0.0));
    const float blue (node->getFloatValue ("color/blue", 0.0));
    tlayer->setColor (red, green, blue);

    // Set the point size.
    const float pointSize (node->getFloatValue ("point-size", 10.0) * w_scale);
    tlayer->setPointSize (pointSize);

    // Set the font.
    const string fontName (node->getStringValue ("font", "7-Segment"));
    tlayer->setFontName (fontName);

    const SGPropertyNode *chunk_group (node->getNode ("chunks"));
    if (chunk_group != 0) {
      const int nChunks (chunk_group->nChildren ());
      for (int i = 0; i < nChunks; i++) {
        const SGPropertyNode *node (chunk_group->getChild (i));
        if (!strcmp(node->getName (), "chunk")) {
          FGTextLayer::Chunk * const chunk (readTextChunk (node));
          if (chunk != 0) {
            tlayer->addChunk (chunk);
          }
        } else {
          SG_LOG(SG_COCKPIT, SG_INFO, "Skipping " << node->getName()
                 << " in chunks");
        }
      }
      layer = tlayer;
    }
  } else if (type == "switch") {
    // A switch instrument layer.
    layer = new FGSwitchLayer ();
    for (int i = 0; i < node->nChildren (); i++) {
      const SGPropertyNode *child (node->getChild (i));
      if (!strcmp (child->getName (), "layer")) {
        ((FGGroupLayer *) layer)->addLayer (readLayer (child, w_scale, h_scale));
      }
    }
  } else {
    // An unknown type.
    SG_LOG(SG_COCKPIT, SG_ALERT, "Unrecognized layer type " << type);
    delete layer;
    return 0;
  }

  //
  // Get the transformations for each layer.
  //
  const SGPropertyNode *trans_group (node->getNode ("transformations"));
  if (trans_group != 0) {
    const int nTransformations (trans_group->nChildren ());
    for (int i = 0; i < nTransformations; i++) {
      const SGPropertyNode *node (trans_group->getChild (i));
      if (!strcmp(node->getName (), "transformation")) {
        FGPanelTransformation * const t (readTransformation (node, w_scale, h_scale));
        if (t != 0) {
          layer->addTransformation (t);
        }
      } else {
        SG_LOG(SG_COCKPIT, SG_INFO, "Skipping " << node->getName()
               << " in transformations");
      }
    }
  }

  readConditions (layer, node);
  SG_LOG(SG_COCKPIT, SG_DEBUG, "Read layer " << name);
  return layer;
}

/**
 * Read an instrument from a property list.
 *
 * The instrument consists of a preferred width and height
 * (the panel may override these), together with a list of layers
 * and a list of actions to be performed when the user clicks
 * the mouse over the instrument.  All co-ordinates are relative
 * to the instrument's position, so instruments are fully relocatable;
 * likewise, co-ordinates for actions and transformations will be
 * scaled automatically if the instrument is not at its preferred size.
 */
static FGPanelInstrument *
readInstrument (const SGPropertyNode *node) {
  const string name (node->getStringValue ("name"));
  const int x (node->getIntValue ("x", -1));
  const int y (node->getIntValue ("y", -1));
  const int real_w (node->getIntValue ("w", -1));
  const int real_h (node->getIntValue ("h", -1));
  int w (node->getIntValue ("w-base", -1));
  int h (node->getIntValue ("h-base", -1));

  if (x == -1 || y == -1) {
    SG_LOG(SG_COCKPIT, SG_ALERT,
           "x and y positions must be specified and > 0");
    return 0;
  }

  float w_scale (1.0);
  float h_scale (1.0);
  if (real_w != -1) {
    w_scale = float (real_w) / float (w);
    w = real_w;
  }
  if (real_h != -1) {
    h_scale = float (real_h) / float (h);
    h = real_h;
  }

  SG_LOG(SG_COCKPIT, SG_DEBUG, "Reading instrument " << name);

  FGLayeredInstrument * const instrument
    (new FGLayeredInstrument (x, y, w, h));

  //
  // Get the layers for the instrument.
  //
  const SGPropertyNode *layer_group (node->getNode ("layers"));
  if (layer_group != 0) {
    const int nLayers (layer_group->nChildren ());
    for (int i = 0; i < nLayers; i++) {
      const SGPropertyNode *node (layer_group->getChild (i));
      if (!strcmp (node->getName (), "layer")) {
        FGInstrumentLayer * const layer (readLayer (node, w_scale, h_scale));
        if (layer != 0) {
          instrument->addLayer (layer);
        }
      } else {
        SG_LOG(SG_COCKPIT, SG_INFO, "Skipping " << node->getName ()
               << " in layers");
      }
    }
  }

  readConditions (instrument, node);
  SG_LOG(SG_COCKPIT, SG_DEBUG, "Done reading instrument " << name);
  return instrument;
}

/**
 * Construct the panel from a property tree.
 */
SGSharedPtr<FGPanel>
FGReadablePanel::read (SGPropertyNode_ptr root) {
  SG_LOG(SG_COCKPIT, SG_INFO, "Reading properties for panel " <<
         root->getStringValue ("name", "[Unnamed Panel]"));

  FGPanel * const panel (new FGPanel (root));
  panel->setWidth (root->getIntValue ("w", 1024));
  panel->setHeight (root->getIntValue ("h", 443));

  SG_LOG(SG_COCKPIT, SG_INFO, "Size=" << panel->getWidth () << "x" << panel->getHeight ());

  // Assign the background texture, if any, or a bogus chequerboard.
  //
  const string bgTexture (root->getStringValue ("background"));
  if (!bgTexture.empty ()) {
    panel->setBackground (new FGCroppedTexture (bgTexture));
  }
  panel->setBackgroundWidth (root->getDoubleValue( "background-width", 1.0));
  panel->setBackgroundHeight (root->getDoubleValue( "background-height", 1.0));
  SG_LOG(SG_COCKPIT, SG_INFO, "Set background texture to " << bgTexture);

  //
  // Get multibackground if any...
  //
  for (int i = 0; i < 8; i++) {
    SGPropertyNode * const mbgNode (root->getChild ("multibackground", i));
    string mbgTexture;
    if (mbgNode != NULL) {
      mbgTexture = mbgNode->getStringValue ();
    }
    if (mbgTexture.empty ()) {
      if (i == 0) {
        break; // if first texture is missing, ignore the rest
      } else {
        mbgTexture = "FOO"; // if others are missing - set default texture
      }
    }
    panel->setMultiBackground (new FGCroppedTexture (mbgTexture), i);
    SG_LOG(SG_COCKPIT, SG_INFO, "Set multi-background texture" << i << " to " << mbgTexture);
  }
  //
  // Create each instrument.
  //
  SG_LOG( SG_COCKPIT, SG_INFO, "Reading panel instruments" );
  const SGPropertyNode *instrument_group (root->getChild ("instruments"));
  if (instrument_group != 0) {
    const int nInstruments (instrument_group->nChildren ());
    for (int i = 0; i < nInstruments; i++) {
      const SGPropertyNode *node = instrument_group->getChild (i);
      if (!strcmp (node->getName (), "instrument")) {
        FGPanelInstrument * const instrument (readInstrument (node));
        if (instrument != 0) {
          panel->addInstrument (instrument);
        }
      } else {
        SG_LOG(SG_COCKPIT, SG_INFO, "Skipping " << node->getName()
               << " in instruments section");
      }
    }
  }
  SG_LOG(SG_COCKPIT, SG_INFO, "Done reading panel instruments");


  //
  // Return the new panel.
  //
  return panel;
}

// end of panel_io.cxx
