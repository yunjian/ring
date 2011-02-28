// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: ringNeuron.cpp,v 1.5 2008-04-27 00:22:01 wjiang Exp $]
//

#include "ring.h"




//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS NEURON  MEMBER FUNCTIONS
//____________________________________________________________________


// make links to target neurons 

void NET::Connect(
    NEURON     &neu, 
    list<NID> *qTargets, 
    bool       bDelay) 
{
    // competition: 
    // 1. if more targets than it can pay attention,
    //    choose most well-connected targets;
    //    only the nodes with highest energy survive!
    // 2. limited energy will be distributed among links 
    //    based on link-strength;
    if (neu.Type() == OUTPUT) {
        return;
    }
    list<NID>::iterator it;
    for (it=qTargets->begin(); it!=qTargets->end(); it++) {
        // avoid self-loop
        if ((*it) == neu.Id()) {
            continue;
        }
        // avoid direct loop :
        // if A->B, then B cannot link to A
        if (Get(*it).Linked(neu.Id())) {
            continue;
        }
        neu.Link(*it, bDelay);
    }
}

// connect or strengthen with given neuron

void NEURON::Link(NID nid, bool bDelay) 
{
    map<NID,SYNAP>::iterator itConn;
    if ((itConn=_links.find(nid)) == _links.end()) {
        // make link if not already
        _links.insert(pair<NID,SYNAP>(nid,SYNAP(bDelay)));
    } else {
        // strengthened based if delay flag matches
        if ((*itConn).second.Delayed() == bDelay) {
            (*itConn).second.Strengthen();
        }
    }
}

// reset neuron state based on previous recording
void NEURON::StateReset(METASTATE s)
{
    _flag  =s.u.s._flag; 
    _state =s.u.s._state; 
    _potent=s.u.s._potent;
}


// cool down neuron activity
// reset to zero if reaches QUIET state
void NEURON::Cool()
{
    PotentialReduce();
    FlagReset(FLAG_FIRING);
    switch (_state) {
    case QUIET:   break;
    case AWAKE:   _state = QUIET; _potent=0; break;
    case MELLOW:  _state = AWAKE;  break;
    case HYPER:   _state = MELLOW; break;
    default:      break;
    }
}


// reset potential to zero
void NEURON::PotentialReset()
{
    _potent = 0;
}


// linearly reduce potential
void NEURON::PotentialReduce()
{
    _potent /= 2;
    //float ratio = ((float)(_state - QUIET))/((float)(_state + 1));
    //_potent =  (int) (ratio * (float)_potent);
}

// add given potential;
// return true if state changed to HYPER;

bool NEURON::Excite(NEU_POTENT pot)
{
    PotentialAdd(pot);
    if (Potential() > THRESH_BASE && _state != HYPER ) {
        _state = HYPER;
    }
    return (_state == HYPER);
}


// remove link attached to destination neuron

bool NEURON::LinkRemove(NID id)
{
    bool fResult = false;
    map<NID,SYNAP>::iterator itConn;
    if ((itConn=_links.find(id)) != _links.end()) {
        fResult = true;
        _links.erase(itConn);
        //_links.erase(id); // extra runtime to find item
    }
    return fResult;
}

// weaken link strength attached to destination neuron

bool NEURON::LinkWeaken(NID id, bool bDelayed)
{
    bool fResult = false;
    map<NID,SYNAP>::iterator itConn;
    if ((itConn=_links.find(id)) != _links.end()) {
        fResult = true;
        if ((*itConn).second.Active() &&
            (*itConn).second.Delayed() == bDelayed) {
            (*itConn).second.Weaken();
            if ((*itConn).second.Weight() == 0) {
                _links.erase(itConn);
            }
        }
    }
    return fResult;
}

bool NEURON::LinkDeactive(NID id, bool bDelayed)
{
    bool fResult = false;
    map<NID,SYNAP>::iterator itConn;
    if ((itConn=_links.find(id)) != _links.end()) {
        fResult = true;
        if ((*itConn).second.Active() &&
            (*itConn).second.Delayed() == bDelayed) {
            (*itConn).second.Deactive();
        }
    }
    return fResult;
}
