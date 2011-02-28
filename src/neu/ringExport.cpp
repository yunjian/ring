// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// FILE : ringExport.cpp
// 
// DESCRIPTION :
//    Export the neuron network in various formats.
//    Currently supported : GIF, DOT.


#include <stdio.h>
#include <string.h>
#include "ring.h"
#include "gif/gifsave.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Interface with GIFSAVE package
//____________________________________________________________________
static NET  *GifNet;
static IPAD *GifPad;
static int GifGetPixelNet(int x, int y);
static int GifGetPixelPad(int x, int y);
static const int iGifPix=3,
    iGifWidth  = iGifPix * 40,
    iGifHeight = iGifPix * 25,
    iGifColors = 4,
    iGifDepth  = 2;

void NET::WriteGif()
{
    static unsigned iGifIndex = 0;
    char str[8];
    string sName ("gif/net");
    sprintf(str,"%d",iGifIndex++);
    sName += str;
    sName += ".gif";
    GifNet = this;

    GIF_Create(sName.c_str(), iGifWidth, iGifHeight,
               iGifColors, iGifDepth);

    // RED, GREEN, BLUE
    GIF_SetColor(0, 3, 3, 3);
    GIF_SetColor(1, 2, 0, 2);
    GIF_SetColor(2, 2, 2, 0);
    GIF_SetColor(3, 3, 0, 0);

    GIF_CompressImage(0, 0, -1, -1, GifGetPixelNet);

    GIF_Close();
}

int GifGetPixelNet(int x, int y)
{
    unsigned idx = (y/iGifPix)*(iGifWidth/iGifPix) + 
        (x/iGifPix);
    int s = GifNet->Get(idx).State();
    return s;
}

int GifGetPixelPad(int x, int y)
{
    return GifPad->Pix(x,y);
}

void IPAD::WriteGif()
{
    static unsigned iGifIndex = 0;
    char str[8];
    string sName ("gif/in");
    sprintf(str,"%d",iGifIndex++);
    sName += str;
    sName += ".gif";
    GifPad = this;

    GIF_Create(sName.c_str(), width(), height(),
               256/*num colors*/, 8/*color resolution*/);

    // setup grey scale for each color
    for (int i=0; i<256; ++i) {
        // RED, GREEN, BLUE
        GIF_SetColor(i, i, i, i);
    }

    GIF_CompressImage(0, 0, -1, -1, GifGetPixelPad);

    GIF_Close();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Writing ATT DOT format
//____________________________________________________________________

void NET::WriteDot ()
{
    static unsigned iGifIndex = 0;
    vector<NID> *vLevels = Levelize();
    if (!vLevels) {
        return;
    }
    char str[10];
    string sName ("dot/net");
    if (iGifIndex < 10) {
        sprintf(str,"000%d",iGifIndex++);
    } else if (iGifIndex < 100) {
        sprintf(str,"00%d", iGifIndex++);
    } else if (iGifIndex < 1000) {
        sprintf(str,"0%d",  iGifIndex++);
    } else {
        sprintf(str,"%d",  iGifIndex++);
    }
    sName += str;
    sName += ".dot";
    
    ofstream pFile (sName.c_str());
    if ( !pFile ) {
        cerr << "Cannot open file " << sName << endl;
        return;
    }
    
    int nLevels = vLevels->size();
    
    // write the DOT header
    pFile << "# RING 1.0" << endl << endl;
    pFile << "digraph RING {" << endl;
    pFile << "size = \"7.5,10\";" << endl;
    pFile << "center = true;" << endl << endl;
    
    // labels on the left of the picture
    pFile << "{" << endl;
    pFile << "  node [shape = plaintext];" << endl;
    pFile << "  edge [style = invis];" << endl;
    pFile << "  LevelTitle1 [label=\"\"];" << endl;
    pFile << "  LevelTitle2 [label=\"\"];" << endl;
    
    // generate node names with labels for each level
    int Level;
    for ( Level = 0; Level <= nLevels; Level++ ) {
        // the visible node name
        pFile << "  Level" << Level << endl;
        pFile << " [label = ";
        pFile << "\"";
        pFile << "\"";
        pFile << "];\n";
    }
    
    pFile << "  LevelTitle1 ->  LevelTitle2 ->" ;
    // genetate the sequence of visible/invisible nodes to mark levels
    for ( Level = 0; Level <= nLevels; Level++ ) {
        // the visible node name
        pFile << "  Level" <<  Level;
        // the connector
        if ( Level != nLevels ) {
            pFile << " ->";
        } else {
            pFile << ";";
        }
    }
    pFile << endl << "}" << endl << endl;
    
    // generate title box on top
    pFile  << "{" << endl;
    pFile  << "  rank = same;" << endl;
    pFile  << "  LevelTitle1;" << endl;
    pFile  << "  title1 [shape=plaintext," << endl;
    pFile  << "          fontsize=20," << endl;
    pFile  << "          fontname = \"Times-Roman\"," << endl;
    pFile  << "          label=\"";
    pFile  << "RING 1.0";
    pFile  << "\"" << endl;
    pFile  << "         ];" << endl;
    pFile  << "}" << endl << endl;
    
    // generate statistics box
    pFile  << "{" << endl;
    pFile  << "  rank = same;" << endl;
    pFile  << "  LevelTitle2;" << endl;
    pFile  << "  title2 [shape=plaintext," << endl;
    pFile  << "          fontsize=18," << endl;
    pFile  << "          fontname = \"Times-Roman\"," << endl;
    pFile  << "          label=\"";
    pFile  << nLevels << " L";
    pFile << "\"" << endl;
    pFile << "         ];" << endl;
    pFile << "}" << endl << endl;
    
    // generate nodes of each rank
    // fill in color if the value is set
    int iN;
    for ( Level = 0; Level < nLevels; Level++ ) {
        pFile << "{" << endl;
        pFile << "  rank = same;" << endl;
        pFile << "  Level" << Level << ";" << endl;
        for ( iN = vLevels->at(nLevels-Level-1);
              iN != NEURON::NONE; iN = Get(iN).Next()) {
            pFile << "  Node" << iN;
            pFile << "[label = \"" << iN << "\"";
            if ( IsInput(iN) ) {
                pFile << ", shape = triangle";
            } else {
                pFile << ", shape = ellipse";
            }
            // fill in color based on its value
            switch (Get(iN).State()) {
            case NEURON::QUIET:
                pFile << ", color=grey, style=filled";
                break;
            case NEURON::AWAKE:
                pFile << ", color=green, style=filled";
                break;
            case NEURON::MELLOW:
                pFile << ", color=yellow, style=filled";
                break;
            case NEURON::HYPER: 
                pFile << ", color=red, style=filled";
                break;
            default:
                break;
            }
            pFile << "];" << endl;
        }
        pFile << "}" << endl << endl;
    }
    
    // generate invisible edges from the square down
    pFile << "title1 -> title2 [style = invis];" << endl;
    // create edges to the PO nodes
    // iN = vLevels->at(0);
    // while (iN != NEURON::NONE) {
    //     pFile << "title2 -> Node" << iN;
    //     pFile << " [style = " << "bold";
    //     pFile << ", color = coral];" << endl;
    //     iN = Get(iN).Next();
    // }
    
	// generate edges
    for ( Level = 0; Level < nLevels; Level++ ) {
        for ( iN = vLevels->at(nLevels-Level-1);
              iN != NEURON::NONE; iN = Get(iN).Next()) {
            
            map<NID,SYNAP>& mL=Get(iN).Links();
            map<NID,SYNAP>::iterator it;
            foreachv (it, mL) {
                NID iN2 = (*it).first;
                // generate the edge from inputs to this node
                pFile << "Node" << iN;
                pFile << " -> ";
                pFile << "Node" << iN2;
                pFile << " [style = ";
                pFile << (((*it).second.Weight()>1) ? "bold" : "dashed");
                pFile << ", color = ";
                pFile << (((*it).second.Delayed()) ? "blue" : "grey");
                pFile << "];" << endl;
            }
        }
    }
	pFile << "}" << endl << endl;
    pFile.close();
    
    delete vLevels;
}
