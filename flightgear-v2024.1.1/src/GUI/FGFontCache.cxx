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
#include <config.h>

#include "FGFontCache.hxx"

// this is our one in 3rdparty
#include "fnt.h"

#if defined(HAVE_PUI)
#include "FlightGear_pu.h"
#endif

#include <simgear/props/props.hxx>
#include <simgear/misc/sg_dir.hxx>

#include <Main/globals.hxx>

////////////////////////////////////////////////////////////////////////
// FGFontCache class.
////////////////////////////////////////////////////////////////////////

static std::unique_ptr<FGFontCache> global_fontCacheInstance;

#if defined(HAVE_PUI)
extern puFont FONT_HELVETICA_14;
extern puFont FONT_SANS_12B;
extern puFont FONT_HELVETICA_12;
#endif
namespace
{
struct GuiFont
{
    const std::string name;
    puFont *font;
};

const std::initializer_list<GuiFont> guiFonts = {
    {"default", &PUFONT_HELVETICA_12},
    {"FIXED_8x13", &PUFONT_8_BY_13},
    {"FIXED_9x15", &PUFONT_9_BY_15},
    {"TIMES_10", &PUFONT_TIMES_ROMAN_10},
    {"TIMES_24", &PUFONT_TIMES_ROMAN_24},
    {"HELVETICA_10", &PUFONT_HELVETICA_10},
    {"HELVETICA_18", &PUFONT_HELVETICA_18},
#if defined(HAVE_PUI)
    {"HELVETICA_12", &FONT_HELVETICA_12},
    {"HELVETICA_14", &FONT_HELVETICA_14},
    {"SANS_12B", &FONT_SANS_12B},
#endif
};
}

FGFontCache* FGFontCache::instance()
{
    if (!global_fontCacheInstance.get()) {
        global_fontCacheInstance.reset(new FGFontCache);
    }

    return global_fontCacheInstance.get();
}

void FGFontCache::shutdown()
{
    global_fontCacheInstance.reset();
}

FGFontCache::FGFontCache() :
    _initialized(false)
{
}

FGFontCache::~FGFontCache()
{
    for (auto puFontIt : _cache) {
        delete puFontIt.second;
    }

    // these were created i initializeFonts
    for (auto texFontIt : _texFonts) {
        delete texFontIt.second;
    }
}

bool FGFontCache::FntParams::operator<(const FntParams& other) const
{
    int comp = name.compare(other.name);
    if (comp < 0)
        return true;
    else if (comp > 0)
        return false;
    if (size < other.size)
        return true;
    else if (size > other.size)
        return false;
    return slant < other.slant;
}

FGFontCache::FontCacheEntry*
FGFontCache::getfnt(const std::string& fontName, float size, float slant)
{
    FntParams fntParams(fontName, size, slant);
    PuFontMap::iterator i = _cache.find(fntParams);
    if (i != _cache.end())
        return i->second;
    // fntTexFont s are all preloaded into the _texFonts map
    TexFontMap::iterator texi = _texFonts.find(fontName);
    fntTexFont* texfont = 0;
    puFont* pufont = 0;
    if (texi != _texFonts.end()) {
        texfont = texi->second;
    } else {
        auto guifont = std::find_if(guiFonts.begin(), guiFonts.end(),
                                    [&fontName](const GuiFont& gf) {
                                        return gf.name == fontName;
                                    });

        if (guifont != guiFonts.end()) {
            pufont = guifont->font;
        }
    }

    auto f = new FontCacheEntry;
    if (pufont) {
        f->pufont = pufont;
    } else if (texfont) {
        f->texfont = texfont;
        f->ownsPUFont = true; // ensure it gets cleaned up
        f->pufont = new puFont;
        f->pufont->initialize(static_cast<fntFont *>(f->texfont), size, slant);
    } else {
        f->pufont = guiFonts.begin()->font;
    }

    // insert into the cache
    _cache[fntParams] = f;
    return f;
}

puFont *
FGFontCache::get(const std::string& name, float size, float slant)
{
    return getfnt(name, size, slant)->pufont;
}

fntTexFont *
FGFontCache::getTexFont(const std::string& name, float size, float slant)
{
    init();
    return getfnt(name, size, slant)->texfont;
}

puFont *
FGFontCache::get(SGPropertyNode *node)
{
    if (!node)
        return get("Helvetica.txf", 15.0, 0.0);

    std::string name = node->getStringValue("name", "Helvetica.txf");
    float size = node->getFloatValue("size", 15.0);
    float slant = node->getFloatValue("slant", 0.0);

    return get(name, size, slant);
}

void FGFontCache::init()
{
    if (!_initialized) {
        char *envp = ::getenv("FG_FONTS");
        if (envp != NULL) {
            _path = SGPath::fromEnv("FG_FONTS");
        } else {
            _path = globals->get_fg_root();
            _path.append("Fonts");
        }
        _initialized = true;
    }
}

SGPath
FGFontCache::getfntpath(const std::string& name)
{
    init();
    SGPath path(_path);
    if (name != "") {
        path.append(name);
        if (path.exists())
            return path;
    }

    path = SGPath(_path);
    path.append("Helvetica.txf");
    
    return path;
}

bool FGFontCache::initializeFonts()
{
    static std::string fontext(".txf");
    init();

    auto dir = simgear::Dir(_path);
    for (auto p : dir.children(simgear::Dir::TYPE_FILE, fontext)) {
        fntTexFont* f = new fntTexFont;
        if (f->load(p))
            _texFonts[p.file()] = f;
        else
            delete f;
    }

    return true;
}

FGFontCache::FontCacheEntry::~FontCacheEntry()
{
    if (ownsPUFont) {
        delete pufont;
    }
}

