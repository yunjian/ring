// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: ring.cpp,v 1.10 2008-06-08 00:05:56 wjiang Exp $]
//

#include "ring.h"


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC FUNCTIONS DEFINED IN THIS FILE
//____________________________________________________________________
static void     sPrintDebug ();
static unsigned sGetInteger (unsigned char *memblock);
static void     sReadPad    (unsigned char *m, IPAD &dn);

// size of input image pad width, 
// the square of which is the size of input neurons
#define IPAD_SIZE 28

int main (int argc, char **argv)
{
    const char *chUsage="ring [-g][-n][-v] training_input.dat\n"
        "\t-g generating a sample training data file\n"
        "\t-n reading data file from MNIST benchmark suite\n"
        "\t-v turn on verbose mode\n";
    if (argc <=1) {
        cerr << chUsage;
        return 0;
    }
    WORK iwork;
    
    int  iArg=0;
    char *chFileName;
    while (++iArg < argc) {
        // optionally generate training data
        if (strcmp(argv[iArg], "-g")==0) {
            iwork.GenTrainingSet();
        } else if (strcmp(argv[iArg], "-n")==0) {
            iwork.mnist(true);
        } else if (strcmp(argv[iArg], "-v")==0) {
            iwork.verbose(true);
        } else {
            chFileName = argv[iArg];
        }
    }
    // to help debugging
    if (iwork.verbose()) {
        //sPrintDebug();
    }
    if (iwork.mnist()) {
        NET inet(IPAD_SIZE*IPAD_SIZE);
        iwork.TrainMnist(inet,chFileName);
    } else {
        NET inet(9);
        iwork.TrainPad  (inet,chFileName);
    }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// EXPERIMENT SETUP
//____________________________________________________________________
void WORK::GenTrainingSet() 
{
    fstream dataFile;
    dataFile.open("test.dat",fstream::out);
    if (!dataFile.is_open()) {
        cerr << "File test.dat cannot be opened!" << endl;
        return;
    }
    int i;
    // a few blank pads followed by migrating rows
    foreach (i,0,5) { PADS::Write(PADS::blank,dataFile); }
    foreach (i,0,5) { PADS::Write(PADS:: row1,dataFile); }
    foreach (i,0,5) { PADS::Write(PADS:: row2,dataFile); }
    foreach (i,0,5) { PADS::Write(PADS:: row3,dataFile); }
    foreach (i,0,5) { PADS::Write(PADS::blank,dataFile); }
    dataFile.close();
}


void WORK::TrainPad(
    NET & net, 
    const char *chFileName)
{
    char line[25];
    fstream dataFile;
    dataFile.open(chFileName,fstream::in);
    if (!dataFile.is_open()) {
        cerr << "ERROR: file "<<chFileName<<" cannot be opened!" << endl;
        return;
    }
    PAD p;
    dataFile.getline(line,20);
    while (dataFile.gcount() > 0) {
        PADS::Read(p, line);
        //PADS::Write(p, std::cout);
        net.Input(p);
        net.Update();
        dataFile.getline(line,20);
    }
    dataFile.close();
}

void WORK::TrainMnist(
    NET & net, 
    const char *chFileName) 
{
    ifstream datafile (chFileName, ios::in|ios::binary|ios::ate);
    if (!datafile.is_open()) {
        cerr << "ERROR: file "<<chFileName<<" cannot be opened!" << endl;
        return;
    }
    // read the entire file together
    unsigned char * memblock;
    ifstream::pos_type size;
    size = datafile.tellg();
    memblock = new unsigned char [size];
    datafile.seekg (0, ios::beg);
    datafile.read ((char *)memblock, size);
    datafile.close();
    if (_verb) {
        cout << "the complete MNIST file content is in memory" <<endl;
    }
    // process the image blocks
    ProcessMnist (net,memblock);
    delete[] memblock;
}

void WORK::ProcessMnist (
    NET & net, 
    unsigned char *memblock)
{
    unsigned iMagic, iMagCount, iRows, iCols, iPos;
    iMagic    = sGetInteger (memblock);
    iMagCount = sGetInteger (memblock+4);
    iRows     = sGetInteger (memblock+8);
    iCols     = sGetInteger (memblock+12);
    if (_verb) {
        cout << "Magic :" << iMagic << endl;
        cout << "count :" << iMagCount << endl;
        cout << "rows  :" << iRows << endl;
        cout << "cols  :" << iCols << endl;
    }
    iPos = 16;
    // pick a small set for initial experiments 
    iMagCount = 5;
    for (unsigned i=0; i<iMagCount; ++i) {
        IPAD datapad(28,28);
        sReadPad (memblock+iPos, datapad);
        IPAD neupad (IPAD_SIZE,IPAD_SIZE);
        neupad.Scale(datapad);
        // 1. animation from single pad for training
        IMOV movie(neupad);
        movie.Roll();
        net.Train (movie);
        // 2.simply update neural-net with image pad
        // net.Input (neupad);
        // net.Update();
        iPos += datapad.size();
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC FUNCTION DEFINITIONS
//____________________________________________________________________

static void sPrintDebug () 
{
    cout << "char size: " << sizeof(char) << endl;
    cout << "unsigned size: " << sizeof(unsigned) << endl;
    cout << "SYNAP size: " << sizeof(SYNAP) << endl;
    cout << "NEU_POTENT size: " << sizeof(NEU_POTENT) << endl;
    cout << "NEU_STATE size: " << sizeof(NEU_STATE) << endl;
    cout << "NEURON size: " << sizeof(NEURON) << endl;
    cout << "METASTATE size: " << sizeof(METASTATE) << endl;
}

// convert an array of 4 bytes into 32-bit integer using big-endian
//     addr+0 : b1
//     addr+1 : b2
//     addr+2 : b3
//     addr+3 : b4
// integer = b1*h1000 + b2*h0100 + b3*h0010 + b4
static unsigned sGetInteger (unsigned char *memblock)
{
    unsigned result=0;
    for (unsigned i=0; i<4; ++i) {
        result = memblock[i] + (result << 8);
    }
    return result;
}

static void sReadPad (unsigned char *m, IPAD &dn)
{
    for (int i=0; i<dn.size(); ++i) {
        (dn.Data())[i] = m[i];
    }
}


// TESTING LOG
// ---------------------------------------------------------------
// 
// 
// Sun Mar 23 19:08:21 PDT 2008
// ---------------------------------------------------------------
// Q1: Energy distribution between learned and new links for input
// 
// 8 = 2 (25%, new) + 6 (75%, learned)
// 0-1-2 pattern : 3 (random fire) + 2 + 2 + 2 = 9
// 0 pattern : 0 + 6 = 6
// 1 pattern : 0 + 6 = 6
// 2 pattern : 0 + 6 = 6
// 
// 1. if increase input energy from 8 to 20, and 
// 2. train 5 times
// 
// 18 = 3 (1/6) + 15 (5/6)
// 0-1-2 pattern : 3 (random fire) + 3 + 3 + 3 = 12
// 0 pattern : 0 + 15 = 15
// 
// Tue Mar 25 22:13:49 PDT 2008
// ---------------------------------------------------------------
// A1: Verified with test/4.dot, that above solves the problem
// of not sufficient energy distribution for new patterns.
// However, new problem is redundant connection for existing
// learned patters.
//
// Sat Mar 29 10:40:56 PDT 2008
// ---------------------------------------------------------------
// Q2: Tolerate the redundant links; 
// A2: prevent learned links being destroyed.
//     0 -> 853 -> 824
//     0        -> 824
//     1 -> 814 -> 824
//     1        -> 824
//     2 -> 795 -> 824
//     2        -> 824
// When '0' (in addition with '36') fires, '853' and '824' 
// compete; Do not remove any learned links.
// 
// Sat Mar 29 12:34:09 PDT 2008
// ---------------------------------------------------------------
// Q3: How to learn parallel, overlapping patterns without
// inferring hierarchy among them?
//    /0 -> 853 -> 824
//   | 1 -> 814 -> 824
//    \2 -> 795 -> 824
//    /3 -> 243 -> 919
//   | 4 -> 835 -> 919
//    \5 -> 400 -> 919
//    /0 -> 853 -> 824 -> 652  : extra layer
//   | 3 -> 243 -> 919 -> 652  : extra layer
//    \6 -> 777 -> 888 -> 652  : extra layer
// A3: Learned pattern should not fire if any input link missing.
// Each neuron owns a STAMP that stores its firing signature;
// which is then updated whenever it is triggered to fire.
// 
// Sat Mar 29 21:40:53 PDT 2008
// ---------------------------------------------------------------
// Q4: Insufficient energy distribution for new links
// A4: reverse changes made in A1.
//   10 = 2 (1/5) + 8 (4/5)
//   0-1-2 pattern : 3 (random fire) + 2 + 2 + 2 = 9
//   0 pattern : 0 + 8 = 8
// 
// 
// Q: Each neuron will have only one chance to learn!?
// 
// Sun Apr 20 21:53:54 PDT 2008
// ---------------------------------------------------------------
// delayed edge should not be propagated for regular firing. todo
// 
// 
// Sat Apr 26 11:15:39 PDT 2008
// ---------------------------------------------------------------
// For new fired neuron with no existing signs, remove delayed link
// for combinational pattern testing first. Should recompute the 
// synapse strength also!
// 
// 1st:
// 345             -> 878
// 345796D -> 345  -> 604
// 
// 2nd:
// 878             -> 1001 (output)
// 878796D -> 878  -> 604  (should win over output firing!)
// 878796D         -> 604
// 
// Sat Apr 26 21:56:54 PDT 2008
// ---------------------------------------------------------------
// :STAMP: 1 (MISS) 147 7 (MISS) 4 (MISS)
//  => 796 was missed here, since it has signature "012"
// :STAMP: 637 (MISS) 637 414 637 (MISS)
// :QUEUE: 553 414 796 878 637
//  => yet it's excited by input "1"'s energy along
//  => should disallow energy propagation from "1" to "796"
// 
// Sat Jun  7 15:06:21 PDT 2008
// ---------------------------------------------------------------
// ::using scaled/rotated image-pad for training
// ISSUE: 
//   Since the entire IPAD pattern is used as signature, rather than
//   small features, the learning is too specific and not general
//   enough, i.e. new patterns does not trigger previously seen
//   patterns at all.
// SOLUTION: 
//   Need an iterative approach for the matching process to 
//   converge: 
//   do {
//      bottem-up pattern abstraction/inferencing;
//      top-down prediction-morphing;  (HOW?)
//      matching at lower level;
//   } while (notMatch && notGiveUp);
// 

