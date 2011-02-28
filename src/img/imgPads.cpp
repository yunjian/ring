// RING : Real Intelligence Neural-net
// 
// Copyright @ Yunjian Jiang (William) 2008
//
// Revision [$Id: imgPads.cpp,v 1.3 2008-06-08 00:05:58 wjiang Exp $]
//


#include "ring.h"

// - use uchar for a pixel.
#include "img/imgRotate.h"


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STATIC DECLARATION
//____________________________________________________________________


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS DECLARATION
//____________________________________________________________________

// RotateByShear: class for image rotation
// Author: Eran Yariv
// http://www.codeproject.com/KB/graphics/rotatebyshear.aspx
//
//      3-shear rotation by a specified angle is equivalent
//      to the sequential transformations
//        x' = x + tan(angle/2) * y      for first x-shear
//        y' = y + sin(angle)   * x      for y-shear
//        x' = x + tan(angle/2) * y      for second x-shear

class RotateByShear : public CRotateByShear<COLORREF>
{
public: 
    RotateByShear (ProgressAnbAbortCallBack callback = NULL) : 
        CRotateByShear<COLORREF> (callback) {}

    virtual ~RotateByShear() {}

    COLORREF GetRGB (COLORREF *pImage,  // Pointer to image
                     SIZE  sImage,      // Size of image
                     UINT  x,           // X coordinate
                     UINT  y            // Y coordinate
                    )
    {
        return pImage [x + y * sImage.cx];
    }

    // Set RGB value at given pixel coordinates
    void SetRGB (COLORREF  *pImage,   // Pointer to image
                 SIZE       sImage,   // Size of image
                 UINT       x,        // X coordinate
                 UINT       y,        // Y coordinate
                 COLORREF   clr       // New color to set
                )
    {
        pImage [x + y * sImage.cx] = clr;
    }

    // Create a new bitmap, given a bitmap dimensions
    COLORREF *CreateNewBitmap (SIZE  sImage) // Size of image
    {
        return new COLORREF [sImage.cx * sImage.cy];
    }

    // Create a new bitmap which is an identical copy of the source bitmap
    COLORREF *CopyBitmap (COLORREF *pImage,     // Pointer to source image
                          SIZE      sImage      // Size of source (and destination) image
                         )
    {
        COLORREF *pDst =  CreateNewBitmap (sImage);
        if (NULL != pDst)
        {
            memcpy (pDst, pImage, sizeof (COLORREF) * sImage.cx * sImage.cy);
        }
        return pDst;
    }          

    // Destroy a bitmap previously created in call to CreateNewBitmap(..)
    // or to CopyBitmap (...)
    void DestroyBitmap (COLORREF *pImage,   // Pointer to image
                        SIZE                // Size of image
                       )
    {
        delete [] pImage;
    }
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS FUNCTIONS - IPAD 
//____________________________________________________________________
IPAD::IPAD (IPAD &p) :
    _width(p.width()),_height(p.height()),_size(_width*_height) 
{
    unsigned i;
    _data = new unsigned char [_size];
    foreach (i,0,_size) {
        _data[i] = p._data[i];
    }
}

// scale up or down a given IPAD to its own size;
// return false if not feasible 
// assumptions : 
//  1. width == height
//  2. integer scaling ratio
bool IPAD::Scale (IPAD &pad)
{
    // assume the ratio is an integer
    int ratio;
    int remainder;
    if (width() != height()) {
        return false;
    }
    if (size() > pad.size()) {
        ratio = (int)ceil((double)width()/(double)pad.width());
    } else {
        ratio = -(int)ceil((double)pad.width()/(double)width());
    }
    // nothing to scale for ratio of 1:1; just copy pixels
    if (ratio == 1 || ratio == -1) {
        memcpy (_data, pad._data, sizeof(char)*size());
        return true;
    }
    if (ratio < 0) { // scale down
        ratio = -ratio;
        for (unsigned j=0; j<height(); ++j) {
            for (unsigned i=0; i<width(); ++i) {
                // consolidate pixels
                // e.g. if ratio = 2, then
                // (0,0) <= (0,0) (0,1) (1,0) (1,1)
                // (1,1) <= (2,2) (2,3) (3,2) (3,3)
                unsigned res=0;
                for (unsigned s=0; s<ratio; ++s) {
                    for (unsigned k=0; k<ratio; ++k) {
                        res += pad.Pix(supx(i,ratio)+s,
                                       supy(j,ratio)+k);
                    }
                }
                res /= ratio*ratio;
                _data[index(i,j)] = (unsigned char)res;
            }
        }
    } else { // scale up
        for (unsigned j=0; j<height(); ++j) {
            for (unsigned i=0; i<width(); ++i) {
                // expand pixels
                // e.g. if ratio = 2, then
                // (0,0) => (0,0) (0,1) (1,0) (1,1)
                // (1,1) => (2,2) (2,3) (3,2) (3,3)
                _data[index(i,j)] = pad.Pix(sdnx(i,ratio),
                                            sdnx(j,ratio));
            }
        }
    }
    return true;
}

// scale up a given IPAD with the same pad size
// assumptions : 
//  1. width == height
//  2. integer scaling ratio
bool IPAD::ScaleUp   (unsigned ratio)
{
}

bool IPAD::ScaleDown (unsigned ratio)
{
    IPAD padTmp(width()/ratio,height()/ratio);
    padTmp.Scale(*this);
    for (unsigned i=0; i<_width; ++i) {
        for (unsigned j=0; j<_height; ++j) {
            int x = xcord(i);
            int y = ycord(j);
            if ((half(padTmp.width())+x) < padTmp.width() &&
                (half(padTmp.height())+y) < padTmp.height() ) {
                _data[index(i,j)] = 
                    padTmp._data[indexcord(x,y,padTmp.width(),padTmp.height())];
            } else {
                _data[index(i,j)] = 0;
            }
        }
    }
}

// assumption for shifting : (0,0) is the upper-left corner

// shift all pixels left for n unit; append 0's
bool IPAD::ShiftLeft (unsigned n)
{
    if (n >= width()) {
        return false;
    }
    unsigned i,j,size=width()-n;
    for (j=0; j<height(); ++j) {
        for (i=0; i<size; ++i) {
            _data[index(i,j)] = _data[index(i+n,j)];
        }
        for (i=size; i<width(); ++i) {
            _data[index(i,j)] = 0;
        }
    }
    return true;
}

// shift all pixels right for n unit; append 0's
bool IPAD::ShiftRight(unsigned n)
{
    if (n >= width()) {
        return false;
    }
    unsigned i,j;
    for (j=0; j<height(); ++j) {
        for (i=width()-1; i>=n; --i) {
            _data[index(i,j)] = _data[index(i-n,j)];
        }
        for (i=0; i<n; ++i) {
            _data[index(i,j)] = 0;
        }
    }
    return true;
}

// shift all pixels up for n unit; append 0's
bool IPAD::ShiftUp(unsigned n)
{
    if (n >= height()) {
        return false;
    }
    unsigned i,j,size=height()-n;
    for (i=0; i<width(); ++i) {
        for (j=0; j<size; ++j) {
            _data[index(i,j)] = _data[index(i,j+n)];
        }
        for (j=size; j<height(); ++j) {
            _data[index(i,j)] = 0;
        }
    }
    return true;
}

// shift all pixels down for n unit; append 0's
bool IPAD::ShiftDown(unsigned n)
{
    if (n >= height()) {
        return false;
    }
    unsigned i,j;
    for (i=0; i<width(); ++i) {
        for (j=height()-1; j>=n; --j) {
            _data[index(i,j)] = _data[index(i,j-n)];
        }
        for (j=0; j<n; ++j) {
            _data[index(i,j)] = 0;
        }
    }
    return true;
}

bool IPAD::Rotate (double dAngle)
{
    RotateByShear rt;
    SIZE   rsrc,rdst;
    rsrc.cx = _width;
    rsrc.cy = _height;
    // generate rotated image (larger than original)
    unsigned char *cdata;
    cdata = rt.AllocAndRotate(_data, rsrc, dAngle, &rdst, 0);
    // crop to original size
    for (unsigned i=0; i<_width; ++i) {
        for (unsigned j=0; j<_height; ++j) {
            _data[index(i,j)] = 
                cdata[indexcord(xcord(i),ycord(j),rdst.cx,rdst.cy)];
        }
    }
    delete [] cdata;
}

void IPAD::Clear ()
{
    memset(_data, 0, sizeof(unsigned char) * _size);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CLASS FUNCTIONS - IMOV 
//____________________________________________________________________

IMOV::IMOV (IPAD &p)
{
    _ipads.push_back(new IPAD(p));
}

IMOV::~IMOV ()
{
    vector<IPAD*>::iterator it;
    foreachv (it,_ipads) {
        delete (*it);
    }
}


bool IMOV::AddIpad(
    IPAD   * p, 
    MTYPE    type, // type of move
    unsigned a)    // quantity of move
{
    bool bOk=true;
    IPAD *pad = new IPAD(*p);
    if (a != 0) {
        switch (type) {
        case ROTATE :
            bOk=pad->Rotate((double)a); break;
        case SHIFTU :
            bOk=pad->ShiftUp   (a); break;
        case SHIFTD :
            bOk=pad->ShiftDown (a); break;
        case SHIFTL :
            bOk=pad->ShiftLeft (a); break;
        case SHIFTR :
            bOk=pad->ShiftRight(a); break;
        case SCALED :
            bOk=pad->ScaleDown (a); break;
        case NONE :
        default: break;
        }
    }
    pad->WriteGif();
    if (bOk) {
        _ipads.push_back(pad);
    } else {
        delete pad;
    }
    return bOk;
}

// expand the ipad into a movie by a sequence of moves
// e.g. shifting, scaling, rotating
bool IMOV::Roll ()
{
    IPAD *pado = _ipads.front();
    _ipads.pop_back();
    // rotate from left to right at 15 degree interval
    AddIpad (pado, ROTATE,  30);
    AddIpad (pado, ROTATE,  15);
    AddIpad (pado, ROTATE,   0);
    AddIpad (pado, ROTATE, 345);
    AddIpad (pado, ROTATE, 330);
    IPAD padn (*pado);
    delete pado;
    // scale down and shift 4 times
    padn.ScaleDown(2);
    AddIpad (&padn, NONE,     0);
    AddIpad (&padn, SHIFTL,   3);
    AddIpad (&padn, SHIFTU,   3);
    AddIpad (&padn, SHIFTR,   3);
    AddIpad (&padn, SHIFTD,   3);
    // rotate from left to right at 15 degree interval
    AddIpad (&padn, ROTATE,  30);
    AddIpad (&padn, ROTATE,  15);
    AddIpad (&padn, ROTATE,   0);
    AddIpad (&padn, ROTATE, 345);
    AddIpad (&padn, ROTATE, 330);
    // bland pad for 5 frames
    padn.Clear();
    for (unsigned i=0; i<5; ++i) {
        AddIpad (&padn, NONE, 0);
    }
}

