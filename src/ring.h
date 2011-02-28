// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: ring.h,v 1.13 2008-06-08 00:05:56 wjiang Exp $]
//

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <map>
#include <ext/hash_map>
#include <assert.h>
using namespace std;
using namespace __gnu_cxx;


#define foreach(i,m,n)  for(i=m; i<n; ++i)
#define foreachv(it,v)  for(it=v.begin(); it!=v.end(); it++)
#define foreachl(it,l)  for(list<NID>::iterator it=l->begin(); it!=l->end(); it++)

//namespace RING
//{


// const char * comparator
struct _char_equal
{
  bool operator()(const char* s1, const char* s2) const
  { return strcmp(s1, s2) == 0; }
};

// PAD is a image pad composed of 9 black/white pixels
typedef char     PAD [9];
// neural network types
typedef unsigned NID;
typedef short    NEU_POTENT;
typedef char     NEU_STATE;
typedef hash_map<const char *, NID, 
                 hash<const char *>, _char_equal> hash_str;
class IPAD;
class IMOV;


// Note: enum consumes 4 Byte by default!
// thus we use bit-encoding and 1 char for the NEU_STATE.
// convert the NEU_TYPE later
//enum NEU_STATE { QUIET=0, AWAKE, MELLOW, HYPER };
enum NEU_TYPE  { INPUT, INTERNAL, OUTPUT };


// TODO:
// - reference counter and link decay
// - serialization of neuron net database to disk
// - temperal association of different outputs (concepts)
// - predicts concepts or learned associations based on output association


// Basic learning principles:
// - opportunity for a neuron to dominate(abstract):
//   1). already established large connectivity
//   2). frequent firing to acquire connectivity
//   3). neurons can form a cluster that always fire together
//
// - opportunity to reconnect:
//   1). synapse weakens over time
//
// - neuron potential/energy
//   1). collect energy from links, based on link strength;
//   2). excite state if energy is larger than threshold;
//   3). available energy to distribute is based on state;
//   4). distribute available energy to all links
//   5). initial energy source : 
//       - inputs firing : strong
//       - random firing : weak
// 
// - competition for new links
//   1). each excited neuron connects to the ones 
//       with highest energy from previous firing;
//   2). then collect/distribute new energy;
//   3). cool down after new connection of the next round
// 
// - how to suppress duplicated firing?
//   If a pattern is already recognized, then do not allow
//   new neuron to repeat the same connectivity. Use a look-up table
//   of firing signatures; if same signature exist, higher energy
//   neuron survive.
//


// SYNAP
// - connects one neurons
// - remembers weights ranges [0,15]
// - weight increase/decrease linearly (TODO: exponentially saturate)
// - remove link when weight reaches 0
// - mark active-flag if newly connected or strengthened (for undo)
// - todo: weight decay every 1000 time instance if no activity
class SYNAP
{
 public:
    static const unsigned DECAY=1000;
    SYNAP ()                      
        : _wt(1),_active(1),_delayed(0),_age(0),_decay(0) {} 

    SYNAP (unsigned w)            
        : _wt(w),_active(1),_delayed(0),_age(0),_decay(0) {}

    SYNAP (bool t) 
        : _wt(1),_active(1),_delayed(t),_age(0),_decay(0) {}

    SYNAP (unsigned w,bool t) 
        : _wt(w),_active(1),_delayed(t),_age(0),_decay(0) {}

    SYNAP (unsigned w,bool t,unsigned d) 
        : _wt(w),_active(1),_delayed(t),_age(d),_decay(0) {}

    SYNAP (const SYNAP& s)
        : _wt(s._wt),_active(s._active),_delayed(s._delayed),
          _age(s._age),_decay(s._decay) {}
    
    const short Weight  () const { return _wt;          }
    const bool  Active  () const { return _active;      }
    const bool  Delayed () const { return _delayed;     }
    const unsigned Age  () const { return _age;         }
    void  Strengthen () { if (_wt < 15) _wt++; _active = 1; }
    void  Weaken     () { if (_wt > 0) _wt--;  _active = 0; }
    void  Deactive   () { _active = 0;             }
    void  Aging      () { if (_age < 1000) _age++; }

 private:
    unsigned _wt     :  8; // initial weight is 1; max : 15
    unsigned _active :  1; // activated in current firing (for undo)
    unsigned _delayed:  1; // delayed propagation
    unsigned _age    : 11; // decay strength every 1000 time instance
    unsigned _decay  : 11; // decay strength every 1000 time instance
};


// SIGN
// - is a signature of NIDs
class SIGN
{
 public:
    SIGN () {}
    void Append     (const NID, const SYNAP);
    bool Empty      ()              { _sig.empty(); }
    void Clear      ()              { _sig.erase(); }
    void operator  =(const SIGN &a) { _sig = a._sig; }
    bool operator ==(const SIGN &a) { return (_sig.compare(a._sig)==0);}
    const char *Cstr()              { return _sig.c_str(); }
 private:
    string   _sig;     // trigering pattern
};


// STAMP 
// - is a signature of firing pattern
// - collects the ID of contributing neurons in a string
// - has iterator for contributing NIDs
class STAMP : public SIGN
{
 public:
    STAMP() : _strength(0) {};
    STAMP(STAMP *s) : _strength(s->_strength),
        _syns(s->_syns),_nids(s->_nids) {};
    // attach a new ID with its strength
    void Append(const NID, const SYNAP);
    // clear all registered patterns (overloaded)
    void Clear ();
    // handling delayed/sequential edges in pattern
    bool Delayed ();
    bool RemoveDelay ();
    
    unsigned        Age     ();
    unsigned        Strength()  { return _strength;  }
    vector<SYNAP> & Synaps  ()  { return _syns;      }
    vector<NID>   & Nids    ()  { return _nids;      }
    
 private:
    unsigned         _strength;// combined synapse strength
    vector<SYNAP>    _syns;    // trigering synaps
    vector<NID>      _nids;    // trigering neurons
};


// BBS
// - bulletin-board-system for unique firing patterns 
// - decides which neuron gets to fire or shut-down
// - operating sequence : Post - Select - Iter - Clear
class NET;
class BBS
{
 public:
    BBS  (NET &n) : _fIterFiring(true),_net(n) {};
    ~BBS ();
    void     Clear   ();
    unsigned Size    () { return _board.size(); }
    // register each axon action
    bool    Post     (NID src, NID dst, SYNAP s);
    // register a combinational pattern
    STAMP * PostComb (NID, STAMP *);
    // pick unique patterns based on synapse strength
    bool    Select   (bool);
    // retrieve the pattern of a firing neuron
    bool    GetStamp (NID, STAMP * &);
    // iterate success firing patterns
    void    IterBegin(bool);
    bool    IterNext (NID &);
    // remove losers from the board.
    void    Filter   ();
    // check existence of ID
    bool    Exists   (NID n) {
        return (_board.find(n) != _board.end());
    }

 private:
    void Release();
    bool Compare(NID,NID,STAMP*,STAMP*);
    bool _fIterFiring;   // 1:select winners; 0:losers
    // maps unique signature string to NID
    hash_str                         _stamps;
    hash_map<NID, STAMP *>           _board;
    hash_map<NID, STAMP *>::iterator _it;
    // need a NET reference to retrieve neurons
    NET & _net;
};


// DFSNODE - graph virtex for DFS traversal
class DFSNODE
{
 public:
    DFSNODE() : _tid(0),_lev(0),_next((unsigned)(0-1)) {}
    unsigned TravId()           { return _tid;  }
    void     TravId(unsigned t) { _tid=t;       }
    unsigned Level()            { return _lev;  }
    void     Level(unsigned l)  { _lev=l;       }
    NID      Next()             { return _next; }
    void     Next(NID n)        { _next=n;      }
 private:
    unsigned           _tid;  // traversal id
    unsigned           _lev;  // DFS level
    NID                _next; // linked list
};

// DFSGRAPH - graph for DFS traversal
class DFSGRAPH
{
 public:
    DFSGRAPH() : _tid(0) {}
    unsigned TravId()           { return _tid;  }
    void     TravIdInc()       { _tid++;       }
 private:
    unsigned           _tid;  // traversal id
};


// forward declaration
class METASTATE;


// NEURON 
// - receives inputs and fires if potential is larger than threshold;
// - has a unique firing pattern;
// - is excited only when the signature firing pattern and no else;
// - seeks connection with others that fire at the same time instance;
// - after firing, distribute energy among connected synapses;
// - activity quiet out in three time instances;
class NEURON : public DFSNODE
{
    friend class METASTATE;
 public:
    NEURON  () : _id(0),_type(INTERNAL),
        _state(QUIET),_potent(0),_flag(FLAG_NONE) {}

    static const char  QUIET    =0;
    static const char  AWAKE    =1;
    static const char  MELLOW   =2;
    static const char  HYPER    =3;
    static const short MAX_SYNAP=1000;      // maximum synapse per neuron
    static const short MIN_SYNAP=5;         // minimum synapse for propagation
    static const NID NONE=(unsigned)(0-1);  // maximum possible id
    static const NEU_POTENT THRESH_BASE =5; // threshold for hyper state
    static const NEU_POTENT EXCITE_BASE =6; // random excitation
    static const NEU_POTENT THRESH_HIGH =7; // threshold for propagating energy
    static const NEU_POTENT EXCITE_HIGH =10;// input excitation

    // mutual exclusive flags
    enum FLAG {
        FLAG_NONE=0,
        FLAG_FIRING=1,     // being ignited
        FLAG_IGNITING=2,   // igniting others
        FLAG_IGNITING_P=4  // igniting in prio round
    };

    // access functions
    NID  Id()               { return _id;   }
    void Id(NID i)          { _id = i;      }
    NEU_TYPE Type()         { return _type; }
    void Type(NEU_TYPE t)   { _type = t;    }
    NEU_STATE State()       { return _state; }
    void State(NEU_STATE s) { _state = s;    }
    void StateReset(METASTATE s);
    
    // potential management
    void PotentialAdd(short w=1) 
        { _potent+=w; if (_potent>255) _potent=255; }
    void PotentialReset();
    void PotentialReduce();
    NEU_POTENT Potential()  { return _potent; }
    
    // link management
    map<NID,SYNAP>& Links() { return _links; }
    unsigned LinkCount()    { return _links.size(); }
    bool Linked    (NID id) { return _links.find(id) != _links.end(); }
    bool LinkRemove(NID);
    bool LinkWeaken  (NID, bool d=false);
    bool LinkDeactive(NID, bool d=false);
    void Link        (NID, bool d=false);
    
    // 8 1-bit flags
    void FlagSet  (FLAG f) { _flag |= (char)(f); }
    void FlagReset(FLAG f) { _flag &= ~((char)f); }
    bool FlagTest (FLAG f) { return _flag & ((char)f); }
    
    // firing & cooling
    bool Excite(NEU_POTENT p);
    void Cool  ();
    
    // check if given STAMP matches with internal SIGN
    bool Match (STAMP *pStamp) { 
        return (_sign.Empty() || (_sign==(*pStamp)) 
                || (LinkCount()==0) ); 
    }
    // assign a unique pattern
    void Assign(STAMP *pStamp) 
        { _sign=(*pStamp); }
    SIGN & Sign () { return _sign; }
    
 private:
    NID                _id;
    NEU_TYPE           _type;
    char               _flag;
    NEU_STATE          _state;
    NEU_POTENT         _potent;
    SIGN               _sign;
    map<NID,SYNAP>     _links;
};


// METASTATE
// record the state of a neuron
class METASTATE
{
    friend class NEURON;
 public:
    METASTATE() { u._data = 0; }
    METASTATE(const NEURON &n) {
        u.s._flag=n._flag; u.s._state=n._state; u.s._potent=n._potent; 
    }
    METASTATE(const METASTATE &m) { u._data = m.u._data; }
    void operator =(const METASTATE m) { u._data=m.u._data;  }
    //char Flag() { return u.s._flag; }
 private:
    typedef struct {
        char          _flag;
        NEU_STATE     _state;
        NEU_POTENT    _potent;
    } state;
    // use union and direct unsigned-copy to speed up copy construct
    union {
        unsigned      _data;
        state          s;
    } u;
};



// NET 
// - is a collection of NEURON, which:
// - (1) a subset are designated to receive input 
// - (2) are sparsely connected 
// - (3) a randomly selected 0.1% fire at each time instance
// - (4) outputs connects to learned concepts in each firing;
//       can be associated (fed back) to other concepts for learning
// 
// - keeps track of time & monitors firing at each time instance
//
// Let's start with processing of 9 image pixels.
class NET : public DFSGRAPH
{
 public:
    NET  (NID);
    ~NET ();
    const NID RSIZE; //=1;
    const NID ISIZE; //=9;
    const NID OSIZE; //=200;
    const NID NSIZE; //=1000;        // input+internal
    const NID TSIZE; //=OSIZE+NSIZE; // total size
    
    // How many neurons survive each round ?  This is a critical
    // parameter to control learning. With MAX_FIRE=1, the information
    // is condensed; with larger numbers, information spreads, more
    // connections happen, but also more redundant connections.
    static const short MAX_FIRE=1000;

    unsigned Verbose     ()   { return _verbose; }
    void     Advance     ()   { _time ++; }
    void     Input       (PAD);
    void     Input       (IPAD &);
    void     Train       (IMOV &);
    void     Update      ();
    void     Report      ();
    void     ReportState (NEU_STATE st);
    void     Cool        ();
    void     WriteGif    ();
    void     WriteDot    ();
    bool     IsInput     (NID id) { return (id>=0 && id<ISIZE); }
    NEURON & Get(NID id) {
        if (id>=0&&id<TSIZE) return _neurons[id];
        cout<<id<<endl; assert(0); return _neurons[0];
    }
    int          GetNumLevels();
    vector<NID>* Levelize();
    
    // register a neuron state and revert it;
    // (could be done inside a neuron, but do it here
    //  for memory concern)
    void     StateRegister (NEURON &n);
    void     StateRevert   (NEURON &n);
    void     StateClear    ();
    
    // compare the potential of two neurons
    bool     Compare       (NID n1, NID n2) {
        return (Get(n1).Potential() > Get(n2).Potential());
    }

 protected:
    enum FIRING_TYPE { 
        FIRING_INPUT, FIRING_CURRENT, FIRING_DELAYED 
    };
    bool       Update         (NEURON &n,list<NID> *q=0,bool d=0);
    bool       Propagate      (NEURON &n,list<NID> *q,bool d=0);
    void       Connect        (NEURON &n,list<NID> *q,bool d=0);
    void       ConnectOutput  (const NID);
    void       RandomFire     ();
    void       RealFire       (FIRING_TYPE);
    void       Sort           (list<NID> *);
    void       CoolCurrQueue  ();
    void       CoolPrioQueue  ();
    void       CoolOutput     ();
    void       ProcessFiring  (FIRING_TYPE type, list<NID> *qFiring=0);
    bool       ProcessFiringQueue();
    NID        NextOutput     () 
        { _nextOutput++; return (NSIZE+_nextOutput-1); }

 private:
    NEURON    * _neurons;  //[TSIZE];
    NID       * _inputs ;  //[ISIZE];
    NID       * _outputs;  //[OSIZE];
    NID         _nextOutput;
    
    list<NID>  _random;     // 0.5% random firing
    list<NID>* _firingPrio; // neurons that fire in prio round
    list<NID>* _firingCurr; // neurons that fire currently
    list<NID>* _firingWavf; // neurons in the firing wave front
    list<NID>* _firingWavb; // neurons in the firing wave back
    list<NID>  _firingBake; // remaining from >2 rounds before
    BBS        _bbs;        // bulletin board of firing pattern
    hash_map<NID,METASTATE> 
        _record;            // register neuron states

    long       _time;
    unsigned   _verbose;
};


class WORK 
{
 public:
    WORK () :_mnist(false) {}
    
    void GenTrainingSet ();
    void TrainPad   (NET &,const char *);
    void TrainMnist (NET &,const char *);
    
    void mnist(bool m)   { _mnist = true; }
    bool mnist()         { return _mnist; }
    void verbose(bool m) { _verb  = true; }
    bool verbose()       { return _verb;  }

 protected:
    void ProcessMnist (NET &,unsigned char *memblock);

 private:
    bool  _mnist;
    bool  _verb;
};


// PADS 
// is a collection of commonly seen 9-pixel patterns
namespace PADS 
{
    static const PAD blank = {0,0,0,0,0,0,0,0,0};
    static const PAD  ring = {1,1,1,1,0,1,1,1,1};
    static const PAD   dot = {0,0,0,0,1,0,0,0,0};
    static const PAD  row1 = {1,1,1,0,0,0,0,0,0};
    static const PAD  row2 = {0,0,0,1,1,1,0,0,0};
    static const PAD  row3 = {0,0,0,0,0,0,1,1,1};
    static const PAD  col1 = {1,0,0,1,0,0,1,0,0};
    static const PAD  col2 = {0,1,0,0,1,0,0,1,0};
    static const PAD  col3 = {0,0,1,0,0,1,0,0,1};
    static const PAD   upl = {1,0,0,0,0,0,0,0,0};
    static const PAD   upr = {0,0,1,0,0,0,0,0,0};
    static const PAD  lowl = {0,0,0,0,0,0,1,0,0};
    static const PAD  lowr = {0,0,0,0,0,0,0,0,1};
    static const PAD diag1 = {1,0,0,0,1,0,0,0,1};
    static const PAD diag2 = {0,0,1,0,1,0,1,0,0};
    static const PAD cross = {1,0,1,0,1,0,1,0,1};
    static void Write (const PAD p, std::ostream &out) { 
        int i;
        foreach (i,0,9) { out << int(p[i]) << " "; }
        out << std::endl;
    }
    static void Read (PAD p, char *line) {
        int i=0;
        char *pch = strtok (line," ");
        while (pch != NULL) {
            p[i++] = atoi(pch);
            pch = strtok (NULL, " ");
        }
    }
}


// IPAD is an image pad 
// composed of 28 X 28 (784) grey scale pixels [0,255]
class IPAD
{
 public:
    IPAD  (unsigned w, unsigned h) :
        _width(w),_height(h),_size(w*h) {
        _data = new unsigned char [_size];
    }
    IPAD  (IPAD &p);
    ~IPAD () { delete [] _data; }
    // pad size is constant once constructed
    const unsigned width () { return _width;  }
    const unsigned height() { return _height; }
    const unsigned size  () { return _size;   }
    // retrieve pixels
    unsigned char Pix(unsigned x, unsigned y) {
        if (index(x,y) < _size) {
            return _data[index(x,y)];
        } else { 
            return 0;
        }
    }
    unsigned char * &Data() { return _data; }
    // scale up or down the image pad
    bool Scale     (IPAD &);
    bool ScaleUp   (unsigned);
    bool ScaleDown (unsigned);
    // in-place shifting
    bool ShiftLeft (unsigned);
    bool ShiftRight(unsigned);
    bool ShiftUp   (unsigned);
    bool ShiftDown (unsigned);
    // in-place rotation (counter clockwise)
    bool Rotate (double);
    void Clear  ();
    // write out image in GIF format
    void WriteGif ();

 protected:
    // for pixel access and image scaling
    unsigned index(unsigned i, unsigned j) {
        return (j*_width+i);
    }
    // scaling up
    unsigned supx(unsigned i, unsigned ratio) {
        return (i*ratio);
    }
    unsigned supy(unsigned j, unsigned ratio) {
        return (j*ratio);
    }
    // scaling down
    unsigned sdnx(unsigned i, unsigned ratio) {
        return (i/ratio);
    }
    unsigned sdny(unsigned j, unsigned ratio) {
        return (j/ratio);
    }
    unsigned half(unsigned w) {
        return ((w%2)?(w/2+1):(w/2));
    }
    // croping based on center
    int xcord(unsigned i) {
        return (i - half(_width));
    }
    int ycord(unsigned j) {
        return (j - half(_height));
    }
    unsigned indexcord(int i, int j, unsigned w, unsigned h) {
        return (half(w)+i) + (half(h)+j) * w;
    }
 private:
    const unsigned  _width ;
    const unsigned  _height;
    const unsigned  _size  ;
    unsigned char * _data  ;
};

// IMOV is a collection of image pad frames, as a movie
// composed of 28 X 28 (784) grey scale pixels [0,255]
// TODO: improve memory efficiency with memory pool
class IMOV
{
 public:
    IMOV  (IPAD &p);
    ~IMOV ();
    // moves for making a movie
    enum MTYPE { 
        ROTATE, SCALED, SHIFTU, SHIFTD, SHIFTL, SHIFTR,
        NONE,
    };
    vector<IPAD*> & ipads() { return _ipads; }
    // append a frame at the end
    bool AddIpad (IPAD *,MTYPE,unsigned);
    // expand the ipad into a movie by a sequence of moves
    // e.g. shifting, scaling, rotating
    bool Roll ();

 private:
    vector<IPAD*> _ipads;
};


// POOL
// - a dynamic memory manager for NEURONs (TO ADD LATER)
// class POOL
// {
//  public:
//     POOL  (int size);
//     ~POOL ();
//     int     Size();
//     int     Add (NEURON *&); // add new neuron, return its id and pointer
//     NEURON *Get (int);       // access a neuron from its id 
//     void    Delete(NEURON*);   // put a neuron to recycle bin
// 
//  private: 
//     // memory pool is divided into pages, each of 64K size;
//     // new page will be allocated when current pages run out.
//     static const int iPageSize = 65536;
//     static const int iPageMask = 65535;
//     static const int iPageShif = 16;
//     int      nUsed;   // total used neurons including recycled
//     int      nPage;   // total pages allocated so far
//     int      nBook;   // total storage for page pointers
//     NEURON  *pDead;   // the head of all recycled nodes
//     NEURON **sNodes;  // pointer to the head of each page
// };
