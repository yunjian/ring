#ifndef GIFSAVE_H
#define GIFSAVE_H


#ifndef EXTERN
#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif
#endif

enum GIF_Code {
    GIF_OK,
    GIF_ERRCREATE,
    GIF_ERRWRITE,
    GIF_OUTMEM
};


EXTERN int  GIF_Create(
         const char *filename,
         int width, int height,
         int numcolors, int colorres
     );

EXTERN void GIF_SetColor(
         int colornum,
         int red, int green, int blue
     );

EXTERN int  GIF_CompressImage(
         int left, int top,
         int width, int height,
         int (*getpixel)(int x, int y)
     );

EXTERN int  GIF_Close(void);



#endif
