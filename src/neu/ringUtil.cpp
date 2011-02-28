// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: ringUtil.cpp,v 1.11 2008-06-08 00:06:00 wjiang Exp $]
//


#include "ring.h"
#include <math.h>  // for ceil()


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC DECLARATION
//____________________________________________________________________
static NET * pGlobalNet;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// UTILITY FUNCTIONS
//____________________________________________________________________
static bool RingCompare (NID n1, NID n2)
{
    return pGlobalNet->Compare(n1, n2);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS FUNCTIONS - BBS, SIGN, STAMP
//____________________________________________________________________
void NET::Sort (list<NID> * pList)
{
    pGlobalNet = this;
    pList->sort(RingCompare);
}


// append NID to signature
void SIGN::Append(const NID id, const SYNAP syn)
{
    // use full ASCII code to encode the signature
    const char ASCII[] = 
        {'0','1','2','3','4','5','6','7','8','9','-','=',
         '~','!','@','#','$','%','^','&','(',')','_','+',
         ';',':','<','>','?',',','.','/','[',']','{','}',
         'a','b','c','d','e','f','g','h','i','j','k','l','m',
         'n','o','p','q','r','s','t','u','v','w','x','y','z',
         'A','B','C','D','E','F','G','H','I','J','K','L','M',
         'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
        };
    const unsigned asize = sizeof(ASCII)/sizeof(char);
    // derive each digit by iterative division
    unsigned i=0;
    char buf[10];
    NID dig, quo, rem;
    dig = id;
    while (dig>0) {
        quo = dig/asize;
        rem = dig%asize;
        buf[i++] = ASCII[rem];
        dig = quo;
    }
    // use '*' to indicate delayed firing
    if (syn.Delayed()) {
        buf[i++] = '*';
    }
    buf[i] = '\0';
    _sig.append(buf);
}

void STAMP::Append(
    const NID   id,
    const SYNAP syn)
{
    // append to signature (parent function)
    SIGN::Append(id, syn);
    // append to NID vector
    _nids.push_back(id );
    _syns.push_back(syn);
    _strength += syn.Weight();
}

// return true if the signature contains delayed edge
bool STAMP::Delayed ()
{
    vector<SYNAP>::iterator it;
    foreachv (it, _syns) {
        if ((*it).Delayed()) {
            return true;
        }
    }
    return false;
}

// remove the delayed edge from signature; recompute strength
bool STAMP::RemoveDelay ()
{
    SIGN::Clear();
    _strength = 0;
    vector<SYNAP>::iterator sit;
    vector<NID  >::iterator nit;
    sit = _syns.begin();
    nit = _nids.begin();
    while (sit != _syns.end()) {
        if ((*sit).Delayed()) {
            _syns.erase(sit);
            _nids.erase(nit);
        } else {
            SIGN::Append(*nit, *sit);
            _strength += (*sit).Weight();
            nit++;
            sit++;
        }
    }
}

// sum of ages for all synapses in signature
unsigned STAMP::Age ()
{
    unsigned sum_age=0;
    vector<SYNAP>::iterator sit;
    for (sit=_syns.begin(); sit!=_syns.end(); sit++) {
        sum_age += (*sit).Age();
    }
    return sum_age;
}

void STAMP::Clear()
{ 
    SIGN :: Clear();
    _nids . clear();
    _syns . clear();
}


BBS::~BBS() 
{
    Release();
}


// release STAMP memory
void BBS::Release()
{
    for (_it=_board.begin();
         _it!=_board.end(); _it++) {
        delete (*_it).second;
    }
}


void BBS::Clear() 
{
    Release();
    _stamps . clear ();
    _board  . clear ();
}

// retrieve stamp associated with destination NID

bool BBS::GetStamp (
    NID     dst, 
    STAMP * &ps)
{
    bool fResult = false;
    hash_map<NID, STAMP *>::iterator its;
    its = _board.find(dst);
    if (its != _board.end()) {
        ps = (*its).second;
        fResult = true;
    }
    return fResult;
}

// record each source-destination pattern, 
// and their synapse strength 

bool BBS::Post(
    NID   src, 
    NID   dst,
    SYNAP synap)
{
    STAMP * pSt;
    hash_map<NID, STAMP *>::iterator its;
    its = _board.find(dst);
    if (its == _board.end()) {
        pSt = new STAMP;
        //_board.insert(pair<NID, STAMP*>(dst, pSt));
        _board[dst] = pSt;
    } else {
        pSt = (*its).second;
    }
    pSt->Append (src, synap);
    return true;
}

// record a combinational pattern from a delayed one

STAMP * BBS::PostComb (NID dst, STAMP *pst)
{
    // make a copy of the stamp, and remove delayed edge
    STAMP *pstnew = new STAMP(pst);
    pstnew->RemoveDelay();
    // post onto board
    hash_map<NID, STAMP *>::iterator its;
    its = _board.find(dst);
    if (_board.find(dst) == _board.end()) {
        _board[dst] = pstnew;
    } else {
        delete (*its).second;
        (*its).second = pstnew;
    }
    return pstnew;
}

// use hash table to pick best neuron candidate
// among the ones with same firing patterns;
// 0. remove neurons whose firing pattern does not match;
// 1. highest synap strength
// 2. for same strength, younger wins (encourages learning)
// 
// special handling for delayed links:
// - check if there is conficting pattern without delayed link;
// - if conflict exists, defer firing (remove from wave front)
// - else, keep firing
// 

bool BBS::Select(bool bVerbose) 
{
    // store combinational patterns derived from delayed ones;
    // may need to use HASH instead of hash_map to improve runtime.
    BBS bbsComb(_net);
    hash_str::iterator its;
    
    if (bVerbose) { cout << " :STAMP: "; }
    foreachv (_it, _board) {
        NID    nid1 = (*_it).first;
        STAMP *nst1 = (*_it).second;
        if (bVerbose) { cout << nst1->Cstr() << " "; }
        if (!_net.Get(nid1).Match(nst1)) {
            // firing pattern does not match internal
            if (bVerbose) { cout << "(MISS) "; }
            continue;
        }
        // store delay type of the signature
        bool d1=false,d2=false;
        // for new fired neuron with no existing signs,
        // remove delayed link for combinational pattern testing
        if (_net.Get(nid1).Sign().Empty() && nst1->Delayed()) {
            nst1 = bbsComb.PostComb (nid1, nst1);
            d1 = true;
        }
        its = _stamps.find(nst1->Cstr());
        if (its == _stamps.end()) {
            // first unique pattern; insert in hash
            _stamps[nst1->Cstr()] = nid1;
        } else {
            // existing pattern; compare synapse strength
            NID nid2; STAMP *nst2;
            nid2 = (*its).second;
            // retrieve combinational pattern first
            if (bbsComb.GetStamp (nid2, nst2)) {
                d2 = true;
            } else {
                nst2 = _board[nid2];
            }
            // output firing give up to delay firing
            bool win1 = (_net.Get(nid2).Type()==OUTPUT) && d1;
            bool win2 = (_net.Get(nid1).Type()==OUTPUT) && d2;
            if (win1  || (!win2 && Compare(nid1,nid2,nst1,nst2))) {
                _stamps.erase(its);
                _stamps[nst1->Cstr()] = nid1;
                if (bVerbose) {
                    cout << "("<< nid1 <<") ";
                }
            }
        }
    }
    if (bVerbose) { cout << endl; }
    
    // process delayed edges after uniquefication of combinational
    // patterns
    if (bbsComb.Size() > 0) {
        unsigned idx=0;
        foreachv (_it, _board) {
            NID    nid1 = (*_it).first;
            STAMP *nst1 = (*_it).second;
            if (_net.Get(nid1).Sign().Empty() &&
                nst1->Delayed()) {
                // restore delayed edge in BBS; 
                // (it must have been processed and stored earlier)
                STAMP *nst2;
                bool bRes = bbsComb.GetStamp (nid1,nst2);
                assert (bRes);
                its = _stamps.find(nst2->Cstr());
                if (its != _stamps.end() && (*its).second==nid1) {
                    // remove combinational pattern;
                    _stamps.erase(its);
                    // insert delayed pattern,
                    // only if one does not already exist
                    if (_stamps.find(nst1->Cstr())==_stamps.end()) {
                        _stamps[nst1->Cstr()] = nid1;
                    }
                }
                idx++;
            }
        }
        assert(idx==bbsComb.Size());
    }
    return true;
}

// first compare synaps strength; then synapse age
// for tie, compare neuron potential
bool BBS::Compare(
    NID    n1, 
    NID    n2,
    STAMP *s1, 
    STAMP *s2)
{
    bool bResult = false;
    int  d = s1->Strength() - s2->Strength();
    int  a = s1->Age() - s2->Age();
    if (d > 0) {
        bResult = true;
    } else if (d < 0) {
        bResult = false;
    } else {
        if (a != 0) {
            bResult = (a>0)? true : false;
        } else {
            bResult = _net.Compare(n1, n2);
        }
    }
    return bResult;
}

void BBS::IterBegin(bool f) 
{
    _fIterFiring = f;
    _it = _board.begin();
}

bool BBS::IterNext(NID &n)
{
    bool fResult=false;
    if (_it!=_board.end()) {
        hash_str::iterator sit;
        // find the NID associated with this pattern
        // _fIterFiring=1 : winners;
        // _fIterFiring=0 : losers;
        sit = _stamps.find(((*_it).second)->Cstr());
        if (sit == _stamps.end()) {
            // not qualified for firing due to patter mismatch
            if (_fIterFiring) {
                _it++; fResult = IterNext (n);
            } else {
                n = (*_it).first;
                _it++; fResult = true;
            }
        } else if (_fIterFiring ^ ((*_it).first==(*sit).second)) {
            // iter-flag differs with matching status
            _it++; fResult = IterNext (n);
        } else {
            // found matching
            n = (*_it).first;
            _it++; fResult = true;
        }
    }
    return fResult;
}

// erase losers from the board; 
// TODO: improve runtime performance later

void BBS::Filter()
{
    vector<NID> listid;
    _it = _board.begin();
    while (_it!=_board.end()) {
        hash_str::iterator sit;
        sit = _stamps.find(((*_it).second)->Cstr());
        if (sit == _stamps.end() || 
            ((*_it).first != (*sit).second)) {
            listid.push_back ((*_it).first);
        }
        _it ++;
    }
    vector<NID>::iterator it;
    foreachv (it, listid) {
        _board.erase (*it);
    }
}



