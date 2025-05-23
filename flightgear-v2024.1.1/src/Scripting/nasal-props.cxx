// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <cstring>

#include <simgear/nasal/nasal.h>
#include <simgear/props/props.hxx>
#include <simgear/props/vectorPropTemplates.hxx>

#include <Main/globals.hxx>

#include "NasalSys.hxx"
#include "NasalSys_private.hxx"

using namespace std;

// Implementation of a Nasal wrapper for the SGPropertyNode class,
// using the Nasal "ghost" (er... Garbage collection Handle for
// OutSide Thingy) facility.
//
// Note that these functions appear in Nasal with prepended
// underscores.  They work on the low-level "ghost" objects and aren't
// intended for use from user code, but from Nasal code you will find
// in props.nas.  That is where the Nasal props.Node class is defined,
// which provides a saner interface along the lines of SGPropertyNode.

static void propNodeGhostDestroy(void* ghost)
{
  SGPropertyNode* prop = static_cast<SGPropertyNode*>(ghost);
  if (!SGPropertyNode::put(prop)) delete prop;
}

naGhostType PropNodeGhostType = { propNodeGhostDestroy, "prop", nullptr, nullptr };

naRef propNodeGhostCreate(naContext c, SGPropertyNode* ghost)
{
  if(!ghost) return naNil();
  SGPropertyNode::get(ghost);
  return naNewGhost(c, &PropNodeGhostType, ghost);
}

naRef FGNasalSys::propNodeGhost(SGPropertyNode* handle)
{
    return propNodeGhostCreate(d->_context, handle);
}

SGPropertyNode* ghostToPropNode(naRef ref)
{
  if (!naIsGhost(ref) || (naGhost_type(ref) != &PropNodeGhostType))
    return NULL;

  return static_cast<SGPropertyNode*>(naGhost_ptr(ref));
}

#define NASTR(s) s ? naStr_fromdata(naNewString(c),(char*)(s),strlen(s)) : naNil()

//
// Standard header for the extension functions.  It turns the "ghost"
// found in arg[0] into a SGPropertyNode_ptr, and then "unwraps" the
// vector found in the second argument into a normal-looking args
// array.  This allows the Nasal handlers to do things like:
//   Node.getChild = func { _getChild(me.ghost, arg) }
//
#define NODENOARG()                                                            \
    if(argc < 2 || !naIsGhost(args[0]) ||                                      \
        naGhost_type(args[0]) != &PropNodeGhostType)                           \
        naRuntimeError(c, "bad argument to props function");                   \
    SGPropertyNode_ptr node = static_cast<SGPropertyNode*>(naGhost_ptr(args[0]));

#define NODEARG()                                                              \
    NODENOARG();                                                               \
    naRef argv = args[1]

//
// Pops the first argument as a relative path if the first condition
// is true (e.g. argc > 1 for getAttribute) and if it is a string.
// If the second confition is true, then another is popped to specify
// if the node should be created (i.e. like the second argument to
// getNode())
//
// Note that this makes the function return nil if the node doesn't
// exist, so all functions with a relative_path parameter will
// return nil if the specified node does not exist.
//
#define MOVETARGET(cond1, create)                                              \
    if(cond1) {                                                                \
        naRef name = naVec_get(argv, 0);                                       \
        if(naIsString(name)) {                                                 \
            try {                                                              \
                node = node->getNode(naStr_data(name), create);                \
            } catch(const string& err) {                                       \
                naRuntimeError(c, (char *)err.c_str());                        \
                return naNil();                                                \
            }                                                                  \
            if(!node) return naNil();                                          \
            naVec_removefirst(argv); /* pop only if we were successful */      \
        }                                                                      \
    }


// Get the type of a property (returns a string).
// Forms:
//    props.Node.getType(string relative_path);
//    props.Node.getType();
static naRef f_getType(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;
    NODEARG();
    MOVETARGET(naVec_size(argv) > 0, false);
    const char* t = "unknown";
    switch(node->getType()) {
    case props::NONE:   t = "NONE";   break;
    case props::ALIAS:  t = "ALIAS";  break;
    case props::BOOL:   t = "BOOL";   break;
    case props::INT:    t = "INT";    break;
    case props::LONG:   t = "LONG";   break;
    case props::FLOAT:  t = "FLOAT";  break;
    case props::DOUBLE: t = "DOUBLE"; break;
    case props::STRING: t = "STRING"; break;
    case props::UNSPECIFIED: t = "UNSPECIFIED"; break;
    case props::VEC3D:  t = "VEC3D";  break;
    case props::VEC4D:  t = "VEC4D";  break;
    case props::EXTENDED: t = "EXTENDED";  break; // shouldn't happen
    }
    return NASTR(t);
}

// Check if type of a property is numeric (returns 0 or 1).
// Forms:
//    props.Node.isNumeric(string relative_path);
//    props.Node.isNumeric();
static naRef f_isNumeric(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;
    NODEARG();
    MOVETARGET(naVec_size(argv) > 0, false);
    switch(node->getType()) {
    case props::INT:
    case props::LONG:
    case props::FLOAT:
    case props::DOUBLE: return naNum(true);
    default:
        break;
    }
    return naNum(false);
}

// Check if type of a property is integer (returns 0 or 1).
// Forms:
//    props.Node.isInt(string relative_path);
//    props.Node.isInt();
static naRef f_isInt(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;
    NODEARG();
    MOVETARGET(naVec_size(argv) > 0, false);
    if ((node->getType() == props::INT) || (node->getType() == props::LONG)) {
        return naNum(true);
    }
    return naNum(false);
}


// Get an attribute of a property by name (returns true/false).
// Forms:
//    props.Node.getType(string relative_path,
//                       string attribute_name);
//    props.Node.getType(string attribute_name);
static naRef f_getAttribute(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    if(naVec_size(argv) == 0) return naNum(unsigned(node->getAttributes()));
    MOVETARGET(naVec_size(argv) > 1, false);
    naRef val = naVec_get(argv, 0);
    const char *a = naStr_data(val);
    SGPropertyNode::Attribute attr;
    if(!a) a = "";
    if(!strcmp(a, "last")) return naNum(SGPropertyNode::LAST_USED_ATTRIBUTE);
    else if(!strcmp(a, "children"))    return naNum(node->nChildren());
    else if(!strcmp(a, "listeners"))   return naNum(node->nListeners());
    // Number of references without instance used in this function
    else if(!strcmp(a, "references"))  return naNum(node.getNumRefs() - 1);
    else if(!strcmp(a, "tied"))        return naNum(node->isTied());
    else if(!strcmp(a, "alias"))       return naNum(node->isAlias());
    else if(!strcmp(a, "readable"))    attr = SGPropertyNode::READ;
    else if(!strcmp(a, "writable"))    attr = SGPropertyNode::WRITE;
    else if(!strcmp(a, "archive"))     attr = SGPropertyNode::ARCHIVE;
    else if(!strcmp(a, "trace-read"))  attr = SGPropertyNode::TRACE_READ;
    else if(!strcmp(a, "trace-write")) attr = SGPropertyNode::TRACE_WRITE;
    else if(!strcmp(a, "userarchive")) attr = SGPropertyNode::USERARCHIVE;
    else if(!strcmp(a, "preserve"))    attr = SGPropertyNode::PRESERVE;
    else if(!strcmp(a, "protected"))   attr = SGPropertyNode::PROTECTED;
    else if(!strcmp(a, "listener-safe"))        attr = SGPropertyNode::LISTENER_SAFE;
    else if(!strcmp(a, "value-changed-up"))     attr = SGPropertyNode::VALUE_CHANGED_UP;
    else if(!strcmp(a, "value-changed-down"))   attr = SGPropertyNode::VALUE_CHANGED_DOWN;

    else {
        naRuntimeError(c, "props.getAttribute() with invalid attribute");
        return naNil();
    }
    return naNum(node->getAttribute(attr));
}


// Set an attribute by name and boolean value or raw (bitmasked) number.
// Forms:
//    props.Node.setAttribute(string relative_path,
//                            string attribute_name,
//                            bool value);
//    props.Node.setAttribute(string attribute_name,
//                            bool value);
//    props.Node.setArtribute(int attributes);
static naRef f_setAttribute(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    if (node->getAttribute(SGPropertyNode::PROTECTED)) {
        naRuntimeError(c, "props.setAttribute() called on protected property %s",
                       node->getPath().c_str());
        return naNil();
    }

    MOVETARGET(naVec_size(argv) > 2, false);
    naRef val = naVec_get(argv, 0);
    if(naVec_size(argv) == 1 && naIsNum(val)) {
        naRef ret = naNum(node->getAttributes());
        // prevent Nasal modifying PROTECTED
        int attrs = static_cast<int>(val.num) & (~SGPropertyNode::PROTECTED);
        node->setAttributes(attrs);
        return ret;
    }
    SGPropertyNode::Attribute attr;
    const char *a = naStr_data(val);
    if(!a) a = "";
    if(!strcmp(a, "readable"))         attr = SGPropertyNode::READ;
    else if(!strcmp(a, "writable"))    attr = SGPropertyNode::WRITE;
    else if(!strcmp(a, "archive"))     attr = SGPropertyNode::ARCHIVE;
    else if(!strcmp(a, "trace-read"))  attr = SGPropertyNode::TRACE_READ;
    else if(!strcmp(a, "trace-write")) attr = SGPropertyNode::TRACE_WRITE;
    else if(!strcmp(a, "userarchive")) attr = SGPropertyNode::USERARCHIVE;
    else if(!strcmp(a, "preserve"))    attr = SGPropertyNode::PRESERVE;
    // explicitly don't allow "protected" to be modified here
    else {
        naRuntimeError(c, "props.setAttribute() with invalid attribute");
        return naNil();
    }
    naRef ret = naNum(node->getAttribute(attr));
    node->setAttribute(attr, naTrue(naVec_get(argv, 1)) ? true : false);
    return ret;
}


// Get the simple name of this node.
// Forms:
//    props.Node.getName();
static naRef f_getName(naContext c, naRef me, int argc, naRef* args)
{
    NODENOARG();
    return NASTR(node->getNameString().c_str());
}


// Get the index of this node.
// Forms:
//    props.Node.getIndex();
static naRef f_getIndex(naContext c, naRef me, int argc, naRef* args)
{
    NODENOARG();
    return naNum(node->getIndex());
}

// Check if other_node refers to the same as this node.
// Forms:
//    props.Node.equals(other_node);
static naRef f_equals(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();

    naRef rhs = naVec_get(argv, 0);
    if( !naIsGhost(rhs) || naGhost_type(rhs) != &PropNodeGhostType )
      return naNum(false);

    SGPropertyNode* node_rhs = static_cast<SGPropertyNode*>(naGhost_ptr(rhs));
    return naNum(node.ptr() == node_rhs);
}

template<typename T>
naRef makeVectorFromVec(naContext c, const T& vec)
{
    const int num_components
        = sizeof(vec.data()) / sizeof(typename T::value_type);
    naRef vector = naNewVector(c);
    naVec_setsize(c, vector, num_components);
    for (int i = 0; i < num_components; ++i)
        naVec_set(vector, i, naNum(vec[i]));
    return vector;
}


// Get the value of a node, with or without a relative path.
// Forms:
//    props.Node.getValue(string relative_path);
//    props.Node.getValue();
static naRef f_getValue(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;
    NODEARG();
    MOVETARGET(naVec_size(argv) > 0, false);
    return FGNasalSys::getPropertyValue(c, node);
}

template<typename T>
T makeVecFromVector(naRef vector)
{
    T vec;
    const int num_components
        = sizeof(vec.data()) / sizeof(typename T::value_type);
    int size = naVec_size(vector);

    for (int i = 0; i < num_components && i < size; ++i) {
        naRef element = naVec_get(vector, i);
        naRef n = naNumValue(element);
        if (!naIsNil(n))
            vec[i] = n.num;
    }
    return vec;
}

static std::string s_val_description(naRef val)
{
    std::ostringstream message;
    if (naIsNil(val))           message << "nil";
    else if (naIsNum(val))      message << "num:" << naNumValue(val).num;
    else if (naIsString(val))   message << "string:" << naStr_data(val);
    else if (naIsScalar(val))   message << "scalar";
    else if (naIsVector(val))   message << "vector";
    else if (naIsHash(val))     message << "hash";
    else if (naIsFunc(val))     message << "func";
    else if (naIsCode(val))     message << "code";
    else if (naIsCCode(val))    message << "ccode";
    else if (naIsGhost(val))    message << "ghost";
    else message << "?";
    return message.str();
}

// Helper function to set the value of a node; returns true if it succeeded or
// false if it failed.  <val> can be a string, number, or a
// vector or numbers (for SGVec3D/4D types).
static naRef f_setValueHelper(naContext c, SGPropertyNode_ptr node, naRef val) {
    bool result = false;
    if(naIsString(val)) {
        result = node->setStringValue(naStr_data(val));
    } else if(naIsVector(val)) {
        if(naVec_size(val) == 3)
            result = node->setValue(makeVecFromVector<SGVec3d>(val));
        else if(naVec_size(val) == 4)
            result = node->setValue(makeVecFromVector<SGVec4d>(val));
        else
            naRuntimeError(c, "props.setValue() vector value has wrong size");
    } else if(naIsNum(val)) {
        double d = naNumValue(val).num;
        if (SGMisc<double>::isNaN(d)) {
            naRuntimeError(c, "props.setValue() passed a NaN");
        }

        result = node->setDoubleValue(d);
    } else {
        naRuntimeError(c, "props.setValue() called with unsupported value %s", s_val_description(val).c_str());
    }
    return naNum(result);
}


// Set the value of a node; returns true if it succeeded or
// false if it failed. <val> can be a string, number, or a
// vector or numbers (for SGVec3D/4D types).
// Forms:
//    props.Node.setValue(string relative_path,
//                        val);
//    props.Node.setValue(val);
static naRef f_setValue(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, true);
    naRef val = naVec_get(argv, 0);
    return f_setValueHelper(c, node, val);
}

static naRef f_setIntValue(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, true);
    // Original code:
    //   int iv = (int)naNumValue(naVec_get(argv, 0)).num;

    // Junk to pacify the gcc-2.95.3 optimizer:
    naRef tmp0 = naVec_get(argv, 0);
    naRef tmp1 = naNumValue(tmp0);
    if(naIsNil(tmp1))
        naRuntimeError(c, "props.setIntValue() with non-number");
    double tmp2 = tmp1.num;
    int iv = (int)tmp2;

    return naNum(node->setIntValue(iv));
}

static naRef f_setBoolValue(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, true);
    naRef val = naVec_get(argv, 0);
    return naNum(node->setBoolValue(naTrue(val) ? true : false));
}

static naRef f_toggleBoolValue(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;

    NODEARG();
    MOVETARGET(naVec_size(argv) > 0, false);
    if (node->getType() != props::BOOL) {
        naRuntimeError(c, "props.toggleBoolValue() on non-bool prop");
    }

    const auto val = node->getBoolValue();
    return naNum(node->setBoolValue(val ? false : true));
}

static naRef f_setDoubleValue(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, true);
    naRef r = naNumValue(naVec_get(argv, 0));
    if (naIsNil(r))
        naRuntimeError(c, "props.setDoubleValue() with non-number");

    if (SGMisc<double>::isNaN(r.num)) {
      naRuntimeError(c, "props.setDoubleValue() passed a NaN");
    }

    return naNum(node->setDoubleValue(r.num));
}

static naRef f_adjustValue(naContext c, naRef me, int argc, naRef* args)
{
    using namespace simgear;

    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, false);
    naRef r = naNumValue(naVec_get(argv, 0));
    if (naIsNil(r))
        naRuntimeError(c, "props.adjustValue() with non-number");

    if (SGMisc<double>::isNaN(r.num)) {
        naRuntimeError(c, "props.adjustValue() passed a NaN");
    }

    switch(node->getType()) {
    case props::BOOL:
    case props::INT:
    case props::LONG:
    case props::FLOAT:
    case props::DOUBLE:
        // fall through
        break;

    default:
        naRuntimeError(c, "props.adjustValue() called on non-numeric type");
        return naNil();
    }

    const auto dv = node->getDoubleValue();
    return naNum(node->setDoubleValue(dv + r.num));
}

// Forward declaration
static naRef f_setChildrenHelper(naContext c, SGPropertyNode_ptr node, char* name, naRef val);

static naRef f_setValuesHelper(naContext c, SGPropertyNode_ptr node, naRef hash)
{
    if (!naIsHash(hash)) {
        naRuntimeError(c, "props.setValues() with non-hash");
    }

    naRef keyvec = naNewVector(c);
    naHash_keys(keyvec, hash);
    naRef ret;

    for (int i = 0; i < naVec_size(keyvec); i++) {
        naRef key = naVec_get(keyvec, i);
        if (! naIsScalar(key)) {
            naRuntimeError(c, "props.setValues() with non-scalar key value");
        }
        char* keystr = naStr_data(naStringValue(c, key));
        ret = f_setChildrenHelper(c, node, keystr, naHash_cget(hash, keystr));
    }

    return ret;
}

static naRef f_setValues(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    MOVETARGET(naVec_size(argv) > 1, true);
    naRef val = naVec_get(argv, 0);
    return f_setValuesHelper(c, node, val);
}

static naRef f_setChildrenHelper(naContext c, SGPropertyNode_ptr node, char* name, naRef val)
{
    naRef ret;
    try {
        SGPropertyNode_ptr subnode = node->getNode(name, true);

        if (naIsScalar(val)) {
            ret =  f_setValueHelper(c, subnode, val);
        } else if (naIsHash(val)) {
            ret = f_setValuesHelper(c, subnode, val);
        } else if (naIsVector(val)) {
            char nameBuf[1024];
            for (int i = 0; i < naVec_size(val); i++) {
                const auto len = ::snprintf(nameBuf, sizeof(nameBuf), "%s[%i]", name, i);
                if ((len < 0) || (len >= (int) sizeof(nameBuf))) {
                  naRuntimeError(c, "Failed to create buffer for property name in setChildren");
                }
                ret = f_setChildrenHelper(c, node, nameBuf, naVec_get(val, i));
            }
        } else if (naIsNil(val)) {
            // Nil value OK - no-op
        } else {
            // We have an error, but throwing a runtime error will prevent certain things from
            // working (such as the pilot list)
            // The nasal version would fail silently with invalid data - a runtime error will dump the stack and
            // stop execution.
            // Overall to be safer the new method should be functionally equivalent to keep compatibility.
            //
            //REMOVED: naRuntimeError(c, "props.setChildren() with unknown type");
        }
    } catch(const string& err) {
        naRuntimeError(c, (char *)err.c_str());
        return naNil();
    }

    return ret;
}

static naRef f_setChildren(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    if(! naIsString(naVec_get(argv, 0))) {
      naRuntimeError(c, "props.setChildren() with non-string first argument");
    }

    char* name = naStr_data(naVec_get(argv, 0));
    naRef val = naVec_get(argv, 1);
    return f_setChildrenHelper(c, node, name, val);
}

// Get the parent of this node as a ghost.
// Forms:
//    props.Node.getParent();
static naRef f_getParent(naContext c, naRef me, int argc, naRef* args)
{
    NODENOARG();
    SGPropertyNode* n = node->getParent();
    if(!n) return naNil();
    return propNodeGhostCreate(c, n);
}


// Get a child by name and optional index=0, creating if specified (by default it
// does not create it). If the node does not exist and create is false, then it
// returns nil, else it returns a (possibly new) property ghost.
// Forms:
//    props.Node.getChild(string relative_path,
//                        int index=0,
//                        bool create=false);
static naRef f_getChild(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef child = naVec_get(argv, 0);
    if(!naIsString(child)) return naNil();
    naRef idx = naNumValue(naVec_get(argv, 1));
    bool create = naTrue(naVec_get(argv, 2)) != 0;
    SGPropertyNode* n;
    try {
        if(naIsNil(idx)) {
            n = node->getChild(naStr_data(child), create);
        } else {
            n = node->getChild(naStr_data(child), (int)idx.num, create);
        }
    } catch (const string& err) {
        naRuntimeError(c, (char *)err.c_str());
        return naNil();
    }
    if(!n) return naNil();
    return propNodeGhostCreate(c, n);
}


// Get all children with a specified name as a vector of ghosts.
// Forms:
//    props.Node.getChildren(string relative_path);
//    props.Node.getChildren(); #get all children
static naRef f_getChildren(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef result = naNewVector(c);
    if(naIsNil(argv) || naVec_size(argv) == 0) {
        // Get all children
        for(int i=0; i<node->nChildren(); i++)
            naVec_append(result, propNodeGhostCreate(c, node->getChild(i)));
    } else {
        // Get all children of a specified name
        naRef name = naVec_get(argv, 0);
        if(!naIsString(name)) return naNil();
        try {
            vector<SGPropertyNode_ptr> children
                = node->getChildren(naStr_data(name));
            for(unsigned int i=0; i<children.size(); i++)
                naVec_append(result, propNodeGhostCreate(c, children[i]));
        } catch (const string& err) {
            naRuntimeError(c, (char *)err.c_str());
            return naNil();
        }
    }
    return result;
}


// Append a named child at the first unused index...
// Forms:
//    props.Node.addChild(string name,
//                        int min_index=0,
//                        bool append=true);
static naRef f_addChild(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef child = naVec_get(argv, 0);
    if(!naIsString(child)) return naNil();
    naRef ref_min_index = naNumValue(naVec_get(argv, 1));
    naRef ref_append = naVec_get(argv, 2);
    SGPropertyNode* n;
    try
    {
      int min_index = 0;
      if(!naIsNil(ref_min_index))
        min_index = ref_min_index.num;

      bool append = true;
      if(!naIsNil(ref_append))
        append = naTrue(ref_append) != 0;

      n = node->addChild(naStr_data(child), min_index, append);
    }
    catch (const string& err)
    {
      naRuntimeError(c, (char *)err.c_str());
      return naNil();
    }

    return propNodeGhostCreate(c, n);
}

static naRef f_addChildren(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef child = naVec_get(argv, 0);
    if(!naIsString(child)) return naNil();
    naRef ref_count = naNumValue(naVec_get(argv, 1));
    naRef ref_min_index = naNumValue(naVec_get(argv, 2));
    naRef ref_append = naVec_get(argv, 3);
    try
    {
      size_t count = 0;
      if( !naIsNum(ref_count) )
        throw string("props.addChildren() missing number of children");
      count = ref_count.num;

      int min_index = 0;
      if(!naIsNil(ref_min_index))
        min_index = ref_min_index.num;

      bool append = true;
      if(!naIsNil(ref_append))
        append = naTrue(ref_append) != 0;

      const simgear::PropertyList& nodes =
        node->addChildren(naStr_data(child), count, min_index, append);

      naRef result = naNewVector(c);
      for( size_t i = 0; i < nodes.size(); ++i )
        naVec_append(result, propNodeGhostCreate(c, nodes[i]));
      return result;
    }
    catch (const string& err)
    {
      naRuntimeError(c, (char *)err.c_str());
    }

    return naNil();
}


// Remove a child by name and index. Returns it as a ghost.
// Forms:
//    props.Node.removeChild(string relative_path,
//                           int index);
static naRef f_removeChild(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef child = naVec_get(argv, 0);
    naRef index = naVec_get(argv, 1);
    if(!naIsString(child) || !naIsNum(index)) return naNil();
    SGPropertyNode_ptr n;
    try {
        n = node->getChild(naStr_data(child), (int)index.num);
        if (n && n->getAttribute(SGPropertyNode::PROTECTED)) {
            naRuntimeError(c, "props.Node.removeChild() called on protected child %s of %s",
                           naStr_data(child), node->getPath().c_str());
            return naNil();
        }
        n = node->removeChild(naStr_data(child), (int)index.num);
    } catch (const string& err) {
        naRuntimeError(c, (char *)err.c_str());
    }
    return propNodeGhostCreate(c, n);
}


// Remove all children with specified name. Returns a vector of all the nodes
// removed as ghosts.
// Forms:
//    props.Node.removeChildren(string relative_path);
//    props.Node.removeChildren(); #remove all children
static naRef f_removeChildren(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef result = naNewVector(c);
    if(naIsNil(argv) || naVec_size(argv) == 0) {
        // Remove all children
        for(int i = node->nChildren() - 1; i >=0; i--) {
            SGPropertyNode_ptr n = node->getChild(i);
            if (n->getAttribute(SGPropertyNode::PROTECTED)) {
                SG_LOG(SG_NASAL, SG_ALERT, "props.Node.removeChildren: node " <<
                       n->getPath() << " is protected");
                continue;
            }

            node->removeChild(i);
            naVec_append(result, propNodeGhostCreate(c, n));
        }
    } else {
        // Remove all children of a specified name
        naRef name = naVec_get(argv, 0);
        if(!naIsString(name)) return naNil();
        try {
            auto children = node->getChildren(naStr_data(name));
            for (auto cn : children) {
                if (cn->getAttribute(SGPropertyNode::PROTECTED)) {
                    SG_LOG(SG_NASAL, SG_ALERT, "props.Node.removeChildren: node " <<
                        cn->getPath() << " is protected");
                    continue;
                }
                node->removeChild(cn);
                naVec_append(result, propNodeGhostCreate(c, cn));
            }
        } catch (const string& err) {
            naRuntimeError(c, (char *)err.c_str());
            return naNil();
        }
    }
    return result;
}

// Remove all children of a property node.
// Forms:
//    props.Node.removeAllChildren();
static naRef f_removeAllChildren(naContext c, naRef me, int argc, naRef* args)
{
  NODENOARG();
  node->removeAllChildren();
  return propNodeGhostCreate(c, node);
}

// Alias this property to another one; returns 1 on success or 0 on failure
// (only applicable to tied properties).
// Forms:
//    props.Node.alias(string global_path);
//    props.Node.alias(prop_ghost node);
//    props.Node.alias(props.Node node); #added by props.nas
static naRef f_alias(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    if (node->getAttribute(SGPropertyNode::PROTECTED)) {
        naRuntimeError(c, "props.Node.alias() called on protected property %s",
                       node->getPath().c_str());
        return naNil();
    }
    SGPropertyNode* al;
    naRef prop = naVec_get(argv, 0);
    try {
        if(naIsString(prop)) al = globals->get_props()->getNode(naStr_data(prop), true);
        else if(naIsGhost(prop)) al = static_cast<SGPropertyNode*>(naGhost_ptr(prop));
        else
            throw sg_exception("props.alias() with bad argument");
    } catch (sg_exception& err) {
        naRuntimeError(c, err.what());
        return naNil();
    }

    bool withListeners = false;
    if (naVec_size(argv) > 1) {
        withListeners = static_cast<int>(naVec_get(argv, 1).num) != 0;
    }
    return naNum(node->alias(al, withListeners));
}


// Un-alias this property. Returns 1 on success or 0 on failure (only
// applicable to tied properties).
// Forms:
//    props.Node.unalias();
static naRef f_unalias(naContext c, naRef me, int argc, naRef* args)
{
    NODENOARG();
    return naNum(node->unalias());
}


// Get the alias of this node as a ghost.
// Forms:
//    props.Node.getAliasTarget();
static naRef f_getAliasTarget(naContext c, naRef me, int argc, naRef* args)
{
    NODENOARG();
    return propNodeGhostCreate(c, node->getAliasTarget());
}


// Get a relative node. Returns nil if it does not exist and create is false,
// or a ghost object otherwise (wrapped into a props.Node object by props.nas).
// Forms:
//    props.Node.getNode(string relative_path,
//                       bool create=false);
static naRef f_getNode(naContext c, naRef me, int argc, naRef* args)
{
    NODEARG();
    naRef path = naVec_get(argv, 0);
    bool create = naTrue(naVec_get(argv, 1)) != 0;
    if(!naIsString(path)) return naNil();
    SGPropertyNode* n;
    try {
        n = node->getNode(naStr_data(path), create);
    } catch (const string& err) {
        naRuntimeError(c, (char *)err.c_str());
        return naNil();
    }
    return propNodeGhostCreate(c, n);
}


// Create a new property node.
// Forms:
//    props.Node.new();
static naRef f_new(naContext c, naRef me, int argc, naRef* args)
{
    return propNodeGhostCreate(c, new SGPropertyNode());
}


// Get the global root node (cached by props.nas so that it does
// not require a function call).
// Forms:
//    props._globals()
//    props.globals
static naRef f_globals(naContext c, naRef me, int argc, naRef* args)
{
    return propNodeGhostCreate(c, globals->get_props());
}

static struct {
    naCFunction func;
    const char* name;
} propfuncs[] = {
    { f_getType,            "_getType"            },
    { f_getAttribute,       "_getAttribute"       },
    { f_setAttribute,       "_setAttribute"       },
    { f_getName,            "_getName"            },
    { f_getIndex,           "_getIndex"           },
    { f_equals,             "_equals"             },
    { f_getValue,           "_getValue"           },
    { f_setValue,           "_setValue"           },
    { f_setValues,          "_setValues"          },
    { f_setIntValue,        "_setIntValue"        },
    { f_setBoolValue,       "_setBoolValue"       },
    { f_toggleBoolValue,    "_toggleBoolValue"    },
    { f_setDoubleValue,     "_setDoubleValue"     },
    { f_getParent,          "_getParent"          },
    { f_getChild,           "_getChild"           },
    { f_getChildren,        "_getChildren"        },
    { f_addChild,           "_addChild"           },
    { f_addChildren,        "_addChildren"        },
    { f_removeChild,        "_removeChild"        },
    { f_removeChildren,     "_removeChildren"     },
    { f_removeAllChildren,  "_removeAllChildren"  },
    { f_setChildren,        "_setChildren"        },
    { f_alias,              "_alias"              },
    { f_unalias,            "_unalias"            },
    { f_getAliasTarget,     "_getAliasTarget"     },
    { f_getNode,            "_getNode"            },
    { f_new,                "_new"                },
    { f_globals,            "_globals"            },
    { f_isNumeric,          "_isNumeric"          },
    { f_isInt,              "_isInt"              },
    { f_adjustValue,        "_adjustValue"        },
    { 0, 0 }
};

naRef FGNasalSys::genPropsModule()
{
    naRef namespc = naNewHash(d->_context);
    for(int i=0; propfuncs[i].name; i++)
        hashset(namespc, propfuncs[i].name,
                naNewFunc(d->_context, naNewCCode(d->_context, propfuncs[i].func)));
    return namespc;
}

naRef FGNasalSys::getPropertyValue(naContext c, SGPropertyNode* node)
{
    using namespace simgear;
    if (!node)
        return naNil();
    
    switch(node->getType()) {
    case props::BOOL:   case props::INT:
    case props::LONG:   case props::FLOAT:
    case props::DOUBLE:
    {
        double dv = node->getDoubleValue();
        if (SGMisc<double>::isNaN(dv)) {
          SG_LOG(SG_NASAL, SG_ALERT, "Nasal getValue: property " << node->getPath() << " is NaN");
          return naNil();
        }

        return naNum(dv);
    }

    case props::STRING:
    case props::UNSPECIFIED:
        return NASTR(node->getStringValue().c_str());
    case props::VEC3D:
        return makeVectorFromVec(c, node->getValue<SGVec3d>());
    case props::VEC4D:
        return makeVectorFromVec(c, node->getValue<SGVec4d>());
    default:
        return naNil();
    }
}
