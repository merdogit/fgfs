/*
 * SPDX-FileName: AddonMetadataParser.cxx
 * SPDX-FileComment: Parser for FlightGear add-on metadata files
 * SPDX-FileCopyrightText: Copyright (C) 2018  Florent Rougon
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <regex>
#include <string>
#include <tuple>
#include <vector>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>

#include "addon_fwd.hxx"
#include "AddonMetadataParser.hxx"
#include "AddonVersion.hxx"
#include "contacts.hxx"
#include "exceptions.hxx"
#include "pointer_traits.hxx"

#include <Main/globals.hxx>
#include <Main/locale.hxx>

namespace strutils = simgear::strutils;

using std::string;
using std::vector;

namespace flightgear::addons
{

// Static method
SGPath
Addon::MetadataParser::getMetadataFile(const SGPath& addonPath)
{
  return addonPath / "addon-metadata.xml";
}

static std::string getMaybeLocalized(const std::string& tag, SGPropertyNode* base, SGPropertyNode* lang)
{
    if (lang) {
        auto n = lang->getChild(tag);
        if (n) {
            return strutils::strip(n->getStringValue());
        }
    }

    auto n = base->getChild(tag);
    if (n) {
        return strutils::strip(n->getStringValue());
    }

    return {};
}

static SGPropertyNode* getAndCheckLocalizedNode(SGPropertyNode* addonNode,
                                                const SGPath& metadataFile)
{
    const auto localizedNode = addonNode->getChild("localized");
    if (!localizedNode) {
        return nullptr;
    }

    for (int i = 0; i < localizedNode->nChildren(); ++i) {
        const auto node = localizedNode->getChild(i);
        const std::string& name = node->getNameString();

        if (name.find('_') != std::string::npos) {
            throw errors::error_loading_metadata_file(
                "underscores not allowed in names of children of <localized> "
                "(in add-on metadata file '" + metadataFile.utf8Str() + "'); "
                "hyphens should be used, as in 'fr-FR' or 'en-GB'");
        }
    }

    return localizedNode;
}

// Static method
Addon::Metadata
Addon::MetadataParser::parseMetadataFile(const SGPath& addonPath)
{
  SGPath metadataFile = getMetadataFile(addonPath);
  SGPropertyNode addonRoot;
  Addon::Metadata metadata;

  if (!metadataFile.exists()) {
    throw errors::no_metadata_file_found(
      "unable to find add-on metadata file '" + metadataFile.utf8Str() + "'");
  }

  try {
    readProperties(metadataFile, &addonRoot);
  } catch (const sg_exception &e) {
    throw errors::error_loading_metadata_file(
      "unable to load add-on metadata file '" + metadataFile.utf8Str() + "': " +
      e.getFormattedMessage());
  }

  // Check the 'meta' section
  SGPropertyNode *metaNode = addonRoot.getChild("meta");
  if (metaNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /meta node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }

  // Check the file type
  SGPropertyNode *fileTypeNode = metaNode->getChild("file-type");
  if (fileTypeNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /meta/file-type node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }

  std::string fileType = fileTypeNode->getStringValue();
  if (fileType != "FlightGear add-on metadata") {
    throw errors::error_loading_metadata_file(
      "Invalid /meta/file-type value for add-on metadata file '" +
      metadataFile.utf8Str() + "': '" + fileType + "' "
      "(expected 'FlightGear add-on metadata')");
  }

  // Check the format version
  SGPropertyNode *fmtVersionNode = metaNode->getChild("format-version");
  if (fmtVersionNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /meta/format-version node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }

  int formatVersion = fmtVersionNode->getIntValue();
  if (formatVersion != 1) {
    throw errors::error_loading_metadata_file(
      "unknown format version in add-on metadata file '" +
      metadataFile.utf8Str() + "': " + std::to_string(formatVersion));
  }

  // Now the data we are really interested in
  SGPropertyNode *addonNode = addonRoot.getChild("addon");
  if (addonNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /addon node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }

  const auto localizedNode = getAndCheckLocalizedNode(addonNode, metadataFile);
  SGPropertyNode* langStringsNode = globals->get_locale()->selectLanguageNode(localizedNode);

  SGPropertyNode *idNode = addonNode->getChild("identifier");
  if (idNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /addon/identifier node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }
  metadata.id = strutils::strip(idNode->getStringValue());

  // Require a non-empty identifier for the add-on
  if (metadata.id.empty()) {
    throw errors::error_loading_metadata_file(
      "empty or whitespace-only value for the /addon/identifier node in "
      "add-on metadata file '" + metadataFile.utf8Str() + "'");
  } else if (metadata.id.find('.') == std::string::npos) {
    SG_LOG(SG_GENERAL, SG_WARN,
           "Add-on identifier '" << metadata.id << "' does not use reverse DNS "
           "style (e.g., org.flightgear.addons.MyAddon) in add-on metadata "
           "file '" << metadataFile.utf8Str() + "'");
  }

  SGPropertyNode *nameNode = addonNode->getChild("name");
  if (nameNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /addon/name node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }

  metadata.name = getMaybeLocalized("name", addonNode, langStringsNode);

  // Require a non-empty name for the add-on
  if (metadata.name.empty()) {
    throw errors::error_loading_metadata_file(
      "empty or whitespace-only value for the /addon/name node in add-on "
      "metadata file '" + metadataFile.utf8Str() + "'");
  }

  SGPropertyNode *versionNode = addonNode->getChild("version");
  if (versionNode == nullptr) {
    throw errors::error_loading_metadata_file(
      "no /addon/version node found in add-on metadata file '" +
      metadataFile.utf8Str() + "'");
  }
  metadata.version = AddonVersion{
    strutils::strip(versionNode->getStringValue())};

  metadata.authors = parseContactsNode<Author>(metadataFile,
                                               addonNode->getChild("authors"));
  metadata.maintainers = parseContactsNode<Maintainer>(
    metadataFile, addonNode->getChild("maintainers"));

  metadata.shortDescription = getMaybeLocalized("short-description", addonNode, langStringsNode);
  metadata.longDescription = getMaybeLocalized("long-description", addonNode, langStringsNode);

  std::tie(metadata.licenseDesignation, metadata.licenseFile,
           metadata.licenseUrl) = parseLicenseNode(addonPath, addonNode);

  SGPropertyNode *tagsNode = addonNode->getChild("tags");
  if (tagsNode != nullptr) {
    auto tagNodes = tagsNode->getChildren("tag");
    for (const auto& node: tagNodes) {
      metadata.tags.push_back(strutils::strip(node->getStringValue()));
    }
  }

  SGPropertyNode *minNode = addonNode->getChild("min-FG-version");
  if (minNode != nullptr) {
    metadata.minFGVersionRequired = strutils::strip(minNode->getStringValue());
  } else {
    metadata.minFGVersionRequired = std::string();
  }

  SGPropertyNode *maxNode = addonNode->getChild("max-FG-version");
  if (maxNode != nullptr) {
    metadata.maxFGVersionRequired = strutils::strip(maxNode->getStringValue());
  } else {
    metadata.maxFGVersionRequired = std::string();
  }

  metadata.homePage = metadata.downloadUrl = metadata.supportUrl =
    metadata.codeRepositoryUrl = std::string(); // defaults
  SGPropertyNode *urlsNode = addonNode->getChild("urls");
  if (urlsNode != nullptr) {
    SGPropertyNode *homePageNode = urlsNode->getChild("home-page");
    if (homePageNode != nullptr) {
      metadata.homePage = strutils::strip(homePageNode->getStringValue());
    }

    SGPropertyNode *downloadUrlNode = urlsNode->getChild("download");
    if (downloadUrlNode != nullptr) {
      metadata.downloadUrl = strutils::strip(downloadUrlNode->getStringValue());
    }

    SGPropertyNode *supportUrlNode = urlsNode->getChild("support");
    if (supportUrlNode != nullptr) {
      metadata.supportUrl = strutils::strip(supportUrlNode->getStringValue());
    }

    SGPropertyNode *codeRepoUrlNode = urlsNode->getChild("code-repository");
    if (codeRepoUrlNode != nullptr) {
      metadata.codeRepositoryUrl =
        strutils::strip(codeRepoUrlNode->getStringValue());
    }
  }

  SG_LOG(SG_GENERAL, SG_DEBUG,
         "Parsed add-on metadata file: '" << metadataFile.utf8Str() + "'");

  return metadata;
}

// Utility function for Addon::MetadataParser::parseContactsNode<>()
//
// Read a node such as "name", "email" or "url", child of a contact node (e.g.,
// of an "author" or "maintainer" node).
static std::string
parseContactsNode_readNode(const SGPath& metadataFile,
                           SGPropertyNode* contactNode,
                           std::string subnodeName, bool allowEmpty)
{
  SGPropertyNode *node = contactNode->getChild(subnodeName);
  std::string contents;

  if (node != nullptr) {
    contents = simgear::strutils::strip(node->getStringValue());
  }

  if (!allowEmpty && contents.empty()) {
    throw errors::error_loading_metadata_file(
      "in add-on metadata file '" + metadataFile.utf8Str() + "': "
      "when the node " + contactNode->getPath(true) + " exists, it must have "
      "a non-empty '" + subnodeName + "' child node");
  }

  return contents;
};

// Static method template (private and only used in this file)
template <class T>
vector<typename contact_traits<T>::strong_ref>
Addon::MetadataParser::parseContactsNode(const SGPath& metadataFile,
                                         SGPropertyNode* mainNode)
{
  using contactTraits = contact_traits<T>;
  vector<typename contactTraits::strong_ref> res;

  if (mainNode != nullptr) {
    auto contactNodes = mainNode->getChildren(contactTraits::xmlNodeName());
    res.reserve(contactNodes.size());

    for (const auto& contactNode: contactNodes) {
      std::string name, email, url;

      name = parseContactsNode_readNode(metadataFile, contactNode.get(),
                                        "name", false /* allowEmpty */);
      email = parseContactsNode_readNode(metadataFile, contactNode.get(),
                                         "email", true);
      url = parseContactsNode_readNode(metadataFile, contactNode.get(),
                                       "url", true);

      using ptr_traits = shared_ptr_traits<typename contactTraits::strong_ref>;
      res.push_back(ptr_traits::makeStrongRef(name, email, url));
    }
  }

  return res;
};

// Static method
std::tuple<std::string, SGPath, std::string>
Addon::MetadataParser::parseLicenseNode(const SGPath& addonPath,
                                        SGPropertyNode* addonNode)
{
  SGPath metadataFile = getMetadataFile(addonPath);
  std::string licenseDesignation;
  SGPath licenseFile;
  std::string licenseUrl;

  SGPropertyNode *licenseNode = addonNode->getChild("license");
  if (licenseNode == nullptr) {
    return std::tuple<std::string, SGPath, std::string>();
  }

  SGPropertyNode *licenseDesigNode = licenseNode->getChild("designation");
  if (licenseDesigNode != nullptr) {
    licenseDesignation = strutils::strip(licenseDesigNode->getStringValue());
  }

  SGPropertyNode *licenseFileNode = licenseNode->getChild("file");
  if (licenseFileNode != nullptr) {
    // This effectively disallows filenames starting or ending with whitespace
    std::string licenseFile_s = strutils::strip(licenseFileNode->getStringValue());

    if (!licenseFile_s.empty()) {
      if (licenseFile_s.find('\\') != std::string::npos) {
        throw errors::error_loading_metadata_file(
          "in add-on metadata file '" + metadataFile.utf8Str() + "': the "
          "value of /addon/license/file contains '\\'; please use '/' "
          "separators only");
      }

      if (licenseFile_s.find_first_of("/\\") == 0) {
        throw errors::error_loading_metadata_file(
          "in add-on metadata file '" + metadataFile.utf8Str() + "': the "
          "value of /addon/license/file must be relative to the add-on folder, "
          "however it starts with '" + licenseFile_s[0] + "'");
      }

#ifdef HAVE_WORKING_STD_REGEX
      std::regex winDriveRegexp("([a-zA-Z]:).*");
      std::smatch results;

      if (std::regex_match(licenseFile_s, results, winDriveRegexp)) {
        std::string winDrive = results.str(1);
#else // all this 'else' clause should be removed once we actually require C++11
      if (licenseFile_s.size() >= 2 &&
          (('a' <= licenseFile_s[0] && licenseFile_s[0] <= 'z') ||
           ('A' <= licenseFile_s[0] && licenseFile_s[0] <= 'Z')) &&
          licenseFile_s[1] == ':') {
        std::string winDrive = licenseFile_s.substr(0, 2);
#endif
        throw errors::error_loading_metadata_file(
          "in add-on metadata file '" + metadataFile.utf8Str() + "': the "
          "value of /addon/license/file must be relative to the add-on folder, "
          "however it starts with a Windows drive letter (" + winDrive + ")");
      }

      licenseFile = addonPath / licenseFile_s;
      if ( !(licenseFile.exists() && licenseFile.isFile()) ) {
        throw errors::error_loading_metadata_file(
          "in add-on metadata file '" + metadataFile.utf8Str() + "': the "
          "value of /addon/license/file (pointing to '" + licenseFile.utf8Str() +
          "') doesn't correspond to an existing file");
      }
    } // of if (!licenseFile_s.empty())
  }   // of if (licenseFileNode != nullptr)

  SGPropertyNode *licenseUrlNode = licenseNode->getChild("url");
  if (licenseUrlNode != nullptr) {
    licenseUrl = strutils::strip(licenseUrlNode->getStringValue());
  }

  return std::make_tuple(licenseDesignation, licenseFile, licenseUrl);
}

} // of namespace flightgear::addons
