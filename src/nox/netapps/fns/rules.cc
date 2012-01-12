/* Copyright 2011 Daniel Turull (KTH) <danieltt@kth.se>
 *
 * This file is part of NOX.
 *
 * NOX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NOX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NOX.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "rules.hh"
#include <boost/functional/hash.hpp>

FNSRule::FNSRule(uint64_t sw_id, ofp_match match1) :
	sw_id(sw_id) {
	memcpy(&match, &match1, sizeof(match));
}

/*Epoint class*/

EPoint::EPoint(uint64_t ep_id, uint32_t in_port, uint16_t vlan,
		uint64_t fns_uuid) :
	ep_id(ep_id), in_port(in_port), vlan(vlan), fns_uuid(fns_uuid) {
	key = generate_key(ep_id, in_port, vlan, 0);
	mpls = 0;
}
EPoint::EPoint(uint64_t ep_id, uint32_t in_port, uint16_t vlan,
		uint64_t fns_uuid, uint32_t mpls) :
	ep_id(ep_id), in_port(in_port), vlan(vlan), fns_uuid(fns_uuid), mpls(mpls) {
	key = generate_key(ep_id, in_port, vlan, mpls);
}

void EPoint::addRule(boost::shared_ptr<FNSRule> r) {
	installed_rules.push_back(r);
}
int EPoint::num_installed() {
	return installed_rules.size();
}
boost::shared_ptr<FNSRule> EPoint::getRuleBack() {
	return installed_rules.back();
}
void EPoint::installed_pop() {
	installed_rules.pop_back();
}

uint64_t EPoint::generate_key(uint64_t sw_id, uint32_t port, uint16_t vlan,
		uint32_t mpls) {
	size_t seed = 0;
	boost::hash_combine(seed, port);
	boost::hash_combine(seed, sw_id);
	boost::hash_combine(seed, vlan);
	boost::hash_combine(seed, mpls);
	return seed;
}

FNS::FNS(uint64_t uuid) :
	uuid(uuid) {

}

uint64_t FNS::getUuid() {
	return uuid;
}

void FNS::addEPoint(boost::shared_ptr<EPoint> ep) {
	//	printf("Adding ep: %d %d\n",ep->ep_id, ep->in_port);
	epoints.push_back(ep);
}

int FNS::removeEPoint(boost::shared_ptr<EPoint> ep) {
	ep->fns_uuid = 0;
	epoints.erase(std::remove(epoints.begin(), epoints.end(), ep),
			epoints.end());

	return 0;
}

int FNS::numEPoints() {
	return epoints.size();
}
boost::shared_ptr<EPoint> FNS::getEPoint(int pos) {
	return epoints.at(pos);
}

/*RulesDB class*/
uint64_t RulesDB::addEPoint(endpoint* ep, boost::shared_ptr<FNS> fns) {
	boost::shared_ptr<EPoint> epoint = boost::shared_ptr<EPoint>(new EPoint(
			ep->swId, ep->port, ep->vlan, fns->getUuid(),ep->mpls));
	//	printf("Adding %ld\n",ep->id);
	boost::shared_ptr<EPoint> node = getEpoint(epoint->key);
	if (node == NULL) {
		endpoints.insert(pair<uint64_t, boost::shared_ptr<EPoint> > (
				epoint->key, epoint));
		fns->addEPoint(getEpoint(epoint->key));
		return epoint->key;
	} else {
		return 0;
	}
}
void RulesDB::removeEPoint(uint64_t key) {
	endpoints.erase(key);
}

boost::shared_ptr<EPoint> RulesDB::getEpoint(uint64_t id) {
	//	printf("# endpoints: %d\n",endpoints.size());
	if (endpoints.size() == 0) {
		return boost::shared_ptr<EPoint>();
	}
	map<uint64_t, boost::shared_ptr<EPoint> >::iterator epr =
			endpoints.find(id);
	if (endpoints.end() == epr)
		return boost::shared_ptr<EPoint>();
	return epr->second;
}

boost::shared_ptr<FNS> RulesDB::addFNS(fnsDesc* fns1) {
	boost::shared_ptr<FNS> fns = boost::shared_ptr<FNS>(new FNS(fns1->uuid));
	fnsList.insert(pair<uint64_t, boost::shared_ptr<FNS> > (fns1->uuid, fns));
	return getFNS(fns1->uuid);
}

boost::shared_ptr<FNS> RulesDB::getFNS(uint64_t uuid) {
	map<uint64_t, boost::shared_ptr<FNS> >::iterator fns1 = fnsList.find(uuid);
	if (fnsList.end() == fns1)
		return boost::shared_ptr<FNS>();
	return fns1->second;
}

void RulesDB::removeFNS(uint64_t uuid) {
	fnsList.erase(uuid);

}

/**
 * Locator class
 */

bool Locator::validateAddr(vigil::ethernetaddr addr) {
	if (addr.is_multicast() || addr.is_broadcast() || addr.is_zero())
		return false;
	/*Check if ethernetaddr exists*/
	if (clients.size() == 0)
		return true;
		map<vigil::ethernetaddr, boost::shared_ptr<EPoint> >::iterator epr =
				clients.find(addr);
	if (clients.end() == epr) {
			return true;
	}

	return false;
}

bool Locator::insertClient(vigil::ethernetaddr addr,
		boost::shared_ptr<EPoint> ep) {
	if (!validateAddr(addr))
		return false;

	map<vigil::ethernetaddr, boost::shared_ptr<EPoint> >::iterator epr =
			clients.find(addr);
	if (clients.end() != epr) {
		/* Update value */
		epr->second = ep;
	}else{
		/* Add value */
		clients.insert(pair<vigil::ethernetaddr, boost::shared_ptr<EPoint> > (addr,
			ep));
	}

	return true;
}


boost::shared_ptr<EPoint> Locator::getLocation(vigil::ethernetaddr addr) {
	if (clients.size() == 0) {
		return boost::shared_ptr<EPoint>();
	}
	map<vigil::ethernetaddr, boost::shared_ptr<EPoint> >::iterator epr =
			clients.find(addr);
	if (clients.end() == epr)
		return boost::shared_ptr<EPoint>();
	return boost::shared_ptr<EPoint>(epr->second);
}

void Locator::printLocations() {
	map<vigil::ethernetaddr, boost::shared_ptr<EPoint> >::iterator it;
	printf("LOACATOR DB:\n");
	printf("num of entries: %d\n", (int) clients.size());
	for (it = clients.begin(); it != clients.end(); it++) {
		printf("%s -> %d p:%d\n", it->first.string().c_str(),
				(int) it->second->ep_id, (int) it->second->in_port);
	}

}
