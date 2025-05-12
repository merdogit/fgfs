#include "FGCocoaMenuBar.hxx"

#include <AppKit/NSMenu.h>
#include <AppKit/NSApplication.h>

#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/structure/SGBinding.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/sg_inlines.h>

#include <Main/fg_props.hxx>
#include <GUI/CocoaHelpers_private.h>
#include <Main/sentryIntegration.hxx>

#include <iostream>

using std::string;
using std::map;
using std::cout;
using namespace simgear;

typedef std::map<NSMenuItem*, SGBindingList> MenuItemBindings;

@class CocoaMenuDelegate;

namespace {
    
    class CocoaItemListener : public SGPropertyChangeListener
    {
    public:
        CocoaItemListener(SGPropertyNode_ptr prop, NSMenuItem* i) :
            enableProp(prop->getNode("enabled")),
            checkedProp(prop->getNode("checked")),
            item(i)
        {
            if (enableProp) {
                enableProp->addChangeListener(this);
                BOOL b = enableProp->getBoolValue();
                [item setEnabled:b];
            }

            if (checkedProp) {
                checkedProp->addChangeListener(this);
                auto b = checkedProp->getBoolValue();
                item.state = b ? NSControlStateValueOn : NSControlStateValueOff;
            }
        }
        
        ~CocoaItemListener()
        {
            if (enableProp) {
                enableProp->removeChangeListener(this);
            }
            if (checkedProp) {
                checkedProp->removeChangeListener(this);
            }
        }
        
        
        virtual void valueChanged(SGPropertyNode *node)
        {
            CocoaAutoreleasePool pool;
            if (node == enableProp) {
              BOOL b = node->getBoolValue();
              [item setEnabled:b];
            }

            if (node == checkedProp) {
              auto b = node->getBoolValue();
              item.state = b ? NSControlStateValueOn : NSControlStateValueOff;
            }
        }
        
    private:
        SGPropertyNode_ptr enableProp;
        SGPropertyNode_ptr checkedProp;
        NSMenuItem* item;
    };
} // of anonymous namespace

class FGCocoaMenuBar::CocoaMenuBarPrivate
{
public:
  CocoaMenuBarPrivate();
  ~CocoaMenuBarPrivate();
  
  void menuFromProps(NSMenu* menu, SGPropertyNode* menuNode);
  
  void fireBindingsForItem(NSMenuItem* item);
  
public:
  CocoaMenuDelegate* delegate;
  
  MenuItemBindings itemBindings;
    std::vector<CocoaItemListener*> listeners;
};

@interface CocoaMenuDelegate : NSObject <NSMenuDelegate> {
@private
  FGCocoaMenuBar::CocoaMenuBarPrivate* peer;
}

@property (nonatomic, assign) FGCocoaMenuBar::CocoaMenuBarPrivate* peer;
@end

@implementation CocoaMenuDelegate

@synthesize peer;

- (void) itemAction:(id) sender
{
  peer->fireBindingsForItem((NSMenuItem*) sender);
}

@end

static void setFunctionKeyShortcut(const std::string& shortcut, NSMenuItem* item)
{
    unichar shortcutChar = NSF1FunctionKey;
    if (shortcut == "F1") {
        shortcutChar = NSF1FunctionKey;
    } else if (shortcut == "F2") {
        shortcutChar = NSF2FunctionKey;
    } else if (shortcut == "F3") {
        shortcutChar = NSF3FunctionKey;
    } else if (shortcut == "F10") {
        shortcutChar = NSF10FunctionKey;
    } else if (shortcut == "F11") {
        shortcutChar = NSF11FunctionKey;
    } else if (shortcut == "F12") {
        shortcutChar = NSF12FunctionKey;
    } else {
        SG_LOG(SG_GENERAL, SG_WARN, "CocoaMenu:setFunctionKeyShortcut: unsupported:" << shortcut);
    }
    
  unichar ch[1];
  ch[0] = shortcutChar;
  [item setKeyEquivalentModifierMask:NSEventModifierFlagFunction];
  [item setKeyEquivalent:[NSString stringWithCharacters:ch length:1]];
  
}



static void setItemShortcutFromString(NSMenuItem* item, const string& s)
{
    std::string shortcut;
  
  bool hasCtrl = strutils::starts_with(s, "Ctrl-"); 
  bool hasShift = strutils::starts_with(s, "Shift-");
  bool hasAlt = strutils::starts_with(s, "Alt-");
  
  int offset = 0; // character offset from start of string
  if (hasShift) offset += 6;
  if (hasCtrl) offset += 5;
  if (hasAlt) offset += 4;
  
  shortcut = s.substr(offset);
  if (shortcut == "Esc") {
    shortcut = "\e";    
  }
  
    if ((shortcut.length() >= 2) && (shortcut[0] == 'F') && isdigit(shortcut[1])) {
        setFunctionKeyShortcut(shortcut, item);
        return;
    }

  shortcut = simgear::strutils::lowercase(shortcut);
  [item setKeyEquivalent:[NSString stringWithCString:shortcut.c_str() encoding:NSUTF8StringEncoding]];
  NSUInteger modifiers = 0;
  if (hasCtrl) modifiers |= NSEventModifierFlagControl;
  if (hasShift) modifiers |= NSEventModifierFlagShift;
  if (hasAlt) modifiers |= NSEventModifierFlagOption;
  
  [item setKeyEquivalentModifierMask:modifiers];
}

static bool doesBindingShowDialog(const SGBindingList& bindings) 
{
  auto it = std::find_if(bindings.begin(), bindings.end(), [](SGAbstractBinding_ptr binding) 
    {
      auto nab = dynamic_cast<SGBinding*>(binding.get());
      if (!nab) {
        return false;
      }

      return nab->getCommandName() == "dialog-show";
    });

    return it != bindings.end();
}

FGCocoaMenuBar::CocoaMenuBarPrivate::CocoaMenuBarPrivate()
{
  delegate = [[CocoaMenuDelegate alloc] init];
  delegate.peer = this;
}
  
FGCocoaMenuBar::CocoaMenuBarPrivate::~CocoaMenuBarPrivate()
{
  CocoaAutoreleasePool pool;
  [delegate release];
}
  
static bool labelIsSeparator(NSString* s)
{
  return [s hasPrefix:@"---"];
}
  
void FGCocoaMenuBar::CocoaMenuBarPrivate::menuFromProps(NSMenu* menu, SGPropertyNode* menuNode)
{
  int index = 0;
  for (SGPropertyNode_ptr n : menuNode->getChildren("item")) {
    if (!n->hasValue("enabled")) {
      n->setBoolValue("enabled", true);
    }
    
    SGBindingList bl = readBindingList(n->getChildren("binding"), globals->get_props());
    bool showsDialog = n->getBoolValue("shows-dialog") || doesBindingShowDialog(bl);

    string l = strutils::simplify(getLocalizedLabel(n));
    if (showsDialog && !strutils::ends_with(l, "...")) {
      l += u8"…";
    }

    NSString* label = stdStringToCocoa(l);
    string shortcut = n->getStringValue("key");
    
    NSMenuItem* item;
    if (index >= [menu numberOfItems]) {
      if (labelIsSeparator(label)) {
        item = [NSMenuItem separatorItem];
        [menu addItem:item];
      } else {        
        item = [menu addItemWithTitle:label action:nil keyEquivalent:@""];
        if (!shortcut.empty()) {
          setItemShortcutFromString(item, shortcut);
        }
      
        [item setTarget:delegate];
        [item setAction:@selector(itemAction:)];

        auto cl = new CocoaItemListener(n, item);
        listeners.push_back(cl);
      }
    } else {
      item = [menu itemAtIndex:index];
      [item setTitle:label]; 
    }

    auto subMenuNode = n->getChild("menu");
    if (subMenuNode) {
      NSMenu* subMenu = [[NSMenu alloc] init];
      menuFromProps(subMenu, subMenuNode);
      [item setSubmenu: subMenu];
    }
    
      
    itemBindings[item] = bl;    
    ++index;
  } // of item iteration
}

void FGCocoaMenuBar::CocoaMenuBarPrivate::fireBindingsForItem(NSMenuItem *item)
{
  MenuItemBindings::iterator it = itemBindings.find(item);
  if (it == itemBindings.end()) {
    return;
  }
 
    NSString* label = [item title];
    const auto s = stdStringFromCocoa(label);
    flightgear::addSentryBreadcrumb("fire menu item:" + s, "info");
  fireBindingList(it->second);
}

FGCocoaMenuBar::FGCocoaMenuBar() :
  p(new CocoaMenuBarPrivate)
{
  
}

FGCocoaMenuBar::~FGCocoaMenuBar()
{
    CocoaAutoreleasePool ap;
    NSMenu* mainBar = [[NSApplication sharedApplication] mainMenu];

    int num = [mainBar numberOfItems];
    for (int index=1; index < num; ++index) {
        NSMenuItem* topLevelItem = [mainBar itemAtIndex:index];
        [topLevelItem.submenu removeAllItems];
    }
    
    for (auto it = p->listeners.begin(); it != p->listeners.end(); ++it) {
        delete *it;
    }
    
    // owing to the bizarre destructor behaviour of SGBinding, we need
    // to explicitly clear these bindings. (PUIMenuBar takes a different
    // approach, and copies each binding into /sim/bindings)
    MenuItemBindings::iterator j;
    for (j = p->itemBindings.begin(); j != p->itemBindings.end(); ++j) {
        clearBindingList(j->second);
    }
}

void FGCocoaMenuBar::init()
{
  CocoaAutoreleasePool pool;
  
  NSMenu* mainBar = [[NSApplication sharedApplication] mainMenu];
  SGPropertyNode_ptr props = fgGetNode("/sim/menubar/default",true);
  
  int index = 0;
  NSMenuItem* previousMenu = [mainBar itemAtIndex:0];
  if (![[previousMenu title] isEqualToString:@"FlightGear"]) {
    [previousMenu setTitle:@"FlightGear"];
  }

  // clear other menus in the menu-bar, eg from the launcher
  while ([mainBar numberOfItems] > 1) {
    [mainBar removeItemAtIndex:1];
  }
  
  for (auto n : props->getChildren("menu")) {
    NSString* label = stdStringToCocoa(getLocalizedLabel(n));
    NSMenuItem* item = [mainBar itemWithTitle:label];
    NSMenu* menu;
    
    if (!item) {
      NSInteger insertIndex = [mainBar indexOfItem:previousMenu] + 1; 
      item = [mainBar insertItemWithTitle:label action:nil keyEquivalent:@"" atIndex:insertIndex];
      item.tag = index + 400;
      
      menu = [[NSMenu alloc] init];
      menu.title = label;
      [menu setAutoenablesItems:NO];
      [mainBar setSubmenu:menu forItem:item];
      [menu autorelease];
    } else {
      menu = item.submenu;
    }
    
  // synchronise menu with properties
    p->menuFromProps(menu, n);
    ++index;
    previousMenu = item;
    
  // track menu enable/disable state
    if (!n->hasValue("enabled")) {
      n->setBoolValue("enabled", true);
    }
    
    auto l = new CocoaItemListener( n, item);
    p->listeners.push_back(l);
  }
}

bool FGCocoaMenuBar::isVisible() const
{
  return true;
}

void FGCocoaMenuBar::show()
{
  // no-op
}

void FGCocoaMenuBar::hide()
{
  // no-op
}

void FGCocoaMenuBar::setHideIfOverlapsWindow(bool hide)
{
    SG_UNUSED(hide);
    // no-op
}

bool FGCocoaMenuBar::getHideIfOverlapsWindow() const
{
    return false;
}

