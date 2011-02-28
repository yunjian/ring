// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: ringNet.cpp,v 1.12 2008-06-08 00:05:59 wjiang Exp $]
//

#include "ring.h"



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC FUNCTIONS DEFINED IN THIS FILE
//____________________________________________________________________
enum LINK_PROCESS_TYPE { LINK_WEAKEN, LINK_DEACTIVE };

static int  netGetNumLevels_rec(NET &, NEURON &);
static void netLevelize_rec (NET &,NEURON &,vector<NID> *);
static void netProcessUniquePattern (NET &,BBS &);
static void netProcessStampLinks(NET &,STAMP &,NID,LINK_PROCESS_TYPE);
static void netReportQueue     (NET &, list<NID> *);
static void netPushFiringQueue (NET &, NID, list<NID> *);
static void netPushBakingQueue (NET &, NID, list<NID> &);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// UTILITY FUNCTIONS
//____________________________________________________________________
static void RingMsg(int id, const char *msg)
{
    cout << "RI-";
    cout.width(4);
    cout.setf(ios_base::left);
    cout << id << msg << endl;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS NET  MEMBER FUNCTIONS
//____________________________________________________________________
NET::NET (NID inc)
    : RSIZE(1),
      ISIZE(inc),          // input size
      OSIZE(inc*20),       // output size
      NSIZE(inc*100),      // internal neuron size
      TSIZE(OSIZE+NSIZE),  // total neuron size
      _neurons (new NEURON [TSIZE]),
      _inputs  (new NID [ISIZE]),
      _outputs (new NID [OSIZE]),
      _nextOutput(0),_bbs(*this),_time(0),_verbose(3)
{
    int i;
    foreach (i,0,ISIZE) {
        _inputs[i] = i;
        _neurons[i].Id(i);
        _neurons[i].Type(INPUT);
    }
    foreach (i,ISIZE,NSIZE) {
        _neurons[i].Id(i);
        _neurons[i].Type(INTERNAL);
    }
    foreach (i,0,OSIZE) {
        _outputs[i] = NSIZE+i;
        _neurons[NSIZE+i].Id(NSIZE+i);
        _neurons[NSIZE+i].Type(OUTPUT);
    }
    _firingPrio = new list<NID>;
    _firingCurr = new list<NID>;
    _firingWavf = new list<NID>;
    _firingWavb = new list<NID>;
    srand ( 1234567 /*time(NULL)*/ );
}
NET::~NET ()
{
    delete _firingPrio;
    delete _firingCurr;
    delete _firingWavf;
    delete _firingWavb;
}


// update input-neuron state based on stimuli
void NET::Input (PAD stimuli) 
{
    int i;
    foreach (i,0,ISIZE) {
        if (stimuli[i]) {
            Get(_inputs[i]).Excite(NEURON::EXCITE_HIGH);
        } else {
            Get(_inputs[i]).State(NEURON::QUIET);
        }
    }
}

// update input-neuron state based on image pad
void NET::Input (IPAD &ipad) 
{
    unsigned x,y,i;
    i=0;
    foreach (y,0,ipad.height()) {
        foreach (x,0,ipad.width()) {
            if (ipad.Pix(x,y)) {
                Get(_inputs[i]).Excite(NEURON::EXCITE_HIGH);
            } else {
                Get(_inputs[i]).State(NEURON::QUIET);
            }
            i++;
        }
    }
}

// train the network with animation movie
void NET::Train (IMOV &imov)
{
    // foreach IPAD frame
    vector<IPAD*>::iterator it;
    foreachv (it, imov.ipads()) {
        // extract pixels 
        Input(*(*it));
        // update neural network
        Update();
    }
}

// update internal-neuron states based on input neurons
void NET::Update () 
{
    int i;
    RingMsg(_time, "...... ......");
    
    RandomFire();
    RealFire(FIRING_INPUT);
    
    // do the same for excited neurons; propagate wave-front
    // in multiple iteration; process unique patterns in each front.
    list<NID>::iterator it;
    while (ProcessFiringQueue()) {
        RealFire(FIRING_CURRENT);
        if (_verbose >= 3) {
            netReportQueue ((*this), _firingCurr);
        }
    }
    
    Cool();
    Report();
    Advance();
}


// first connect; then propagate energy
bool NET::Update(
    NEURON    &neu,     // firing neuron
    list<NID> *qFiring, // target firing queue for excited neurons
    bool       bDelay)  // whether its delayed firing
{
    bool bResult=false;
    // propagate only when energy level is larger than threshold_2
    if (neu.State()     != NEURON::QUIET &&
        neu.Potential() >  NEURON::THRESH_HIGH) {
        // - make connection in temporary firing
        // - if links not yet saturated
        if (!qFiring && neu.LinkCount() < NEURON::MAX_SYNAP) {
            Connect (neu, _firingPrio, bDelay);
        }
        // mark status if propagation actually happened
        if (Propagate (neu, qFiring, bDelay)) {
            neu.FlagSet(NEURON::FLAG_IGNITING);
            // According to law of constant energy, the potential
            // should reset to zero, after propagating to
            // neighbors. However, this causes occilation.
            // Thus use reduce rather than reset.
            //neu.PotentialReduce();
        }
        bResult = true;
    }
    return bResult;
}


// propagate energy among links;
// - if excited any neuron then include it in firing queue;
// - return true if energy got dispatched through links;
// - check delayed edge with delay-firing status
// 
bool NET::Propagate(
    NEURON    &neu, 
    list<NID> *qFiring,
    bool       bDelay)
{
    // compute all link-strength;
    // linearly distribute energy among links based on its strength
    bool bPropagated = false;
    unsigned uTotal  = 0;
    unsigned uEnergy = neu.Potential();
    map<NID,SYNAP>& mL=neu.Links();
    map<NID,SYNAP>::iterator it;
    // propagate energy only to those winners of the temporary firing
    foreachv (it, mL) {
        if ((*it).second.Delayed()==bDelay && 
            (!qFiring || _bbs.Exists((*it).first))) {
            uTotal += (*it).second.Weight();
        }
    }
    foreachv (it, mL) {
        if ((*it).second.Delayed()!=bDelay ||
            !(!qFiring || _bbs.Exists((*it).first))) {
            continue;
        }
        NEURON &neu2 = Get((*it).first);
        float ratio = (((float)(*it).second.Weight())/((float)uTotal));
        unsigned uShare = (unsigned)((float)uEnergy * ratio);
        StateRegister(neu2);
        // record activity on the link through aging
        (*it).second.Aging();
        // for temporary firing, qFiring==NIL;
        // for real firing, push excited neurons into queue
        if (neu2.Excite(uShare) && qFiring) {
            netPushFiringQueue ((*this), neu2.Id(), qFiring);
            bPropagated = true;
        }
        // register the firing pattern in temporary firing
        if (!qFiring) {
            _bbs.Post(neu.Id(), neu2.Id(), (*it).second);
        }
    }
    return bPropagated;
}

// process the firing wave-front and wave-back queues
bool NET::ProcessFiringQueue ()
{
    list<NID> * listTmp;
    list<NID>::iterator it;
    // remove neurons that have been filtered out during firing
    it = _firingWavb->begin();
    while (it!=_firingWavb->end()) {
        // push fired neurons to current firing queue
        if (_neurons[*it].FlagTest(NEURON::FLAG_FIRING)) {
            _neurons[*it].FlagReset(NEURON::FLAG_FIRING);
            netPushFiringQueue ((*this), *it, _firingCurr);
            // learned new concept; connect to output if still no link
            ConnectOutput(*it);
        }
        // 1. erase supressed neurons from wave-front queue;
        if (!_neurons[*it].FlagTest(NEURON::FLAG_FIRING) || 
             _neurons[*it].FlagTest(NEURON::FLAG_IGNITING)) {
            it = _firingWavb->erase(it);
        } else {
            it ++;
        }
        // 2. do not fire again if it has fired already.
        // If a neuron has fired and propagated its energy in this round
        // it will have a state FLAG_IGNITING; in such a case, erase the
        // neuron from wave_back queue, so that it does not have another
        // chance to propagate. 
        // (This may not be a good way to break occilation, but for the
        //  timing being we do this as a workaround.)
    }
    // - push valid firing to current queue;
    // Note : if it has propagated its energy to others, i.e.
    // with IGNITING flag, then do not push to firing queue.
    // This is to avoid being connected in the next round
    // unnecessarily.
    // for (it = _firingWavf->begin(); it!=_firingWavf->end(); it++) {
    //     if (!_neurons[*it].FlagTest(NEURON::FLAG_IGNITING)) {
    //     }
    // }

    // switch wave-front and wave-back
    listTmp     = _firingWavf;
    _firingWavf = _firingWavb;
    _firingWavb = listTmp;
    _firingWavb->clear();
    // clear hash table for state reversal
    StateClear();
    return (!_firingWavf->empty());
}

// randomly select 5 neurons to fire
void NET::RandomFire () 
{
    int i;
    _random.clear();
    foreach (i,0,RSIZE) {
        NID id = rand()%(NSIZE-ISIZE) + ISIZE;
        _random.push_back(id);
        netPushFiringQueue ((*this), id, _firingCurr);
        Get(id).Excite(NEURON::EXCITE_BASE);
    }
}

void NET::RealFire (FIRING_TYPE type) 
{
    // connect inputs to fired neurons from the previous round;
    // propagate input stimuli to connected neurons
    ProcessFiring (type);
    // pattern uniquefication
    netProcessUniquePattern ((*this), _bbs);
    // official firing to propagate energy
    ProcessFiring (type, _firingWavb);
    _bbs.Clear();
}

// reduce activity level; reset firing queue
void NET::Cool () 
{
    // TODO: weaken links that are not excited (use DECAY)
    int i;
    list<NID> * listTmp;
    list<NID>::iterator it;
    // cool neurons from baking pan
    it = _firingBake.begin();
    while (it != _firingBake.end()) {
        if (_neurons[*it].State() == NEURON::HYPER) {
            // exlucde re-excited neurons
            it = _firingBake.erase(it);
        } else {
            _neurons[*it].Cool();
            if (_neurons[*it].State() == NEURON::QUIET) {
                it = _firingBake.erase(it);
            } else {
                it++;
            }
        }
    }
    
    // cool neurons in prio and current queues
    CoolPrioQueue ( );
    CoolCurrQueue ( );
    CoolOutput    ( );
    
    // switch curr and prio queue
    listTmp     = _firingPrio;
    _firingPrio = _firingCurr;
    _firingCurr = listTmp;
    _firingCurr->clear();
    
    // cool input/output neurons also
    foreach (i,0,ISIZE) {
        _neurons[i].PotentialReset();
    }
}


// process firing for different queues:
// INPUT   : stimuli from input neurons;
// CURRENT : internal neurons in the current wave;
// DELAYED : fire delayed links from prio-queue;

void NET::ProcessFiring(FIRING_TYPE type, list<NID> *qFiring)
{
    bool bUpdated=false;
    switch (type) {
    case FIRING_INPUT   : {
        int i;
        foreach (i,0,ISIZE) {
            if (Update (Get(_inputs[i]), qFiring)) {
                bUpdated = true;
            }
        }
        // delayed firing from prio queue following input firing
        if (bUpdated) {
            ProcessFiring (FIRING_DELAYED, qFiring);
        }
        break;
    }
    case FIRING_CURRENT : {
        foreachl (it,_firingWavf) {
            if (Update (Get(*it), qFiring)) {
                bUpdated = true;
            }
        }
        // delayed firing from prio queue following internal firing
        if (bUpdated) {
            ProcessFiring (FIRING_DELAYED, qFiring);
        }
        break;
    }
    case FIRING_DELAYED : {
        foreachl (it,_firingPrio) {
            if (Get(*it).FlagTest(NEURON::FLAG_IGNITING_P)) {
                // should not reset this flag immediately;
                // need this for firing multiple rounds.
                // Get(*it).FlagReset(NEURON::FLAG_IGNITING_P);
                Update (Get(*it), qFiring, true/*delayed*/);
            }
        }
    }
    }
}

// cool neurons from prio round; 
// transfer to baking pan
void NET::CoolPrioQueue () 
{
    list<NID>::iterator it;
    for (it=_firingPrio->begin(); it!=_firingPrio->end(); it++) {
        if (Get(*it).FlagTest(NEURON::FLAG_IGNITING_P)) {
            Get(*it).FlagReset(NEURON::FLAG_IGNITING_P);
        }
        // skip cooling if re-excited in the current round;
        if (!_neurons[*it].FlagTest(NEURON::FLAG_FIRING) ) {
            netPushBakingQueue((*this), *it, _firingBake);
        }
    }
}

// cool the current firing neurons with competition
void NET::CoolCurrQueue () 
{
    list<NID>::iterator it;
    // remove neurons that have been filtered out
    it = _firingCurr->begin();
    while (it!=_firingCurr->end()) {
        // change igniting state to igniting_p so that it has a chance
        // to fire in delayed mode in the next round.
        if (_neurons[*it].FlagTest (NEURON::FLAG_IGNITING)) {
            _neurons[*it].FlagReset(NEURON::FLAG_IGNITING);
            _neurons[*it].FlagSet  (NEURON::FLAG_IGNITING_P);
        }
        if (!_neurons[*it].FlagTest(NEURON::FLAG_FIRING)) {
            netPushBakingQueue((*this), *it, _firingBake);
            it = _firingCurr->erase(it);
        } else {
            it ++;
        }
    }
    // for unique stamps, fittest survive.
    // first compare potential; then how to break tie?
    Sort (_firingCurr);
    while (_firingCurr->size() > MAX_FIRE) {
        // push incompetent ones to baking pan after cooling
        Get(_firingCurr->back()).Cool();
        _firingBake.push_back(_firingCurr->back());
        _firingCurr->pop_back();
    }
    // remove firing flag from current round; 
    // reduce potential to avoid dominance;
    // connect to output if it is a new concept
    for (it=_firingCurr->begin(); it!=_firingCurr->end(); it++) {
        _neurons[*it].PotentialReduce();
        _neurons[*it].FlagReset(NEURON::FLAG_FIRING);
    }
}

void NET::CoolOutput () 
{
    int i;
    foreach (i,0,OSIZE) {
        if (Get(_outputs[i]).State() != NEURON::QUIET) {
            netPushBakingQueue((*this), _outputs[i], _firingBake);
        }
    }
}

void NET::Report () 
{
    int i, iFiring[5]={0,0,0,0,0};
    foreach (i,ISIZE,NSIZE) {
        iFiring[_neurons[i].State()] ++;
    }
    foreach (i,NSIZE,TSIZE) {
        if (_neurons[i].State() != NEURON::QUIET) {
            iFiring[4] ++; // output
        }
    }
    if (_verbose >= 2) {
        cout<< " :LEVEL: "; 
        cout.width(6); cout<<GetNumLevels()<<endl;
        cout<< " :AWAKE: "; 
        cout.width(6); cout<<iFiring[1];
        ReportState(NEURON::AWAKE);  cout<<endl;
        cout<< " :MELLOW:"; 
        cout.width(6); cout<<iFiring[2];
        ReportState(NEURON::MELLOW); cout<<endl;
        cout<< " :HYPER: "; 
        cout.width(6); cout<<iFiring[3];
        ReportState(NEURON::HYPER);  cout<<endl;
        cout<< " : OUT : "; 
        cout.width(6); cout<<iFiring[4]<<endl;
        //WriteGif();
    }
    if (_verbose >= 4) {
        WriteDot();
    }
}

// return the longest level of the NET using DFS
int NET::GetNumLevels ()
{
    int i, iMax, iCur;
    // set the traversal ID to the current
    TravIdInc();
    
    // DFS from the INPUT neurons
    iMax = -1;
    foreach (i,0,ISIZE) {
        iCur = netGetNumLevels_rec((*this),Get(_inputs[i]));
        if (iMax < iCur) {
            iMax = iCur;
        }
    }
    return iMax;
}

// levelized the net from INPUT to OUTPUT (level[INPUT]==0);
// the nodes in level[i] are linked and head is stored in listLevel[i]
vector<NID>* NET::Levelize()
{
    //initialize the list of levels
    int i, nLevels;
    vector<NID>* vLevels = 0;
    nLevels = GetNumLevels ();
    if (nLevels > 0) {
        vLevels = new vector<NID>(nLevels+1,(unsigned)(0-1));
        
        // set the traversal ID to the current
        TravIdInc();
        
        // assign all INPUT neuron to longest level
        foreach (i,0,ISIZE) {
            Get(_inputs[i]).Level(nLevels);
        }
        
        // DFS from the INPUT
        foreach (i,0,ISIZE) {
            netLevelize_rec((*this), Get(_inputs[i]), vLevels);
        }
    }
    return vLevels;
}

void NET::ReportState (NEU_STATE st)
{
    int i;
    foreach (i,ISIZE,NSIZE) {
        if (Get(i).State() == st) {
            cout.width(3);cout<<Get(i).Id();
            cout<<"("<<Get(i).Potential()<<") ";
        }
    }
}

// register a neuron state
void NET::StateRegister (NEURON &n)
{
    if (_record.find(n.Id()) == _record.end()) {
        _record[n.Id()] = METASTATE(n);
    }
}

// revert the neuron to a previous state
void NET::StateRevert   (NEURON &n)
{
    if (_record.find(n.Id()) != _record.end()) {
        n.StateReset(_record[n.Id()]);
    }
}

void NET::StateClear   ()
{
    _record.clear();
}

void NET::ConnectOutput(const NID nid)
{
    if (_neurons[nid].LinkCount()==0) {
        _neurons[nid].Link(NextOutput());
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC FUNCTION DEFINITIONS
//____________________________________________________________________


// select highest potential for each unique pattern
void netProcessUniquePattern (
    NET &net, 
    BBS &bbs) 
{
    NID    id;
    STAMP *pst;
    bbs.Select (net.Verbose() >= 2);
    bbs.IterBegin (false); 
    // remove links attached to the losers;
    while (bbs.IterNext(id)) {
        net.StateRevert(net.Get(id));
        if (bbs.GetStamp(id,pst)) {
            netProcessStampLinks(net,(*pst),id,LINK_WEAKEN);
        }
    }
    // update STAMP for all fired neurons; then revert state
    bbs.IterBegin (true); 
    while (bbs.IterNext(id)) {
        if (bbs.GetStamp(id,pst)) {
            net.Get(id).Assign(pst);
            netProcessStampLinks(net,(*pst),id,LINK_DEACTIVE);
        }
        net.StateRevert(net.Get(id));
    }
    // remove losing NID from board completely, so that 
    // the official firing can skip these neurons.
    bbs.Filter();
    // clear the board for next round
    // bbs.Clear();
}

void netPushFiringQueue(NET &net, NID n, list<NID> *qFiring)
{
    NEURON &neu = net.Get(n);
    if (neu.Type()!=OUTPUT && !neu.FlagTest(NEURON::FLAG_FIRING)) {
        qFiring->push_back (n);
        neu.FlagSet(NEURON::FLAG_FIRING);
    }
}

void netPushBakingQueue(NET &net, NID n, list<NID> &qBaking)
{
    net.Get(n).Cool();
    if (net.Get(n).State() > NEURON::QUIET) {
        qBaking.push_back(n);
    }
}

// process neuron links based on the STAMP.
// 1. WEAKEN : in stead of remove link completely, 
//    we want revert to state prio to connection.
// 2. DEACTIVE : reset the active state of synapses

void netProcessStampLinks(
    NET      &net, 
    STAMP    &pst,
    NID       nDst,
    LINK_PROCESS_TYPE type)
{
    NID id;
    vector<NID  >::iterator nit=pst.Nids().begin();
    vector<SYNAP>::iterator sit=pst.Synaps().begin();
    while (nit != pst.Nids().end()) {
        bool f=false;
        if (type==LINK_WEAKEN) {
            f=net.Get(*nit).LinkWeaken(nDst,(*sit).Delayed());
        } else if (type==LINK_DEACTIVE) {
            f=net.Get(*nit).LinkDeactive(nDst,(*sit).Delayed());
        }
        assert (f);
        nit++;
        sit++;
    }
}

static int netGetNumLevels_rec(
    NET &n, 
    NEURON &neu)
{
    // skip already visited (or being processed) node
    if (neu.TravId() == n.TravId()) {
        return neu.Level();
    }
    // initialize to compute correct level in presence of loops
    neu.TravId(n.TravId());
    neu.Level(0);

    int i, iMax, iCur;
    iMax = -1;
    // traverse all links
    map<NID,SYNAP>& mL=neu.Links();
    map<NID,SYNAP>::iterator it;
    foreachv (it, mL) {
        NEURON &neu2 = n.Get((*it).first);
        iCur = netGetNumLevels_rec(n,neu2);
        if (iMax < iCur) {
            iMax = iCur;
        }
    }
    iMax += 1;
    neu.Level(iMax);
    return iMax;
}

static void netLevelize_rec(
    NET &n, 
    NEURON &neu,
    vector<NID> *vLevels)
{
    // skip already visited (or being processed) node
    if (neu.TravId() == n.TravId()) {
        return;
    }
    neu.TravId(n.TravId());
    
    // traverse all links; assuming level is already computed
    map<NID,SYNAP>& mL=neu.Links();
    map<NID,SYNAP>::iterator it;
    foreachv (it, mL) {
        netLevelize_rec(n,n.Get((*it).first),vLevels);
    }
    if (neu.Type()!=INPUT || neu.LinkCount()>0) {
        assert (neu.Level() < vLevels->size());
        neu.Next ((*vLevels)[neu.Level()]);
        (*vLevels)[neu.Level()] = neu.Id();
    }
}

void netReportQueue (
    NET &n, 
    list<NID> * q)
{
    cout << " :QUEUE: ";
    list<NID>::iterator it;
    for (it=q->begin(); it!=q->end(); it++) {
        cout << (*it) << " ";
    }
    cout << endl;
}
