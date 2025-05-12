/*
 * SPDX-FileName: MPServerResolver.cxx
 * SPDX-FileComment: mpserver names lookup via DNS. This file is part of FlightGear.
 * SPDX-FileCopyrightText: Written and copyright by Torsten Dreyer - November 2016
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <algorithm>

#include <Network/DNSClient.hxx>
#include <Main/fg_props.hxx>
#include <cJSON.h>
#include <cstdlib>

#include "MPServerResolver.hxx"

using namespace simgear;

/**
 * Build a name=value map from base64 encoded JSON string
 */
class MPServerProperties : public std::map<std::string, std::string> {
public:
  MPServerProperties (std::string b64)
  {
    std::vector<unsigned char> b64dec;
    simgear::strutils::decodeBase64 (b64, b64dec);
    auto jsonString = std::string ((char*) b64dec.data (), b64dec.size ());
    cJSON * json = ::cJSON_Parse (jsonString.c_str ());
    if (json) {
      for (int i = 0; i < ::cJSON_GetArraySize (json); i++) {
        cJSON * cj = ::cJSON_GetArrayItem (json, i);
        if (cj->string && cj->valuestring)
          emplace (cj->string, cj->valuestring);
      }
      ::cJSON_Delete (json);
    } else {
      SG_LOG(SG_NETWORK,SG_WARN, "MPServerResolver: Can't parse JSON string '" << jsonString << "'" );
    }
  }
};

class MPServerResolver::MPServerResolver_priv {
public:
  enum {
    INIT, LOADING_SRV_RECORDS, LOAD_NEXT_TXT_RECORD, LOADING_TXT_RECORDS, DONE,
  } _state = INIT;

  FGDNSClient* _dnsClient = globals->get_subsystem<FGDNSClient>();
  DNS::Request_ptr _dnsRequest;
  PropertyList _serverNodes;
  PropertyList::const_iterator _serverNodes_it;
};

MPServerResolver::~MPServerResolver ()
{
    if (_priv->_dnsRequest) {
        _priv->_dnsRequest->cancel();
    }

  delete _priv;
}

MPServerResolver::MPServerResolver () :
    _priv (new MPServerResolver_priv ())
{
}

void
MPServerResolver::run ()
{
  //SG_LOG(SG_NETWORK, SG_DEBUG, "MPServerResolver::run() with state=" << _priv->_state );
  switch (_priv->_state) {
    // First call - fire DNS lookup for SRV records
    case MPServerResolver_priv::INIT:
      if (!_priv->_dnsClient) {
        SG_LOG(SG_NETWORK, SG_WARN, "MPServerResolver: DNS subsystem not available.");
        onFailure ();
        return;
      }

      _priv->_dnsRequest = new DNS::SRVRequest (_dnsName, _service, _protocol);
      SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: sending DNS request for " << _priv->_dnsRequest->getDn());
      _priv->_dnsClient->makeRequest (_priv->_dnsRequest);
      _priv->_state = MPServerResolver_priv::LOADING_SRV_RECORDS;
      break;

    // Check if response from SRV Query
    case MPServerResolver_priv::LOADING_SRV_RECORDS:
      if (_priv->_dnsRequest->isTimeout ()) {
        SG_LOG(SG_NETWORK, SG_WARN, "Timeout waiting for DNS response. Query was: " << _priv->_dnsRequest->getDn());
        onFailure ();
        return;
      }
      if (_priv->_dnsRequest->isComplete ()) {
        // Create a child node under _targetNode for each SRV entry of the response
        SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: got DNS response for " << _priv->_dnsRequest->getDn());
        int idx = 0;
        for (DNS::SRVRequest::SRV_ptr entry : dynamic_cast<DNS::SRVRequest*> (_priv->_dnsRequest.get ())->entries) {
          SG_LOG(SG_NETWORK, SG_DEBUG,
                 "MPServerResolver: SRV " << entry->priority << " " << entry->weight << " " << entry->port << " " << entry->target);
          if( 0 == entry->port ) {
            SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: Skipping offline host " << entry->target );
            continue;
          }
          SGPropertyNode * serverNode = _targetNode->getNode ("server", idx++, true);
          serverNode->getNode ("hostname", true)->setStringValue (entry->target);
          serverNode->getNode ("priority", true)->setIntValue (entry->priority);
          serverNode->getNode ("weight", true)->setIntValue (entry->weight);
          serverNode->getNode ("port", true)->setIntValue (entry->port);
        }

        // prepare an iterator over the server-nodes to be used later when loading the TXT records
        _priv->_serverNodes = _targetNode->getChildren ("server");
        _priv->_serverNodes_it = _priv->_serverNodes.begin ();
        if (_priv->_serverNodes_it == _priv->_serverNodes.end ()) {
          // No SRV records found - flag failure
          SG_LOG(SG_NETWORK, SG_WARN, "MPServerResolver: no multiplayer servers defined via DNS");
          onFailure ();
          return;
        }
        _priv->_state = MPServerResolver_priv::LOAD_NEXT_TXT_RECORD;
        break;
      }
      break;

    // get the next TXT record
    case MPServerResolver_priv::LOAD_NEXT_TXT_RECORD:
      if (_priv->_serverNodes_it == _priv->_serverNodes.end ()) {
        // we are done with all servers
        _priv->_state = MPServerResolver_priv::DONE;
        break;
      }

      // send the DNS query for the hostnames TXT record
      _priv->_dnsRequest = new DNS::TXTRequest ((*_priv->_serverNodes_it)->getStringValue ("hostname"));
      SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: sending DNS request for " << _priv->_dnsRequest->getDn());
      _priv->_dnsClient->makeRequest (_priv->_dnsRequest);
      _priv->_state = MPServerResolver_priv::LOADING_TXT_RECORDS;
      break;

    // check if response for TXT query
    case MPServerResolver_priv::LOADING_TXT_RECORDS:
      if (_priv->_dnsRequest->isTimeout ()) {
        // on timeout, try proceeding with next server
        SG_LOG(SG_NETWORK, SG_WARN, "Timeout waiting for DNS response. Query was: " << _priv->_dnsRequest->getDn());
        _priv->_state = MPServerResolver_priv::LOAD_NEXT_TXT_RECORD;
        ++_priv->_serverNodes_it;
        break;
      }
      if (_priv->_dnsRequest->isComplete ()) {
        SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: got DNS response for " << _priv->_dnsRequest->getDn());
        // DNS::TXTRequest automatically extracts name=value entries for us, lets retrieve them
        auto attributes = dynamic_cast<DNS::TXTRequest*> (_priv->_dnsRequest.get ())->attributes;
        auto mpserverAttribute = attributes["flightgear-mpserver"];
        if (!mpserverAttribute.empty ()) {
          // we are only interested in the 'flightgear-mpserver=something' entry, this is a base64 encoded
          // JSON string, convert this into a map<string,string>
          MPServerProperties mpserverProperties (mpserverAttribute);
          for (auto prop : mpserverProperties) {
            // and store each as a node under our servers node.
            SG_LOG(SG_NETWORK, SG_DEBUG, "MPServerResolver: TXT record attribute " << prop.first << "=" << prop.second);
            // sanitize property name, don't allow dots or forward slash
            auto propertyName = prop.first;
            std::replace( propertyName.begin(), propertyName.end(), '.', '_');
            std::replace( propertyName.begin(), propertyName.end(), '/', '_');
            (*_priv->_serverNodes_it)->setStringValue (propertyName, prop.second);
          }
        } else {
          SG_LOG(SG_NETWORK, SG_INFO, "MPServerResolver: TXT record attributes empty");
        }

        // procede with the net node
        ++_priv->_serverNodes_it;
        _priv->_state = MPServerResolver_priv::LOAD_NEXT_TXT_RECORD;
        break;
      }
      break;

    case MPServerResolver_priv::DONE:
        _priv->_dnsRequest.clear();
        onSuccess();
        return;
  }

  // Relinguish control, call me back on the next frame
  globals->get_event_mgr ()->addEvent ("MPServerResolver_update", [this](){ this->run(); }, .0);
}

